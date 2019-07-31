#ifndef BENCH_WRAP_FUNCTION_H
#define BENCH_WRAP_FUNCTION_H
#include <utility>

// Expands to a type that is specialised to forward calls to FN,
// without any indirect call or jump.
#define WRAP_FUNCTION(FN) internal::FunctionWrapper<decltype(FN), (FN)>

namespace bench {
namespace internal {
template <typename T, T* fn>
struct FunctionWrapper;

template <typename R, typename... Args, R (*fn)(Args...)>
struct FunctionWrapper<R(Args...), fn> {
  template <typename... RealArgs>
  inline __attribute__((__always_inline__)) R operator()(
      RealArgs&&... args) const {
    return fn(std::forward<RealArgs>(args)...);
  }
};
}  // namespace internal
}  // namespace bench
#endif /*!BENCH_WRAP_FUNCTION_H */
