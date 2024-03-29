#ifndef BENCH_COMPARE_FUNCTIONS_H
#define BENCH_COMPARE_FUNCTIONS_H
// The `CompareFunctions` overload defined in this header perform an
// A/B comparison in parallel.
//
// See `*-test.h` in this directory for statistical tests.
//
// Data generation is potentially parallelised, with copies of all
// the functors.
//
//  1. Generate an input `instance` with the `generator()`
//  2. Call the timing function for A, which will perform any
//     setup work, and time the call to the interesting code.
//  3. Call the timing function for B.
//  4. Compare the timing and results with `comparator({time_a, fn_a_result...},
//                                                     {time_b, fn_b_result...},
//                                                     instance...)`.
//
// Each input instance is used for one call to A and one call to B, so
// it's up to the (stateful) `Generator` to change only part of the
// arguments when it makes sense.
//
// In steps 1, 2, and 3, a return type of `void` is piped as 0
// arguments to the next call, and tuple results are unpacked into
// multiple arguments.
//
// The MakeTimingFunction factory similarly pipes a const reference to
// the Generator's instance to a `Prep` function, then that prep
// function's return value (a massaged version of the instance) to the
// `Fn` to time.
//
// The test harness attempts to avoid false positives by:
//  1. accumulating a few hundred data points before passing any
//     judgment, in order to avoid giving too much weight to
//     initialisation effects (in the code, in the execution
//     environment, e.g., paging in code, or in the microarchitectural
//     state).
//  2. disregarding cold cache / cold code effects by identifying and
//     dropping data points generated just after spending a lot of
//     time in the harness's code or after preemption.
//  3. avoiding bias introduced by consistent (or patterned in a way
//     that branch predictors can match) evaluation order by
//     randomising the order in which `FnA` and `FnB` are called.
//  4. minimising noise introduced by the compiler in the timed
//     section with liberal application of compiler barriers,
//     noinline, and code alignment.
//
// The test harness thus offers a consistent "hot" execution
// environment. When we're interested in the behaviour of cold code,
// the `Prep` functions must actively setup a cold
// (micro)architectural state.
//
// The analysis is called in the thread that called `CompareFunctions`,
// with `Observe()` on a list of results returned by `comparator`,
// until `Done()` returns true. `CompareFunctions` then finally returns the
// value returned by `analysis->Summary(&std::clog)`.

#include <assert.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "bench/internal/constructable-array.h"
#include "bench/internal/meta.h"
#include "bench/internal/pooled-thread.h"
#include "bench/test-params.h"
#include "bench/time.h"
#include "bench/timing-function.h"
#include "prng.h"

namespace bench {
// This overload is the general case: `params` specify the behaviour of
// the test harness, `generator` generates instances, and `timing_{a,b}`
// accept the instances and returns a processing time and a return value;
// `comparator` takes the results and converts them to the format
// expected by `analysis`, and the caller is able to inspect
// `analysis` once `CompareFunction` returns.
//
// Any type mismatch in the `TimingFunction`s (which can only happen
// if the `TimingFunction`s were generated by making an untyped call
// into a dynamically linked library) will cause an immediate `abort()`.
template <typename Generator, typename TimingFnA, typename TimingFnB,
          typename Comparator, typename Analysis>
auto CompareFunctions(const TestParams& params, Generator generator,
                      TimingFnA timing_a, TimingFnB timing_b,
                      Comparator comparator, Analysis* analysis) ->
    typename std::enable_if<
        internal::IsTimingFunctionForGenerator<Generator, TimingFnA>::value &&
            internal::IsTimingFunctionForGenerator<Generator, TimingFnB>::value,
        decltype(analysis->Done(),  // This call does nothing (the
                 // comma operator discards its return value), except
                 // check that `analysis->Done()` is a valid expression.
                 analysis->Summary(std::declval<std::ostream*>()))>::type;

// In this overload, `analysis->params()` provides the test parameters.
template <typename Generator, typename TimingFnA, typename TimingFnB,
          typename Comparator, typename Analysis>
auto CompareFunctions(Generator generator, TimingFnA timing_a,
                      TimingFnB timing_b, Comparator comparator,
                      Analysis* analysis) ->
    typename std::enable_if<
        internal::IsTimingFunctionForGenerator<Generator, TimingFnA>::value &&
            internal::IsTimingFunctionForGenerator<Generator, TimingFnB>::value,
        decltype(analysis->params(), analysis->Done(),
                 analysis->Summary(std::declval<std::ostream*>()))>::type {
  return CompareFunctions(analysis->params(), std::move(generator),
                          std::move(timing_a), std::move(timing_b),
                          std::move(comparator), analysis);
}

// Convenience overload when there is no prep transformation: `FnA`
// and `FnB` are wrapped in a `TimingFunction` with an `Identity`
// functor as the `Prep` step.
template <typename Generator, typename FnA, typename FnB, typename Comparator,
          typename Analysis>
auto CompareFunctions(const TestParams& params, Generator generator, FnA fn_a,
                      FnB fn_b, Comparator comparator, Analysis* analysis) ->
    typename std::enable_if<
        !internal::IsTimingFunctionForGenerator<Generator, FnA>::value &&
            !internal::IsTimingFunctionForGenerator<Generator, FnB>::value,
        decltype(analysis->Done(),
                 analysis->Summary(std::declval<std::ostream*>()))>::type {
  return CompareFunctions(params, std::move(generator),
                          MakeTimingFunction<Generator>(std::move(fn_a)),
                          MakeTimingFunction<Generator>(std::move(fn_b)),
                          std::move(comparator), analysis);
}

// Convenience overload with no prep transformation and the test parameters
// provided by `analysis->params()`.
template <typename Generator, typename FnA, typename FnB, typename Comparator,
          typename Analysis>
auto CompareFunctions(Generator generator, FnA fn_a, FnB fn_b,
                      Comparator comparator, Analysis* analysis) ->
    typename std::enable_if<
        !std::is_same<Generator, TestParams>::value &&
            !internal::IsTimingFunctionForGenerator<Generator, FnA>::value &&
            !internal::IsTimingFunctionForGenerator<Generator, FnB>::value,
        decltype(analysis->Done(), analysis->params(),
                 analysis->Summary(std::declval<std::ostream*>()))>::type {
  return CompareFunctions(analysis->params(), std::move(generator),
                          MakeTimingFunction<Generator>(std::move(fn_a)),
                          MakeTimingFunction<Generator>(std::move(fn_b)),
                          std::move(comparator), analysis);
}

// This overload must be instantiated with an explicit `Analysis` type
// argument, and will construct a temporary instance of `Analysis` as
// `Analysis(params)` to drive the comparison.  This function passes the
// identity as `prep` functions.
template <typename Analysis, typename Generator, typename FnA, typename FnB,
          typename Comparator>
auto CompareFunctions(const TestParams& params, Generator generator, FnA fn_a,
                      FnB fn_b, Comparator comparator)
    -> decltype(
        Analysis(params), std::declval<Analysis>().Done(),
        std::declval<Analysis>().Summary(std::declval<std::ostream*>())) {
  Analysis analysis(params);
  return CompareFunctions(params, std::move(generator), std::move(fn_a),
                          std::move(fn_b), std::move(comparator), &analysis);
}

// This overload uses a temporary instance of its explicit `Analysis` type
// argument (instantiated as `Analysis(params)`), assumes there is no `prep`
// work, and uses `analysis.comparator()` as the comparator.
template <typename Analysis, typename Generator, typename FnA, typename FnB>
auto CompareFunctions(const TestParams& params, Generator generator, FnA fn_a,
                      FnB fn_b)
    -> decltype(
        Analysis(params), std::declval<Analysis>().comparator(),
        std::declval<Analysis>().Done(),
        std::declval<Analysis>().Summary(std::declval<std::ostream*>())) {
  Analysis analysis(params);
  return CompareFunctions(params, std::move(generator), std::move(fn_a),
                          std::move(fn_b), analysis.comparator(), &analysis);
}

// This overload accepts a pre-constructed analysis, and calls
// `analysis->params()` to obtain test parameters and `analysis->comparator()`
// to generate a comparator.  This function passes the identity as `prep`
// functions.
template <typename Generator, typename FnA, typename FnB, typename Analysis>
auto CompareFunctions(Generator generator, FnA fn_a, FnB fn_b,
                      Analysis* analysis) ->
    typename std::enable_if<
        !std::is_same<Generator, TestParams>::value,
        decltype(analysis->params(), analysis->comparator(), analysis->Done(),
                 analysis->Summary(std::declval<std::ostream*>()))>::type {
  return CompareFunctions(analysis->params(), std::move(generator),
                          std::move(fn_a), std::move(fn_b),
                          analysis->comparator(), analysis);
}

namespace internal {
template <typename Result>
struct ResultBuffer {
  std::vector<Result> buffer;
  // Set to true whenever we detect preemption or otherwise expect
  // the next run to be noisy.
  bool tainted{true};
  uint64_t total{0};
  uint64_t dropped{0};
};

template <typename Result>
struct Accumulator {
  absl::Mutex mu;
  std::vector<std::vector<Result>> buffers GUARDED_BY(mu);
};

// This pattern matching template checks that `T` is a
// `std::tuple<internal::TimedResult<std::tuple<...>>>`, as we expect
// from timing functions.
//
// When that is the case, `ResultTuple` is the inner tuple type,
// prefixed with one uint64_t (the cycle timing).
template <typename T>
struct IsValidResult {
  static constexpr bool value = false;
};

template <typename... Ts>
struct IsValidResult<std::tuple<TimedResult<std::tuple<Ts...>>>> {
  static constexpr bool value = true;

  using ResultTuple = std::tuple<uint64_t, Ts...>;
};

template <typename Generator, typename FnA, typename FnB, typename Comparator>
class StatisticGenerator {
  using GenResult =
      decltype(CallAndTuplify()(std::declval<Generator>(), std::make_tuple()));
  using FnAResult = decltype(
      CallAndTuplify()(std::declval<FnA>(), std::tuple<const GenResult*>()));
  using FnBResult = decltype(
      CallAndTuplify()(std::declval<FnB>(), std::tuple<const GenResult*>()));

  static_assert(
      IsValidResult<FnAResult>::value,
      "The result of timing functions must be a TimedResult<tuple<...>>.");
  static_assert(
      IsValidResult<FnBResult>::value,
      "The result of timing functions must be a TimedResult<tuple<...>>.");

  using Result = decltype(absl::apply(
      std::declval<Comparator>(),
      std::tuple_cat(
          std::make_tuple(
              std::declval<typename IsValidResult<FnAResult>::ResultTuple>(),
              std::declval<typename IsValidResult<FnBResult>::ResultTuple>()),
          std::declval<GenResult>())));

 public:
  // Accumulate results for ~1 billion cycles worth of work.
  static constexpr uint64_t kTargetCyclePerRun = 1000 * 1000 * 1000ULL;
  // Or when we have 200 observations.
  static constexpr size_t kMaxBufferSize = 200;

  // Don't run any analysis until we have 500 observations: we want to
  // avoid giving too much weight to warm up artifacts early on.
  static constexpr uint64_t kMinObservations = 500;

  explicit StatisticGenerator(Generator generator, FnA fn_a, FnB fn_b,
                              Comparator comparator);

  // (Re)creates internal worker threads.
  void Start(size_t num_threads);

  // Notifies all worker threads to stop and waits for them to
  // publish what they have.
  void Stop();

  // Ensures that the accumulator of `ResultBuffer`s is empty iff
  // `Stop()` has been called, and returns that accumulator (and
  // clears it to make room for more results).
  std::vector<std::vector<Result>> Consume();

  // This struct provides a thread-private `Context` for each worker
  // thread that generates comparison results.
  struct Context {
    explicit Context(Generator generator_, FnA fn_a_, FnB fn_b_,
                     Comparator comparator_)
        : generator(std::move(generator_)),
          fn_a(std::move(fn_a_)),
          fn_b(std::move(fn_b_)),
          comparator(std::move(comparator_)) {}

    Generator generator;
    FnA fn_a;
    FnB fn_b;
    Comparator comparator;
    xs256 prng;

    ResultBuffer<Result> buffer;
  };

 private:
  // Each call to `WorkImpl` generates one instance, and uses that
  // same instance to obtain `kResultsPerCall` comparison results.
  static constexpr size_t kResultsPerCall = 1;

  // This function implements one iteration of the work loop: it
  // generates one full buffer's worth of comparison `Result`s in
  // `context`.
  static void Work(Context* context);

  // Adds `context->buffer_size` results to `context->buffer`.  The
  // versions differ in the order in which they call FnA and FnB.
  //
  // Returns the last cycle timestamp observed in this call.
  //
  // The template arguments selects between semantically identical
  // implementations that should even out measurement noise introduced
  // by the compiler's code generation choices.
  template <bool kCallAFirst>
  static uint64_t WorkImpl(Context* context);

  // Repeatedly calls `Work` and `Flush` until `Stop()` is called and
  // notifies `done`.
  static void WorkerFn(Context context, Accumulator<Result>* acc);

  std::vector<internal::PooledThread> workers_;

  Context context_;
  Accumulator<Result> acc_;
};

template <typename Generator, typename FnA, typename FnB, typename Comparator>
constexpr size_t
    StatisticGenerator<Generator, FnA, FnB, Comparator>::kMaxBufferSize;

template <typename Generator, typename FnA, typename FnB, typename Comparator>
constexpr uint64_t
    StatisticGenerator<Generator, FnA, FnB, Comparator>::kMinObservations;

template <typename Generator, typename FnA, typename FnB, typename Comparator>
constexpr uint64_t
    StatisticGenerator<Generator, FnA, FnB, Comparator>::kTargetCyclePerRun;

template <typename Generator, typename FnA, typename FnB, typename Comparator>
__attribute__((__noinline__))
StatisticGenerator<Generator, FnA, FnB, Comparator>::StatisticGenerator(
    Generator generator, FnA fn_a, FnB fn_b, Comparator comparator)
    : context_(std::move(generator), std::move(fn_a), std::move(fn_b),
               std::move(comparator)) {}

template <typename Generator, typename FnA, typename FnB, typename Comparator>
__attribute__((__noinline__)) void
StatisticGenerator<Generator, FnA, FnB, Comparator>::Start(size_t num_threads) {
  workers_.clear();
  if (num_threads <= 1) {
    return;
  }

  workers_.reserve(num_threads - 1);
  for (size_t i = 1; i < num_threads; ++i) {
    Context context(context_);
    auto* acc_ptr = &acc_;
    workers_.emplace_back([context, acc_ptr] { WorkerFn(context, acc_ptr); });
  }
}

template <typename Generator, typename FnA, typename FnB, typename Comparator>
__attribute__((__noinline__)) void
StatisticGenerator<Generator, FnA, FnB, Comparator>::Stop() {
  for (auto& worker : workers_) {
    worker.Cancel();
  }

  workers_.clear();
}

// Flushes `buffer` to `acc`.
template <typename Result>
__attribute__((__noinline__)) void Flush(ResultBuffer<Result>* buffer,
                                         Accumulator<Result>* acc) {
  using std::swap;

  if (WarnOnRepeatedInterrupts(buffer->total, buffer->dropped)) {
    buffer->total = 0;
    buffer->dropped = 0;
  }

  if (buffer->buffer.empty()) {
    return;
  }

  std::vector<Result> new_buffer;
  new_buffer.reserve(buffer->buffer.capacity());
  swap(new_buffer, buffer->buffer);

  absl::MutexLock ml(&acc->mu);
  acc->buffers.push_back(std::move(new_buffer));
}

// Shared tail-end of WorkImpl.  We assume there's only one result
// in each ConstructableArray.
template <typename GenResult, typename Comparator, typename FnAResult,
          typename FnBResult, typename Result>
__attribute__((__noinline__)) uint64_t PublishResults(
    GenResult&& work_unit,
    ConstructableArray<std::tuple<internal::TimedResult<FnAResult>>, 1>* as,
    ConstructableArray<std::tuple<internal::TimedResult<FnBResult>>, 1>* bs,
    Comparator* comparator, ResultBuffer<Result>* buffer) {
  const bool interrupted = InterruptDetected();
  const uint64_t ret =
      std::max(std::get<0>(as->back()).end, std::get<0>(bs->back()).end);

  ++buffer->total;
  if (interrupted || !AllInOrder(absl::MakeConstSpan(*as)) ||
      !AllInOrder(absl::MakeConstSpan(*bs))) {
    ++buffer->dropped;
    buffer->tainted = true;
  } else if (buffer->tainted) {
    buffer->tainted = false;
  } else {
    auto& a = std::get<0>(as->front());
    auto& b = std::get<0>(bs->front());
    auto a_tuple =
        std::tuple_cat(std::make_tuple(a.end - a.begin), std::move(a.result));
    auto b_tuple =
        std::tuple_cat(std::make_tuple(b.end - b.begin), std::move(b.result));
    auto comparison = absl::apply(
        *comparator,
        std::tuple_cat(std::make_tuple(std::move(a_tuple), std::move(b_tuple)),
                       std::move(work_unit)));
    buffer->buffer.push_back(std::move(comparison));
  }

  as->Destroy();
  bs->Destroy();
  return ret;
}

template <typename Generator, typename FnA, typename FnB, typename Comparator>
__attribute__((__noinline__)) auto
StatisticGenerator<Generator, FnA, FnB, Comparator>::Consume()
    -> std::vector<std::vector<Result>> {
  using std::swap;

  std::vector<std::vector<Result>> ret;
  {
    absl::MutexLock ml(&acc_.mu);
    if (!acc_.buffers.empty()) {
      swap(ret, acc_.buffers);
      return ret;
    }
  }

  Work(&context_);
  Flush(&context_.buffer, &acc_);

  {
    absl::MutexLock ml(&acc_.mu);
    swap(ret, acc_.buffers);
  }

  return ret;
}

template <typename Generator, typename FnA, typename FnB, typename Comparator>
/*static*/ __attribute__((__noinline__)) void
StatisticGenerator<Generator, FnA, FnB, Comparator>::Work(Context* context) {
  // Populate an array with pointers to our semantically identical
  // instances of `WorkImpl` in order to randomly call one of them at
  // each iteration.  This should help balance consistent measurement
  // noise introduced by codegen accidents.
  using WorkFn = uint64_t (*)(Context*);
  WorkFn impl[] = {
      WorkImpl<true>, WorkImpl<false>,
  };
  // Tell the compiler we might mutate these two function pointers to
  // make they don't get constant folded into a conditional branch.
  asm("" : "+m"(impl)::"memory");

  context->buffer.buffer.reserve(context->buffer.buffer.size() +
                                 kMaxBufferSize + kResultsPerCall - 1);
  // Always drop the first set of values, they're potentially
  // tainted by the rest of the framework.
  context->buffer.tainted = true;

  const uint64_t actual_cycle_limit = GetTicksBegin() + kTargetCyclePerRun;
  // Only apply the cycle limit after the first set of values, which is
  // always dropped.
  uint64_t cycle_limit = std::numeric_limits<uint64_t>::max();
  while (context->buffer.buffer.size() < kMaxBufferSize) {
    size_t index = context->prng.Uniform(ABSL_ARRAYSIZE(impl));
    WorkFn target = impl[index];
    // And also pretend we might mutate `target` arbitrarily to make
    // sure we get an indirect call.
    asm volatile("" : "+r"(target));
    if (target(context) > cycle_limit) {
      return;
    }

    cycle_limit = actual_cycle_limit;
  }
}

template <typename Generator, typename FnA, typename FnB, typename Comparator>
template <bool kCallAFirst>
/*static*/ __attribute__((__noinline__)) uint64_t
StatisticGenerator<Generator, FnA, FnB, Comparator>::WorkImpl(
    Context* context) {
  using ResultsA = ConstructableArray<FnAResult, kResultsPerCall>;
  using ResultsB = ConstructableArray<FnBResult, kResultsPerCall>;
  struct ResultAFirst {
    ResultsA a;
    ResultsB b;
  };

  struct ResultBFirst {
    ResultsB b;
    ResultsA a;
  };

  {
    // Make sure the functions don't get folded as identical.
    auto x = kCallAFirst;
    asm volatile("" : "+r"(x)::"memory");
  }

  const CallAndTuplify apply;
  typename std::conditional<kCallAFirst, ResultAFirst, ResultBFirst>::type
      results;

  auto work_unit = apply(context->generator, std::make_tuple());

  SetupInterruptDetection();

  if (kCallAFirst) {
    // `ExplicitFunction::operator()` is always_inline and already
    // includes padding before the indirect call to make sure they get
    // their own cache line, and thus prediction slot (hopefully).
    results.a.EmplaceAt(0, context->fn_a(&work_unit));
    results.b.EmplaceAt(0, context->fn_b(&work_unit));
    static_assert(kResultsPerCall == 1,
                  "kResultsPerCall must match number of results exactly.");
  } else {
    // Same thing, but mirrored.
    results.b.EmplaceAt(0, context->fn_b(&work_unit));
    results.a.EmplaceAt(0, context->fn_a(&work_unit));
    static_assert(kResultsPerCall == 1,
                  "kResultsPerCall must match number of results exactly.");
  }

  return PublishResults(std::move(work_unit), &results.a, &results.b,
                        &context->comparator, &context->buffer);
}

template <typename Generator, typename FnA, typename FnB, typename Comparator>
/*static*/ __attribute__((__noinline__)) void
StatisticGenerator<Generator, FnA, FnB, Comparator>::WorkerFn(
    Context context, Accumulator<Result>* acc) {
  while (!PooledThread::Cancelled()) {
    Work(&context);
    Flush(&context.buffer, acc);
  }
}
}  // namespace internal

template <typename Generator, typename TimingFnA, typename TimingFnB,
          typename Comparator, typename Analysis>
auto CompareFunctions(const TestParams& params, Generator generator,
                      TimingFnA timing_a, TimingFnB timing_b,
                      Comparator comparator, Analysis* analysis) ->
    typename std::enable_if<
        internal::IsTimingFunctionForGenerator<Generator, TimingFnA>::value &&
            internal::IsTimingFunctionForGenerator<Generator, TimingFnB>::value,
        decltype(analysis->Done(),
                 analysis->Summary(std::declval<std::ostream*>()))>::type {
  // Initialize the overhead estimate so it's fast to acquire, and
  // to trigger side effects like warning in non-release builds.
  GetTicksOverhead();

  // Noisily stop the analysis on obvious ABI mismatches.
  const bool a_is_valid = timing_a.IsValid();
  const bool b_is_valid = timing_b.IsValid();
  if (!a_is_valid || !b_is_valid) {
    std::cerr << "ABI mismatch between harness";
    if (!a_is_valid) {
      std::cerr << " and version A";
    }
    if (!b_is_valid) {
      std::cerr << " and version B";
    }
    std::cerr << "." << std::endl;
    abort();
    return analysis->Summary(&std::clog);
  }

  internal::StatisticGenerator<Generator, decltype(timing_a),
                               decltype(timing_b), Comparator>
      stat_gen(std::move(generator), std::move(timing_a), std::move(timing_b),
               std::move(comparator));

  uint64_t num_comparisons = 0;
  const auto consume = [analysis, &stat_gen, &num_comparisons] {
    auto stat_chunks = stat_gen.Consume();
    for (auto& chunk : stat_chunks) {
      num_comparisons += chunk.size();
      analysis->Observe(absl::MakeSpan(chunk));
    }
  };

  const absl::Time deadline = absl::Now() + params.timeout;
  const uint64_t max_comparisons = params.max_comparisons;
  const uint64_t min_comparisons =
      std::max(params.min_count, stat_gen.kMinObservations);

  // Each inner loop runs an analysis until `Done()` returns true
  // enough times in a row.
  //
  // The outer loop exists to restart the worker threads and resume
  // generating data if the analysis changes its mind just as we're
  // about to return the summary.
  for (;;) {
    uint32_t consecutive_done = 0;

    stat_gen.Start(params.num_threads);
    while (num_comparisons < max_comparisons) {
      if (deadline != absl::InfiniteFuture() && absl::Now() > deadline) {
        break;
      }
      if (num_comparisons >= min_comparisons) {
        if (analysis->Done()) {
          if (++consecutive_done >= params.confirm_done) {
            break;
          }
        } else {
          consecutive_done = 0;
        }
      }

      consume();
    }

    stat_gen.Stop();
    consume();
    if (analysis->Done()) {
      break;
    }

    assert(params.retry_after_thread_cancel &&
           "analysis->Done() should act like a sticky bit.");
    if (!params.retry_after_thread_cancel ||
        num_comparisons >= max_comparisons || absl::Now() >= deadline) {
      break;
    }
  }

  return analysis->Summary(&std::clog);
}
}  // namespace bench
#endif /* !BENCH_COMPARE_FUNCTIONS_H */
