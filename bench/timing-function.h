#ifndef BENCH_TIMING_FUNCTION_H
#define BENCH_TIMING_FUNCTION_H
#include "bench/internal/meta.h"
#include "bench/time.h"

#define TIMING_FUNCTION_HUGE_PAGE_SIZE (1UL << 21)

#define DEFINE_MAKE_TIMING_FUNCTION(NAME, Generator, ...)                   \
  extern "C" decltype(::bench::MakeTimingFunction<Generator>(__VA_ARGS__))  \
      __attribute__((__aligned__(TIMING_FUNCTION_HUGE_PAGE_SIZE))) NAME() { \
    return ::bench::MakeTimingFunction<Generator>(__VA_ARGS__);             \
  }

namespace bench {
// A TimingFunction<FnResult, Generator> is an internal late-bound
// functor for a routine that accepts a `const Instance*`, where
// `Instance` is the tuplified type returned by a `Generator`, and
// returns a TimedResult for a value of tuplified type `FnResult`.
template <typename FnResult, typename Generator,
          typename GenResult = decltype(internal::CallAndTuplify()(
              std::declval<Generator>(), std::make_tuple()))>
using TimingFunction =
    typename internal::TimingFunctionTypeGenerator<FnResult, GenResult>::type;

// Returns a type erased functor that is, as much as possible, independent
// of the caller's underlying C++ and C runtime systems.  This independence
// lets us time functions that are defined in shared objects that are built
// with different toolchains.
//
// We expect to process inputs returned by `Generator()()`, prepare them
// for timing with `Prep`, and time the work performed by `Fn`.
//
// If provided, different `tag`s prevent the compiler and the linker
// from merging the machine code for similar timing function.
template <typename Generator, const char* tag = nullptr, typename Prep,
          typename Fn,
          typename GenResult = decltype(internal::CallAndTuplify()(
              std::declval<Generator>(), std::make_tuple())),
          typename PrepResult = decltype(internal::CallAndTuplify()(
              std::declval<Prep>(), std::declval<const GenResult>())),
          typename FnResult = decltype(internal::CallAndTuplify()(
              std::declval<Fn>(), std::declval<PrepResult>()))>
TimingFunction<FnResult, Generator> MakeTimingFunction(Prep prep_fn,
                                                       Fn timed_fn);

// Convenience overload for the case when there is no Prep work,
// or when the prep functor can be default constructed.
template <typename Generator, const char* tag = nullptr,
          typename Prep = internal::Identity, typename Fn,
          typename GenResult = decltype(internal::CallAndTuplify()(
              std::declval<Generator>(), std::make_tuple())),
          typename PrepResult = decltype(internal::CallAndTuplify()(
              std::declval<Prep>(), std::declval<const GenResult>())),
          typename FnResult = decltype(internal::CallAndTuplify()(
              std::declval<Fn>(), std::declval<PrepResult>()))>
TimingFunction<FnResult, Generator> MakeTimingFunction(Fn timed_fn) {
  return MakeTimingFunction<Generator, tag>(Prep(), std::move(timed_fn));
}

template <typename Generator, const char* tag, typename Prep, typename Fn,
          typename GenResult, typename PrepResult, typename FnResult>
TimingFunction<FnResult, Generator> MakeTimingFunction(Prep prep_fn,
                                                       Fn timed_fn) {
  using Ret = TimingFunction<FnResult, void, GenResult>;

  // Warn about any suboptimal function type.
  internal::CopyFnTypeIfPossible<Fn, PrepResult>::BadUsageNotice();

  using ContextT = std::pair<Prep, Fn>;

  static const auto invoke =
      +[](void* context, const GenResult* arg)
          __attribute__((__aligned__(2 * ABSL_CACHELINE_SIZE), __hot__)) {
    auto* local_context = static_cast<ContextT*>(context);

    const internal::CallAndTuplify apply;
    // Make sure each tag gets its own copy of the code.
    asm volatile("" ::"r"(tag));

    typename internal::CopyFnTypeIfPossible<Fn, PrepResult>::type local_fn =
        local_context->second;

    auto prep_input = apply(local_context->first, *arg);
    const uint64_t begin = GetTicksBeginWithBarrier(prep_input);
    auto result = apply(local_fn, std::move(prep_input));
    // Force a compiler barrier on the result of the call, if it
    // exists.
    if (std::tuple_size<decltype(result)>::value > 0) {
      asm volatile("" ::"X"(result) : "memory");
    }
    const uint64_t end = GetTicksEnd();

    return internal::TimedResult<FnResult>{begin, end, std::move(result)};
  };

  static const auto copy = +[](const void* context) {
    const auto* local_context = static_cast<const ContextT*>(context);
    return static_cast<void*>(new ContextT(*local_context));
  };

  static const auto destroy = +[](void* context) {
    auto* local_context = static_cast<ContextT*>(context);
    delete local_context;
  };

  static const typename Ret::ExpectedFunctionType fn_type;

  static const typename Ret::Ops ops = {
      sizeof(FnResult),
      sizeof(GenResult),
      Ret::kIsProbablyABISafe,
      typeid(fn_type).name(),
      invoke,
      copy,
      destroy,
  };

  return Ret(&ops, new ContextT(std::move(prep_fn), std::move(timed_fn)));
}
}  // namespace bench
#endif /*!BENCH_TIMING_FUNCTION_H */
