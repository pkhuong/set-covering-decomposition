#include "perf-test/find-min-value.h"

#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "bench/bounded-mean-test.h"
#include "bench/compare-functions.h"
#include "bench/extract-timing-function.h"
#include "bench/kolmogorov-smirnov-test.h"
#include "bench/quantile-test.h"
#include "bench/stable-unique-ptr.h"
#include "bench/timing-function.h"

using ::bench::BoundedMeanTest;
using ::bench::CompareFunctions;
using ::bench::ComparisonResult;
using ::bench::KolmogorovSmirnovTest;
using ::bench::QuantileTest;
using ::bench::TestParams;

ABSL_FLAG(std::string, lib_a, "perf-test/libbase-find-min-value.so",
          "Path to the shared object for version A.");

ABSL_FLAG(
    std::string, fn_a, "MakeFindMinValue",
    "Name of the function to generate the timing function for version A.");

ABSL_FLAG(std::string, lib_b, "perf-test/libbase-find-min-value.so",
          "Path to the shared object for version A.");

ABSL_FLAG(
    std::string, fn_b, "MakeBaseFindMinValue",
    "Name of the function to generate the timing function for version B.");

ABSL_FLAG(size_t, num_values, 10,
          "Number of values in the arguments to FindMinValue");

ABSL_FLAG(bool, fn_a_lte, false,
          "If true, tests whether A is not worse than B. If false (default), "
          "tests for equality.");

ABSL_FLAG(bool, load_a_first, true,
          "If true, load fn a first, otherwise load fn b first. This flag has "
          "no impact on the analysis, but helps control for accidental "
          "effects of dlopen ordering on performance.");

ABSL_FLAG(size_t, num_threads, 2,
          "Number of worker threads. Defaults to two (one + the main thread), "
          "to guarantee that we never spend more than half of our CPU time on "
          "statistical analysis: only the main thread runs the analysis code, "
          "while worker threads generate more data non-stop.");

bench::StableUniquePtr<const FindMinValueInstance> MakeFindMinValueInstance(
    size_t n) {
  using Backing = std::pair<FindMinValueInstance, std::vector<double>>;

  static thread_local std::unique_ptr<std::mt19937> rng;
  if (rng == nullptr) {
    std::random_device dev;
    rng.reset(new std::mt19937(dev()));
  }

  std::uniform_real_distribution<double> u(0, 1000);
  auto ret = absl::make_unique<Backing>();
  ret->second.resize(n);
  for (double& value : ret->second) {
    value = u(*rng);
  }

  ret->first.values = ret->second.data();
  ret->first.num_values = ret->second.size();
  return bench::MakeStableUniquePtr(&ret->first, std::move(ret));
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  const size_t num_values = absl::GetFlag(FLAGS_num_values);
  auto params =
      TestParams()
          .SetMaxComparisons(1000 * 1000ULL * 1000ULL)
          .SetOutlierLimit(1000, 5e-4)  // The typical case is < 100 cycles.
          .SetMinDfEffect(2.5e-3);

  if (absl::GetFlag(FLAGS_num_threads) > 1) {
    params.SetNumThreads(absl::GetFlag(FLAGS_num_threads));
  }

  if (absl::GetFlag(FLAGS_fn_a_lte)) {
    std::clog << "Testing if A <= B.\n";
    params.SetStopOnFirst(ComparisonResult::kAHigher);
  } else {
    std::clog << "Testing if A ~= B.\n";
  }

  const auto generator = [num_values] {
    return MakeFindMinValueInstance(num_values);
  };

  auto fns = [&] {
    if (absl::GetFlag(FLAGS_load_a_first)) {
      auto fn_a =
          bench::ExtractTimingFunction<std::tuple<size_t>,
                                       std::tuple<decltype(generator())>>(
              absl::GetFlag(FLAGS_lib_a), absl::GetFlag(FLAGS_fn_a));
      auto fn_b =
          bench::ExtractTimingFunction<std::tuple<size_t>,
                                       std::tuple<decltype(generator())>>(
              absl::GetFlag(FLAGS_lib_b), absl::GetFlag(FLAGS_fn_b));
      return std::make_pair(std::move(fn_a), std::move(fn_b));
    }

    auto fn_b = bench::ExtractTimingFunction<std::tuple<size_t>,
                                             std::tuple<decltype(generator())>>(
        absl::GetFlag(FLAGS_lib_b), absl::GetFlag(FLAGS_fn_b));
    auto fn_a = bench::ExtractTimingFunction<std::tuple<size_t>,
                                             std::tuple<decltype(generator())>>(
        absl::GetFlag(FLAGS_lib_a), absl::GetFlag(FLAGS_fn_a));
    return std::make_pair(std::move(fn_a), std::move(fn_b));
  }();

  auto fn_a = std::move(fns.first.first);
  auto fn_b = std::move(fns.second.first);

  // If the mean changes appreciably, something is definitely off.
  //
  // Allowing only 1 cycle of difference needs ~200M data points; 2
  // cycles is more reasonable, around 50M.
  const auto mean_result = CompareFunctions<BoundedMeanTest>(
      params.SetMinEffect(2), generator, fn_a, fn_b);

  // The KS statistics tracks the *maximum* deviation, so we could
  // have a very small probability at the tail deviate by a fair
  // amount of cycles, and KS would treat that the same as a deviation
  // by the same amount of cycles everywhere.
  //
  // The BoundedMean test already checks that the means are
  // comparable.  The KS test insures that we don't have a *huge*
  // difference only at the tail (e.g., a difference of 100 cycles at
  // the 99th percentile).
  const auto ks_result = CompareFunctions<KolmogorovSmirnovTest>(
      params.SetMinEffect(10), generator, fn_a, fn_b);

  // Only run quantiles test for additional debugging output on
  // inequality.
  //
  // Ideally, we'd want to treat the mean / KS tests are quick
  // pre-pass, and use a secondary, more interpretable, test based on
  // a ton of quantile CIs.  We don't have that latter test yet...
  // and, once we do, we might just find that computing a lot of
  // quantile CIs is faster than waiting for a conclusive KS result.
  if (mean_result.mean_result == ComparisonResult::kTie &&
      ks_result.result == ComparisonResult::kTie) {
    return 0;
  }

  QuantileTest quantile({1e-4, 0.5, 0.9, 0.95, 0.975, 0.99, 1.0 - 2.5e-3},
                        params.SetMinEffect(8));
  auto results = CompareFunctions(generator, fn_a, fn_b, &quantile);
  for (const auto& result : results) {
    if (absl::GetFlag(FLAGS_fn_a_lte)) {
      if (result.result != ComparisonResult::kALower &&
          result.result != ComparisonResult::kTie) {
        return 1;
      }
    } else {
      if (result.result != ComparisonResult::kTie) {
        return 1;
      }
    }
  }

  return 0;
}
