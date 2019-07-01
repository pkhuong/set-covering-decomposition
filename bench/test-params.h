#ifndef BENCH_TEST_PARAMS_H
#define BENCH_TEST_PARAMS_H
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>

#include "absl/types/optional.h"

namespace bench {
// Result for one statistical test.
enum class ComparisonResult {
  // Not enough data to conclude anything.
  kInconclusive = 0,

  // A and B are equal, within tolerance defined in the `TestParams`.
  kTie = 1,

  // A is definitely lower (faster) than B.
  kALower = 2,

  // A is definitely higher (slower) than B.
  kAHigher = 3,

  // A definitely neither lower (faster) or higher (slower) than B: a
  // multidimensional comparison finds A both lower and higher than B
  // in different locations.
  kDifferent = 4,
};

std::ostream& operator<<(std::ostream& out, ComparisonResult result);

struct TestParams {
  explicit TestParams() = default;

  // Sets the false positive rate for the analysis.  When a
  // statistical test performs multiple tests, it will correct
  // internally.
  TestParams& SetEps(double eps_) {
    eps = eps_;
    return *this;
  }

  // Sets the early stopping condition when a test checks for multiple
  // conditions simultaneously.  For example, setting this to
  // `kAHigher` would stop as soon as one condtion found that A was
  // slower than B.
  TestParams& SetStopOnFirst(ComparisonResult result) {
    if (result == ComparisonResult::kInconclusive) {
      stop_on_first.reset();
    } else {
      stop_on_first = result;
    }
    return *this;
  }

  // Resets the early stopping condition to its default value: only
  // stop when all the tests have a conclusive result, or the
  // iteration limit has been reached.
  TestParams& ClearStopOnFirst(ComparisonResult result) {
    stop_on_first.reset();
    return *this;
  }

  // Sets the minimum effect size in terms of the value measured
  // (e.g., clock cycles) for A to differ from B.  In practice, we can
  // almost always reject the null hypothesis, we just need a ton of
  // data points.  Setting the minimum effect lets us treat small
  // differences as irrelevant, even if statistically significant.
  //
  // Setting this to, e.g., 3 will declare a Tie when A and B
  // definitely differ by less than 3 cycles, and will only declare A
  // faster than B if it's faster by at least 3 cycles.  See each
  // statistical test for details.
  //
  // There is no hard rule to determine the correct value for this
  // parameters. If you're comparing CPU cycle quantiles, and would
  // disregard a difference of 2 cycles in any quantile, then
  // `min_effect` should be at least 2. If you'd definitely look into
  // a difference of 5 cycles, `min_effect` should be less than 5.
  //
  // In general, `min_effect` should be higher for continuous
  // regression testing, where we want to alert on obvious changes
  // without drowning in false positives, and lower for one-off checks
  // that a given method performs better than another.
  TestParams& SetMinEffect(double min_effect_) {
    min_effect = min_effect_;
    return *this;
  }

  // Sets the minimum absolute effect size (a value in [0, 1] on the
  // probability values in the distribution function for A and B.
  //
  // This is useful for tests on the distribution function like the
  // Kolmogorov-Smirnov test.  In practice, we will always see a
  // difference in CDF once we have enough data. This parameter lets
  // us accept DFs as "similar enough" when their max difference is
  // negligible.
  //
  // Setting this to, e.g., 0.01 will declare a Tie when the
  // cumulative density for A and B differ by at most 0.01 (e.g., the
  // CDF at 30 cycles might be 0.91 and 0.92, which is close enough to
  // identical).  A statistical test on the distribution function will
  // only claim that A has more mass at or below a given value when
  // the difference between A and B is greater than `min_df_effect`,
  // for example if 91% of observations for A are at or below 30
  // cycles, and 89% for B.
  //
  // There is no hard rule to determine the correct value for this
  // parameter.  If you're running a comparison, and would ignore a
  // difference in CDF on the order of 0.1% (90% of observations are
  // at or below 100 cycles, vs 90.1%), `min_df_effect` should be at
  // least 0.001.  If, on the other hand, you would investigate a
  // discrepancy of 1%, `min_df_effect` should be less than 0.01.
  //
  // In general, `min_df_effect` should be higher for continuous
  // regression testing, where we want to alert on obvious changes
  // without drowning in false positives, and lower for one-off checks
  // that a given method performs better than another.
  TestParams& SetMinDfEffect(double min_df_effect_) {
    min_df_effect = min_df_effect_;
    return *this;
  }

  // Sets the minimum number of observations to gather before
  // executing any statistical test.  Since the tests are
  // statistically sound, increasing this value doesn't help avoid
  // noise, but does improve the rate of convergence (with
  // log(min_count), so large values don't help that much).
  TestParams& SetMinCount(uint64_t min_count_) {
    min_count = min_count_;
    return *this;
  }

  // Observations for `a` are transformed into `a_scale * x + a_offset`.
  TestParams& SetScale(double a_scale_) {
    a_scale = a_scale_;
    return *this;
  }

  TestParams& SetOffset(double a_offset_) {
    a_offset = a_offset_;
    return *this;
  }

  // Some tests need a bound on what "normal" observations look like.
  // Values above `limit` will be considered as outliers, and the test
  // will compare outliers only for their appearance rate.
  //
  // If the `ratio` is provided, it also sets the minimum ratio of
  // "interesting" outliers.  If outliers for both A and B are less
  // frequent than `ratio`, their outlier rates are tied.
  TestParams& SetOutlierLimit(double limit) {
    return SetOutlierLimit(limit, min_outlier_ratio);
  }

  TestParams& SetOutlierLimit(double limit, double ratio) {
    outlier_limit = limit;
    min_outlier_ratio = ratio;
    return *this;
  }

  // Sets the maximum number of comparisons to perform before aborting
  // with an inconclusive result.
  TestParams& SetMaxComparisons(uint64_t max_comparisons_) {
    max_comparisons = max_comparisons_;
    return *this;
  }

  // Sets the maximum number of threads (including the calling thread)
  // used to generate data.
  TestParams& SetNumThreads(uint64_t num_threads_) {
    num_threads = num_threads_;
    return *this;
  }

  // Set `log_eps` to `ln(eps)` after a Bonferroni correction for `n`
  // tests.
  TestParams& SetLogEpsForNTests(size_t n);

  // Transforms `x -> a_scale x + a_offset`.
  double Transform(double x) const { return a_scale * x + a_offset; }

  // These values might be used in comparators. Move them here to
  // avoid false sharing.
  double a_scale{1.0};
  double a_offset{0.0};
  double min_effect{0};
  double min_df_effect{0};

  uint64_t max_comparisons{std::numeric_limits<uint64_t>::max()};
  uint64_t num_threads{1};
  double eps{1e-6};
  double log_eps{0};
  absl::optional<ComparisonResult> stop_on_first;
  uint64_t min_count{1000};
  double outlier_limit{std::numeric_limits<double>::infinity()};
  double min_outlier_ratio{0};
};
}  // namespace bench
#endif /*!BENCH_TEST_PARAMS_H */
