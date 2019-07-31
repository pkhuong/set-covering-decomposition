#include "bench/bounded-mean-test.h"

#include <cmath>

#include "csm.h"
#include "martingale-cs.h"

namespace bench {
BoundedMeanTest::BoundedMeanTest(TestParams params)
    // 5 tests: 1 for the outlier rate comparison, 4 for 2x 2-sided
    // martingales on the mean.
    : params_(params.SetLogEpsForNTests(5)) {}

void BoundedMeanTest::Observe(
    absl::Span<const std::pair<double, double>> cycles) {
  const double outlier_limit = params_.outlier_limit;

  n_obs_ += cycles.size();
  for (const auto entry : cycles) {
    const double a = entry.first;
    const double b = entry.second;

    if (a > outlier_limit || b > outlier_limit) {
      if (a > outlier_limit) {
        ++a_outlier_;
      }

      if (b > outlier_limit) {
        ++b_outlier_;
      }
    } else {
      a_sum_ += a;
      b_sum_ += b;
      ++num_summands_;
    }
  }
}

bool BoundedMeanTest::Done() const {
  const Result result = Summary();
  if (params_.stop_on_first.has_value()) {
    const ComparisonResult wanted = params_.stop_on_first.value();
    if (wanted == ComparisonResult::kTie) {
      // kTie only makes sense for the sign test itself, not for the
      // outliers, which begin at a tie.
      if (result.mean_result == ComparisonResult::kTie) {
        return true;
      }
    } else if (result.mean_result == wanted ||
               result.outlier_result == wanted) {
      return true;
    }
  }

  return result.mean_result != ComparisonResult::kInconclusive &&
         result.outlier_result != ComparisonResult::kInconclusive;
}

BoundedMeanTest::Result BoundedMeanTest::Summary(std::ostream* out) const {
  Result ret;

  const double inv_num_summands = 1.0 / std::max<uint64_t>(1, num_summands_);

  ret.a_mean = a_sum_ * inv_num_summands;
  ret.b_mean = b_sum_ * inv_num_summands;
  ret.n_mean_obs = num_summands_;

  if (num_summands_ == 0) {
    ret.mean_result = ComparisonResult::kInconclusive;
    ret.mean_slop = params_.outlier_limit;
  } else {
    // The martingale threshold is on the sum of values, not the
    // sample average.  Divide by num_summands_ to convert it to a
    // confidence interval width on the mean.
    const double threshold =
        martingale_cs_threshold_span(num_summands_, params_.min_count,
                                     params_.outlier_limit, params_.log_eps);

    // This slop is the amount by which we must increase or decrease
    // our empirical mean to contain the actual value with enough
    // probability to satisfy the `exp(log_eps)` false positive rate.
    const double slop = threshold * inv_num_summands;
    ret.mean_slop = slop;

    const double min_diff = params_.min_effect;
    if (ret.a_mean - slop > ret.b_mean + min_diff + slop) {
      ret.mean_result = ComparisonResult::kAHigher;
    } else if (ret.a_mean + slop < ret.b_mean - min_diff - slop) {
      ret.mean_result = ComparisonResult::kALower;
    } else if (std::abs(ret.a_mean - ret.b_mean) + 2 * slop < min_diff) {
      ret.mean_result = ComparisonResult::kTie;
    } else {
      ret.mean_result = ComparisonResult::kInconclusive;
    }
  }

  {
    const double inv_n_obs = 1.0 / std::max<uint64_t>(1, n_obs_);
    ret.a_outlier_ratio = inv_n_obs * a_outlier_;
    ret.b_outlier_ratio = inv_n_obs * b_outlier_;
    ret.total_obs = n_obs_;
  }

  // We don't perform a statistical test here, in order to not dilute
  // our power too much.  In theory, a result of Tie might also be
  // inconclusive, or Lower / Higher, but for our use case, we mostly
  // care that outliers are much rarer than `min_outlier_ratio`.
  if (ret.a_outlier_ratio <= params_.min_outlier_ratio &&
      ret.b_outlier_ratio <= params_.min_outlier_ratio) {
    ret.outlier_result = ComparisonResult::kTie;
  } else if (csm(a_outlier_ + b_outlier_, 0.5, a_outlier_, params_.log_eps,
                 nullptr) == 0) {
    ret.outlier_result = ComparisonResult::kInconclusive;
  } else if (a_outlier_ < b_outlier_) {
    ret.outlier_result = ComparisonResult::kALower;
  } else {
    ret.outlier_result = ComparisonResult::kAHigher;
  }

  ret.level = params_.eps;
  if (out != nullptr) {
    *out << ret << ".\n";
  }

  return ret;
}

std::ostream& operator<<(std::ostream& out,
                         const BoundedMeanTest::Result& result) {
  out << "BoundedMeanTest " << result.mean_result << ": A=" << result.a_mean
      << ", B=" << result.b_mean << " +/- " << result.mean_slop
      << " (n=" << result.n_mean_obs << ") -- outliers "
      << 100 * result.a_outlier_ratio << "% " << result.outlier_result << " "
      << 100 * result.b_outlier_ratio << "% (n=" << result.total_obs << " p < "
      << result.level << ")";
  return out;
}
}  // namespace bench
