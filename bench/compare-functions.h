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
//  2a. Convert instance for `FnA` with `PrepA(instance)`
//  2b. Time a call to `FnA`
//  3a. Convert instance for `FnB`
//  3b. Time a call to `FnB`
//  4. Compare the timing and results with `comparator({time_a, fn_a_result...},
//                                                     {time_b, fn_b_result...},
//                                                     instance...)`.
//
// Each input instance is used for one call to `FnA` and one call to
// `FnB`, so it's up to the (stateful) `Generator` to change only part
// of the arguments when it makes sense.
//
// In steps 1, 2, and 3, a return type of `void` is piped as 0
// arguments to the next call, and tuple results are unpacked into
// multiple arguments.
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
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "bench/internal/constructable-array.h"
#include "bench/internal/meta.h"
#include "bench/test-params.h"
#include "bench/time.h"
#include "prng.h"

namespace bench {
// This overload is the general case: `params` specify the behaviour of
// the test harness, `generator` generates instances, `prep_{a,b}`
// convert the generator's instances to the format expected by `fn_{a,b}`,
// `comparator` takes the results and converts them to the format
// expected by `analysis`, and the caller is able to inspect
// `analysis` once `CompareFunction` returns.
template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator, typename Analysis>
auto CompareFunctions(const TestParams& params, Generator generator,
                      PrepA prep_a, FnA fn_a, PrepB prep_b, FnB fn_b,
                      Comparator comparator, Analysis* analysis)
    -> decltype(analysis->Done(),  // This call does nothing (the
                // comma operator discards its return value), except
                // check that `analysis->Done()` is a valid expression.
                analysis->Summary(std::declval<std::ostream*>()));

// In this overload, `analysis->params()` provides the test parameters.
template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator, typename Analysis,
          typename = typename std::enable_if<
              !std::is_same<Generator, TestParams>::value, void>::type>
auto CompareFunctions(Generator generator, PrepA prep_a, FnA fn_a, PrepB prep_b,
                      FnB fn_b, Comparator comparator, Analysis* analysis)
    -> decltype(analysis->params(), analysis->Done(),
                analysis->Summary(std::declval<std::ostream*>())) {
  return CompareFunctions(analysis->params(), std::move(generator),
                          std::move(prep_a), std::move(fn_a), std::move(prep_b),
                          std::move(fn_b), std::move(comparator), analysis);
}

// Convenience overload when there is no prep transformation: `prep_a`
// and `prep_b` are the identity function.
template <typename Generator, typename FnA, typename FnB, typename Comparator,
          typename Analysis>
auto CompareFunctions(const TestParams& params, Generator generator, FnA fn_a,
                      FnB fn_b, Comparator comparator, Analysis* analysis)
    -> decltype(analysis->Done(),
                analysis->Summary(std::declval<std::ostream*>())) {
  return CompareFunctions(params, std::move(generator), internal::Identity(),
                          std::move(fn_a), internal::Identity(),
                          std::move(fn_b), std::move(comparator), analysis);
}

// Convenience overload with no prep transformation and the test parameters
// provided by `analysis->params()`.
template <typename Generator, typename FnA, typename FnB, typename Comparator,
          typename Analysis,
          typename = typename std::enable_if<
              !std::is_same<Generator, TestParams>::value, void>::type>
auto CompareFunctions(Generator generator, FnA fn_a, FnB fn_b,
                      Comparator comparator, Analysis* analysis)
    -> decltype(analysis->Done(), analysis->params(),
                analysis->Summary(std::declval<std::ostream*>())) {
  return CompareFunctions(analysis->params(), std::move(generator),
                          internal::Identity(), std::move(fn_a),
                          internal::Identity(), std::move(fn_b),
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
template <typename Generator, typename FnA, typename FnB, typename Analysis,
          typename = typename std::enable_if<
              !std::is_same<Generator, TestParams>::value, void>::type>
auto CompareFunctions(Generator generator, FnA fn_a, FnB fn_b,
                      Analysis* analysis)
    -> decltype(analysis->params(), analysis->comparator(), analysis->Done(),
                analysis->Summary(std::declval<std::ostream*>())) {
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

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
class StatisticGenerator {
  using GenResult =
      decltype(CallAndTuplify()(std::declval<Generator>(), std::make_tuple()));
  using PrepAResult = decltype(
      CallAndTuplify()(std::declval<PrepA>(), std::declval<const GenResult>()));
  using FnAResult = decltype(
      CallAndTuplify()(std::declval<FnA>(), std::declval<PrepAResult>()));
  using PrepBResult = decltype(
      CallAndTuplify()(std::declval<PrepB>(), std::declval<const GenResult>()));
  using FnBResult = decltype(
      CallAndTuplify()(std::declval<FnB>(), std::declval<PrepBResult>()));

  using Result = decltype(absl::apply(
      std::declval<Comparator>(),
      std::tuple_cat(
          std::make_tuple(std::tuple_cat(std::make_tuple(uint64_t{0}),
                                         std::declval<FnAResult>()),
                          std::tuple_cat(std::make_tuple(uint64_t{0}),
                                         std::declval<FnBResult>())),
          std::declval<GenResult>())));

  static_assert(!IsFunctionType<FnA>::value,
                "Timed functions should not be late-bound function types. Use "
                "WRAP_FUNCTION.");
  static_assert(!IsFunctionType<FnB>::value,
                "Timed functions should not be late-bound function types. Use "
                "WRAP_FUNCTION.");

 public:
  // Accumulate results for ~1 billion cycles worth of work.
  static constexpr uint64_t kTargetCyclePerRun = 1000 * 1000 * 1000ULL;
  // Or when we have 200 observations.
  static constexpr size_t kMaxBufferSize = 200;

  // Don't run any analysis until we have 500 observations: we want to
  // avoid giving too much weight to warm up artifacts early on.
  static constexpr uint64_t kMinObservations = 500;

  explicit StatisticGenerator(Generator generator, PrepA prep_a, FnA fn_a,
                              PrepB prep_b, FnB fn_b, Comparator comparator);

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
    explicit Context(Generator generator_, PrepA prep_a_, FnA fn_a_,
                     PrepB prep_b_, FnB fn_b_, Comparator comparator_)
        : generator(std::move(generator_)),
          prep_a(std::move(prep_a_)),
          fn_a(std::move(fn_a_)),
          prep_b(std::move(prep_b_)),
          fn_b(std::move(fn_b_)),
          comparator(std::move(comparator_)) {}

    Generator generator;
    PrepA prep_a;
    FnA fn_a;
    PrepB prep_b;
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
  static void WorkerFn(Context context, const absl::Notification* done,
                       Accumulator<Result>* acc);

  // This notification is initially null, and populated for each new
  // batch of worker threads in `Start()`, then signaled and cleared
  // back to null in `Stop()`.
  std::unique_ptr<absl::Notification> done_;
  std::vector<std::thread> workers_;

  Context context_;
  Accumulator<Result> acc_;
};

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
constexpr size_t StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB,
                                    Comparator>::kMaxBufferSize;

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
constexpr uint64_t StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB,
                                      Comparator>::kMinObservations;

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
constexpr uint64_t StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB,
                                      Comparator>::kTargetCyclePerRun;

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
__attribute__((__noinline__))
StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB,
                   Comparator>::StatisticGenerator(Generator generator,
                                                   PrepA prep_a, FnA fn_a,
                                                   PrepB prep_b, FnB fn_b,
                                                   Comparator comparator)
    : context_(std::move(generator), std::move(prep_a), std::move(fn_a),
               std::move(prep_b), std::move(fn_b), std::move(comparator)) {}

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
__attribute__((__noinline__)) void
StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB, Comparator>::Start(
    size_t num_threads) {
  assert(done_ == nullptr);

  done_ = absl::make_unique<absl::Notification>();
  workers_.clear();
  if (num_threads <= 1) {
    return;
  }

  workers_.reserve(num_threads - 1);
  for (size_t i = 1; i < num_threads; ++i) {
    workers_.emplace_back(WorkerFn, Context(context_), done_.get(), &acc_);
  }
}

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
__attribute__((__noinline__)) void
StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB, Comparator>::Stop() {
  assert(done_ != nullptr);
  done_->Notify();
  for (auto& worker : workers_) {
    worker.join();
  }

  done_.reset();
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
    ConstructableArray<std::tuple<uint64_t, uint64_t, FnAResult>, 1>* as,
    ConstructableArray<std::tuple<uint64_t, uint64_t, FnBResult>, 1>* bs,
    Comparator* comparator, ResultBuffer<Result>* buffer) {
  const bool interrupted = InterruptDetected();
  const uint64_t ret =
      std::max(std::get<1>(as->back()), std::get<1>(bs->back()));

  ++buffer->total;
  if (interrupted || !AllInOrder(absl::MakeConstSpan(*as)) ||
      !AllInOrder(absl::MakeConstSpan(*bs))) {
    ++buffer->dropped;
    buffer->tainted = true;
  } else if (buffer->tainted) {
    buffer->tainted = false;
  } else {
    auto& a = as->front();
    auto& b = bs->front();
    auto a_tuple =
        std::tuple_cat(std::make_tuple(std::get<1>(a) - std::get<0>(a)),
                       std::move(std::get<2>(a)));
    auto b_tuple =
        std::tuple_cat(std::make_tuple(std::get<1>(b) - std::get<0>(b)),
                       std::move(std::get<2>(b)));
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

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
__attribute__((__noinline__)) auto
StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB, Comparator>::Consume()
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

  if (done_ != nullptr) {
    Work(&context_);
    Flush(&context_.buffer, &acc_);

    absl::MutexLock ml(&acc_.mu);
    swap(ret, acc_.buffers);
  }

  return ret;
}

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
/*static*/ __attribute__((__noinline__)) void
StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB, Comparator>::Work(
    Context* context) {
  // Warn about any suboptimal function type. Do so here instead of
  // in WorkFn to warn once per template instantiation.
  CopyFnTypeIfPossible<FnA, PrepAResult>::BadUsageNotice();
  CopyFnTypeIfPossible<FnB, PrepBResult>::BadUsageNotice();

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

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
template <bool kCallAFirst>
// Introduce extra alignment to avoid any consistent measurement error
// introduced by mechanisms that work at extra wide granularity.  In
// particular we want to make sure that fetching and decoding the code
// does not affect specific instances of this function differently The
// impact of wasted instruction bytes should be small (< 25%) compared
// to the code size of this function.
/*static*/ __attribute__((__aligned__(2 * ABSL_CACHELINE_SIZE), __noinline__))
uint64_t
StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB, Comparator>::WorkImpl(
    Context* context) {
  using ResultsA = ConstructableArray<std::tuple<uint64_t, uint64_t, FnAResult>,
                                      kResultsPerCall>;
  using ResultsB = ConstructableArray<std::tuple<uint64_t, uint64_t, FnBResult>,
                                      kResultsPerCall>;
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

  typename CopyFnTypeIfPossible<FnA, PrepAResult>::type fn_a = context->fn_a;
  typename CopyFnTypeIfPossible<FnB, PrepBResult>::type fn_b = context->fn_b;

  // Generate one instance, and use it for one call to FnA and one
  // call to FnB.
  auto work_unit = apply(context->generator, std::make_tuple());

#define DO(X)                                              \
  do {                                                     \
    auto prep = apply(context->prep_##X, work_unit);       \
                                                           \
    const uint64_t begin = GetTicksBeginWithBarrier(prep); \
    auto result = apply(fn_##X, std::move(prep));          \
    if (std::tuple_size<decltype(result)>::value > 0) {    \
      asm volatile("" ::"X"(result) : "memory");           \
    }                                                      \
    const uint64_t end = GetTicksEnd();                    \
                                                           \
    results.X.EmplaceAt(0, begin, end, std::move(result)); \
  } while (0)

  SetupInterruptDetection();

  if (kCallAFirst) {
    DO(a);
    DO(b);
    static_assert(kResultsPerCall == 1,
                  "kResultsPerCall must match number of results exactly.");
  } else {
    // Same thing, but mirrored.
    DO(b);
    DO(a);
    static_assert(kResultsPerCall == 1,
                  "kResultsPerCall must match number of results exactly.");
  }
#undef DO

  return PublishResults(std::move(work_unit), &results.a, &results.b,
                        &context->comparator, &context->buffer);
}

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator>
/*static*/ __attribute__((__noinline__)) void
StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB, Comparator>::WorkerFn(
    Context context, const absl::Notification* done, Accumulator<Result>* acc) {
  while (!done->HasBeenNotified()) {
    Work(&context);
    Flush(&context.buffer, acc);
  }
}
}  // namespace internal

template <typename Generator, typename PrepA, typename FnA, typename PrepB,
          typename FnB, typename Comparator, typename Analysis>
auto CompareFunctions(const TestParams& params, Generator generator,
                      PrepA prep_a, FnA fn_a, PrepB prep_b, FnB fn_b,
                      Comparator comparator, Analysis* analysis)
    -> decltype(analysis->Done(),
                analysis->Summary(std::declval<std::ostream*>())) {
  // Initialize the overhead estimate so it's fast to acquire, and
  // to trigger side effects like warning in non-release builds.
  GetTicksOverhead();

  internal::StatisticGenerator<Generator, PrepA, FnA, PrepB, FnB, Comparator>
      stat_gen(std::move(generator), std::move(prep_a), std::move(fn_a),
               std::move(prep_b), std::move(fn_b), std::move(comparator));

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
