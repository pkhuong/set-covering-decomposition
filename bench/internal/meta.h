#ifndef BENCH_META_H
#define BENCH_META_H
#include <cstring>
#include <functional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/macros.h"
#include "absl/meta/type_traits.h"
#include "absl/types/span.h"
#include "absl/utility/utility.h"

// Misc C++ metaprogramming noise for compare-functions.h
namespace bench {
namespace internal {
// The ExplicitFunction class implements the same logic as a
// std::function<Result(const Arg*)>, except that it relies on
// function pointers in an explicit vtable to implement its
// operations, rather than weak-bound magic symbols.
//
// This alternative implementation lets us mix functions that depend
// on different runtime systems, as long as we isolate symbol
// resolution correctly with `dlmopen(3)`.
//
// The pointer-heavy implementation ensures that any implementation is
// ABI compatible.
//
// This class has no thread-safety guarantee.  Each thread should have
// its own copy.
template <typename Result, typename Arg>
class ExplicitFunction {
 public:
  struct Ops {
    size_t size_of_result;
    size_t size_of_arg;
    const char* function_typename;
    Result (*invoke)(void*, const Arg*);
    void* (*copy)(const void*);
    void (*destroy)(void*);
  };

  ExplicitFunction(const Ops* ops, void* context)
      : ops_(ops), context_(context) {
    if (!IsValid()) {
      ops_->destroy(context_);

      ops_ = nullptr;
      context_ = nullptr;
    }
  }

  ~ExplicitFunction() {
    if (ops_ != nullptr) {
      ops_->destroy(context_);
    }
  }

  // Copyable and movable, but not assignable: we don't need
  // assignment for now, so we might as well minimise the untested bug
  // surface.
  ExplicitFunction(const ExplicitFunction& other)
      : ops_(other.ops_), context_(ops_->copy(other.context_)) {}

  ExplicitFunction(ExplicitFunction&& other)
      : ops_(other.ops_), context_(other.context_) {
    other.ops_ = nullptr;
    other.context_ = nullptr;
  }

  ExplicitFunction& operator=(const ExplicitFunction&) = delete;
  ExplicitFunction& operator=(ExplicitFunction&&) = delete;

  // Performs some minimal tests for ABI validity between the caller
  // and the code that generated the `Ops` vtable.
  //
  // Returns true if the vtable might be compatible with the caller,
  // false if there is definite incompatibility.
  bool IsValid() const {
    if (ops_ == nullptr) {
      return false;
    }

    std::function<Result(const Arg*)> expected_fn;
    const char* expected_name = typeid(expected_fn).name();

    if (ops_->size_of_result != sizeof(Result) ||
        ops_->size_of_arg != sizeof(Arg) ||
        std::strcmp(expected_name, ops_->function_typename) != 0) {
      std::cerr << "ABI mismatch for << " << __PRETTY_FUNCTION__
                << " : ops->size_of_result " << ops_->size_of_result << " VS "
                << sizeof(Result) << ", ops->size_of_arg " << ops_->size_of_arg
                << " VS " << sizeof(Arg) << ", ops->function_typename "
                << ops_->function_typename << " VS " << expected_name << ".\n";
      return false;
    }

    return true;
  }

  inline __attribute__((__always_inline__)) Result operator()(
      const Arg* args) const {
    auto* invoke = ops_->invoke;
    void* context = context_;
    // Align the indirect call to make sure it has room for branch
    // prediction data, and insert a compiler barrier to load
    // everything in registers before padding.
    asm volatile(
#if ABSL_CACHELINE_SIZE >= 128
        ".align 128\n\t"
#elif ABSL_CACHELINE_SIZE == 64
        ".align 64\n\t"
#else
        ".align 32\n\t"
#endif
        : "+r"(invoke), "+r"(context), "+r"(args)::"memory");

    return invoke(context, args);
  }

 private:
  const Ops* ops_;  // Only null in invalid instances.
  void* context_;   // Nullable.
};

struct Identity {
  void operator()() const {}

  template <typename T>
  const T& operator()(const T& x) const {
    return x;
  }
};

// Applies fn(args...), and returns the result as a tuple: void
// becomes a nullary tuple, tuples stay as is, and non-tuple values
// are wrapped in a singleton tuple.
struct CallAndTuplify {
  template <typename Fn, typename T>
  static auto Apply(int, Fn&& fn, T&& args) -> absl::enable_if_t<
      std::is_void<decltype(absl::apply(std::forward<Fn>(fn),
                                        std::forward<T>(args)))>::value,
      std::tuple<>> {
    absl::apply(std::forward<Fn>(fn), std::forward<T>(args));
    return std::make_tuple();
  }

  template <typename Fn, typename T>
  static auto Apply(int, Fn&& fn, T&& args)
      -> decltype(std::tuple_cat(std::make_tuple(),
                                 absl::apply(std::forward<Fn>(fn),
                                             std::forward<T>(args)))) {
    return absl::apply(std::forward<Fn>(fn), std::forward<T>(args));
  }

  // The char overload will only apply if none of the int ones do.
  template <typename Fn, typename T>
  static auto Apply(char, Fn&& fn, T&& args)
      -> decltype(std::make_tuple(absl::apply(std::forward<Fn>(fn),
                                              std::forward<T>(args)))) {
    return std::make_tuple(
        absl::apply(std::forward<Fn>(fn), std::forward<T>(args)));
  }

  template <typename Fn, typename T,
            typename RawT = typename std::remove_reference<T>::type>
  auto operator()(Fn&& fn, T&& args) const
      // The comma operator discards the first expression, which is only
      // here to make sure that `RawT` is a tuple(-like) type.
      -> decltype(std::tuple_size<RawT>::value,
                  Apply(int{0}, std::forward<Fn>(fn), std::forward<T>(args))) {
    // The 0 literal will match against `int` if possible, and against
    // `char` as a last resort.
    return Apply(int{0}, std::forward<Fn>(fn), std::forward<T>(args));
  }
};

// `const T` if the T functor can be invoked as const and is fast to
// copy, `const T&` otherwise, or `T&` if the functor must be mutable.
template <typename T, typename Args,
          bool fast_copy =
              std::is_trivially_copyable<T>::value && sizeof(T) <= 64,
          typename = int>
struct CopyFnTypeIfPossible {
  using type = T&;

  static ABSL_DEPRECATED(
      "Timed functions should have operator()(...) "
      "const.") void BadUsageNotice() {}
};

template <typename T, typename Args>
struct CopyFnTypeIfPossible<T, Args, true,
                            // This decltype can only be instantiated
                            // if T is const callable, but always matches the
                            // default type of `int`.
                            decltype(CallAndTuplify()(std::declval<const T>(),
                                                      std::declval<Args>()),
                                     int{0})> {
  using type = const T;

  static void BadUsageNotice() {}
};

template <typename T, typename Args>
struct CopyFnTypeIfPossible<T, Args, false,
                            decltype(CallAndTuplify()(std::declval<const T>(),
                                                      std::declval<Args>()),
                                     int{0})> {
  using type = const T&;

  static ABSL_DEPRECATED(
      "Timed functions should be small (<= 64 chars) and trivially "
      "copyable.") void BadUsageNotice() {}
};

// Returns true if all the time observations are ordered.
template <typename... T>
__attribute__((__noinline__)) bool AllInOrder(
    absl::Span<const std::tuple<uint64_t, uint64_t, T...>> values) {
  if (values.empty()) {
    return true;
  }

  uint64_t previous = std::get<0>(values[0]);
  for (const auto& value : values) {
    if (previous <= std::get<0>(value) &&
        std::get<0>(value) <= std::get<1>(value)) {
      previous = std::get<1>(value);
    } else {
      return false;
    }
  }

  return true;
}

template <typename T>
struct IsFunctionTypeHelper {
  static constexpr bool value = false;
};

template <typename R, typename... Args>
struct IsFunctionTypeHelper<std::function<R(Args...)>> {
  static constexpr bool value = true;
};

template <typename R, typename... Args>
struct IsFunctionTypeHelper<R(Args...)> {
  static constexpr bool value = true;
};

template <typename R, typename... Args>
struct IsFunctionTypeHelper<R (*)(Args...)> {
  static constexpr bool value = true;
};

template <typename T>
struct IsFunctionType : public IsFunctionTypeHelper<typename std::remove_cv<
                            typename std::remove_reference<T>::type>::type> {};
}  // namespace internal
}  // namespace bench
#endif /* !BENCH_META_H */
