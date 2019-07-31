#ifndef BENCH_META_H
#define BENCH_META_H
#include <cstddef>
#include <cstdint>
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
#include "bench/stable-unique-ptr.h"

// Misc C++ metaprogramming noise for compare-functions.h
namespace bench {
namespace internal {
// Wrap timed result in this struct to work around potential ABI
// issues when sharing std::tuple between c++ standard libaries.
template <typename T>
struct TimedResult {
  static constexpr size_t kResultSize = sizeof(T);

  uint64_t begin;
  uint64_t end;

  // It's the size of T that's really up for debate, so only compare
  // that.  The rest is mandated by the C ABI.
  T result;
};

template <typename T>
constexpr size_t TimedResult<T>::kResultSize;

// Try to detect obvious cases of ABI unsafety.
template <typename T>

// This constraint is a relaxation of std::is_pod, which checks that
// a class or struct definition is compatible with C.
//
// Checking for standard layout guarantees that the compiler can't
// mess with the ordering of members too much, and trivial copyability
// means it's safe to create another instance by `memcpy`ing, so the
// destructor must also be trivial.
struct IsProbablyABISafe
    : public std::integral_constant<bool,
                                    std::is_trivially_copyable<T>::value &&
                                        std::is_standard_layout<T>::value> {};

// But we know multi-elements tuples aren't.
template <typename T, typename U, typename... Vs>
struct IsProbablyABISafe<std::tuple<T, U, Vs...>> : public std::false_type {};

// There aren't too many ways to implement nullary tuples,
// so this is probably safe.
template <>
struct IsProbablyABISafe<std::tuple<>> : public std::true_type {};

template <typename T>
struct IsProbablyABISafe<std::tuple<T>> : public IsProbablyABISafe<T> {};

template <typename T>
struct IsProbablyABISafe<TimedResult<T>> : public IsProbablyABISafe<T> {};

template <typename T>
struct IsProbablyABISafe<StableUniquePtr<T>> : public IsProbablyABISafe<T> {};

static_assert(!IsProbablyABISafe<std::tuple<int, int>>::value,
              ">1-element tuple should not be marked safe.");

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
  using ExpectedFunctionType = std::function<Result(const Arg*)>;

  struct Ops {
    size_t size_of_result;
    size_t size_of_arg;
    bool is_probably_abi_safe;
    const char* function_typename;
    Result (*invoke)(void*, const Arg*);
    void* (*copy)(const void*);
    void (*destroy)(void*);
  };

  static constexpr bool kIsProbablyABISafe =
      IsProbablyABISafe<Arg>::value && IsProbablyABISafe<Result>::value;

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

    ExpectedFunctionType expected_fn;
    const char* expected_name = typeid(expected_fn).name();

    if (ops_->size_of_result != Result::kResultSize ||
        ops_->size_of_arg != sizeof(Arg) ||
        ops_->is_probably_abi_safe != kIsProbablyABISafe ||
        std::strcmp(expected_name, ops_->function_typename) != 0) {
      std::cerr << "ABI mismatch for " << __PRETTY_FUNCTION__
                << " : ops->size_of_result " << ops_->size_of_result << " VS "
                << Result::kResultSize << ", ops->size_of_arg "
                << ops_->size_of_arg << " VS " << sizeof(Arg)
                << ", ops->is_probably_abi_safe "
                << (ops_->is_probably_abi_safe ? "true" : "false") << " VS "
                << (kIsProbablyABISafe ? "true" : "false")
                << ", ops->function_typename " << ops_->function_typename
                << " VS " << expected_name << ".\n";
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

// Constructs the ExplicitFunction type for a function that accepts
// a `const GenResult*` and returns a `TimedResult<FnResult>`.
template <typename FnResult, typename GenResult>
struct TimingFunctionTypeGenerator {
  using type = ExplicitFunction<TimedResult<FnResult>, GenResult>;
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

// Determines whether `FunctionType` is a `TimingFunction` that can
// accept the tuplified instances generated by `Generator`.
template <typename Generator, typename FunctionType>
struct IsTimingFunctionForGenerator : public std::false_type {};

template <typename Generator, typename FnResult>
struct IsTimingFunctionForGenerator<
    Generator,
    ExplicitFunction<TimedResult<FnResult>,
                     decltype(CallAndTuplify()(std::declval<Generator>(),
                                               std::make_tuple()))>>
    : public std::true_type {};

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
template <typename T>
__attribute__((__noinline__)) bool AllInOrder(
    absl::Span<const std::tuple<TimedResult<T>>> values) {
  if (values.empty()) {
    return true;
  }

  uint64_t previous = std::get<0>(values[0]).begin;
  for (const auto& tuple : values) {
    const auto& value = std::get<0>(tuple);
    if (previous <= value.begin && value.begin <= value.end) {
      previous = value.end;
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
