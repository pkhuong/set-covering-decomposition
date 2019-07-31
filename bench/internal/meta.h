#ifndef BENCH_META_H
#define BENCH_META_H
#include <functional>
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
