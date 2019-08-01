#include "kolmogorov-smirnov-test.h"

#include <cmath>
#include <limits>

#include "absl/algorithm/container.h"
#include "one-sided-ks.h"

namespace bench {
KolmogorovSmirnovTest::KolmogorovSmirnovTest(TestParams params)
    // 2x sample/distribution 2-sided tests and 1x sample/sample
    // 2-sided test.
    : params_(params.SetLogEpsForNTests(3)) {}

KolmogorovSmirnovTest::~KolmogorovSmirnovTest() = default;

void KolmogorovSmirnovTest::Observe(
    absl::Span<const std::pair<double, double>> observations) {
  num_observations_ += observations.size();
  for (const auto& entry : observations) {
    ++a_counts[entry.first];
    ++b_counts[entry.second];
  }
}

bool KolmogorovSmirnovTest::Done() const {
  const Result result = Summary();
  if (result.result == ComparisonResult::kInconclusive) {
    return false;
  }

  if (result.result_holds_everywhere) {
    return true;
  }

  return params_.stop_on_first.has_value() &&
         params_.stop_on_first.value() == result.result;
}

namespace {
// Converts a map from value to occurrence to a vector of CDF, given
// scale = 1.0 / sum[counts] (i.e., computes the prefix sum of counts
// * scale).
std::vector<std::pair<double, double>> CountsToCdf(
    const std::map<double, uint64_t>& counts, double scale) {
  std::vector<std::pair<double, double>> ret;

  ret.reserve(counts.size());
  uint64_t acc = 0;
  for (const auto& entry : counts) {
    acc += entry.second;
    ret.emplace_back(entry.first, std::min(scale * acc, 1.0));
  }

  return ret;
}

// Returns the location loc that minimises cdf_x[loc] -
// cdf_(y + offset)[loc] = cdf_y[loc - offset], along with that
// difference.
//
// If the delta is positive, x is always lower than y + offset.
// If the delta is negative, x is sometimes higher than y + offset.
//
// Skips the max `min_outlier_ratio` values.  Differences in the tail
// that far are usually just noise.
std::pair<double, double> OneSidedDistributionDistance(
    absl::Span<const std::pair<double, double>> x_cdf,
    absl::Span<const std::pair<double, double>> y_cdf, double offset,
    double min_outlier_ratio) {
  const auto cmp_first = [offset](const std::pair<double, double>& y,
                                  double x) { return y.first + offset < x; };

  const double max_cumulative_freq = 1.0 - min_outlier_ratio;
  double best_loc = 0.0;
  double best_delta = std::numeric_limits<double>::infinity();
  for (const auto& entry : x_cdf) {
    // We don't really care what happens here. Either y has less mass
    // here, so we'd find a positive delta, and it doesn't matter how
    // positive it is (we cap the delta to be non-negative), or y has
    // more mass, but that means it's all outliers and we shouldn't
    // care about them.
    if (entry.second >= max_cumulative_freq) {
      break;
    }

    // We want the mass in y that's strictly less than x - offset.
    // There's no difference in continuous distributions, but, in our
    // discrete world, we want to know how much more mass y has at
    // values definitely lower than x - offset.
    double y_probability;
    {
      auto it = absl::c_lower_bound(y_cdf, entry.first, cmp_first);
      if (it == y_cdf.begin()) {
        y_probability = 0.0;
      } else {
        --it;
        y_probability = it->second;
      }
    }

    const double delta = entry.second - y_probability;
    if (delta < best_delta) {
      best_loc = entry.first;
      best_delta = delta;
    }
  }

  return std::make_pair(best_loc, best_delta);
}
}  // namespace

KolmogorovSmirnovTest::Result KolmogorovSmirnovTest::Summary(
    std::ostream* out) const {
  Result ret;

  ret.result = ComparisonResult::kInconclusive;
  ret.result_holds_everywhere = false;
  ret.n_obs = num_observations_;
  ret.level = params_.eps;

  if (num_observations_ == 0 || num_observations_ < params_.min_count) {
    ret.lower_location = 0;
    ret.lower_delta = 0;
    ret.higher_location = 0;
    ret.higher_delta = 0;
    if (out != nullptr) {
      *out << ret << ".\n";
    }

    return ret;
  }

  const auto a_cdf = CountsToCdf(a_counts, 1.0 / num_observations_);
  const auto b_cdf = CountsToCdf(b_counts, 1.0 / num_observations_);

  // If the delta > 0, we have that a is always less than b + min_effect
  // Otherwise, we found a location where a > b + min_effect.
  const auto lower_delta = OneSidedDistributionDistance(
      a_cdf, b_cdf, params_.min_effect, params_.min_outlier_ratio);

  // Check if sometimes, b > a + min_effect, or if always, b < a +
  // min_effect.
  const auto higher_delta = OneSidedDistributionDistance(
      b_cdf, a_cdf, params_.min_effect, params_.min_outlier_ratio);

  ret.lower_location = lower_delta.first;
  ret.lower_delta = lower_delta.second;
  ret.higher_location = higher_delta.first;
  ret.higher_delta = higher_delta.second;

  // This threshold is the maximum (expected) distance between the
  // empirical CDF and the underlying DF, multiplied by 2 for A and B.
  //
  // If we have empirical CDFs that always match, we can expect the
  // underlying DFs to differ by at most 2 * one_sided_ks_threshold.
  // If that value is less than the min_df_effect, we're done.
  //
  // More generally, we subtract the min_df_effect, and compare the
  // max mismatch with the adjusted value.
  const double distrib_eq_threshold =
      2 * one_sided_ks_distribution_threshold(
              ret.n_obs, params_.min_count, params_.log_eps + one_sided_ks_eq) -
      params_.min_df_effect;

  // This is a regular two-sided KS test, where we try to reject the
  // null hypothesis that A and B are equal (+/- min_effect).
  const double two_sample_eq_threshold =
      params_.min_df_effect +
      one_sided_ks_pair_threshold(ret.n_obs, params_.min_count,
                                  params_.log_eps + one_sided_ks_eq);

  // A is always <= B if the min difference isn't so small that the
  // two actual DFs might crossover by more than min_df_effect.
  //
  // We need to take the min with 0.0 because there are some values at
  // which the empirical CDFs are identicals (e.g., -infty or +infty).
  const bool always_lte =
      std::min(0.0, lower_delta.second) > distrib_eq_threshold;
  const bool always_gte =
      std::min(0.0, higher_delta.second) > distrib_eq_threshold;
  if (always_lte && always_gte) {
    ret.result_holds_everywhere = true;
    ret.result = ComparisonResult::kTie;
  } else {
    const bool sometimes_gt = -lower_delta.second > two_sample_eq_threshold;
    const bool sometimes_lt = -higher_delta.second > two_sample_eq_threshold;

    if (sometimes_gt && sometimes_lt) {
      ret.result = ComparisonResult::kDifferent;
      ret.result_holds_everywhere = true;
    } else if (sometimes_gt) {
      ret.result = ComparisonResult::kAHigher;
      ret.result_holds_everywhere = always_gte;
    } else if (sometimes_lt) {
      ret.result = ComparisonResult::kALower;
      ret.result_holds_everywhere = always_lte;
    } else {
      ret.result = ComparisonResult::kInconclusive;
      ret.result_holds_everywhere = false;
    }
  }

  if (out != nullptr) {
    *out << "KS thresholds " << distrib_eq_threshold << ", "
         << two_sample_eq_threshold << ".\n";
    *out << ret << ".\n";
  }

  return ret;
}

std::ostream& operator<<(std::ostream& out,
                         const KolmogorovSmirnovTest::Result& result) {
  const char* qualifier;

  switch (result.result) {
    case ComparisonResult::kInconclusive:
    case ComparisonResult::kTie:
    case ComparisonResult::kDifferent:
      qualifier = "";
      break;

    case ComparisonResult::kALower:
    case ComparisonResult::kAHigher:
      if (result.result_holds_everywhere) {
        qualifier = "always ";
      } else {
        qualifier = "sometimes ";
      }
      break;
  }

  out << "KolmogorovSmirnovTest " << qualifier << result.result
      << ": <delta=" << result.lower_delta << " @ " << result.lower_location
      << ", >delta=" << result.higher_delta << " @ " << result.higher_location
      << " (n=" << result.n_obs << ", p < " << result.level << ")";
  return out;
}
}  // namespace bench
