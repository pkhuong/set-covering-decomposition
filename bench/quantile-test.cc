#include "bench/quantile-test.h"

#include <assert.h>

#include <cmath>
#include <map>

#include "absl/memory/memory.h"
#include "csm.h"

namespace bench {
namespace {
// We have a pair of confidence intervals for a given quantile.  If
// the max difference between the two intervals (the width of the
// union of the two intervsls) is at most `min_effect`, the distance
// between the two distribution quantiles is also (disregarding false
// positive that are known to be rare enough to satisfy the caller) at
// most `min_effect`.
//
// Similarly, if the two intervals are disjoint, and separated by more
// than `min_effect` (e.g., [a, b] < [c, d] with `c > b + min_effect`),
// the distance between the two distribution quantiles differs by more
// than `min_effect`, and we can conclude that A's quantile is either
// lower or higher than B's.
//
// Otherwise, we have two wide but similar intervals, and the analysis
// is inconclusive until we get more data.  We compute infinite
// sequences of confidence intervals, so waiting for more data before
// comparing the new intervals is sound.
ComparisonResult CompareRanges(const std::pair<double, double>& a,
                               const std::pair<double, double>& b,
                               double min_effect) {
  const double range =
      std::max(a.second, b.second) - std::min(a.first, b.first);
  if (range <= min_effect) {
    return ComparisonResult::kTie;
  }

  if (a.second + min_effect < b.first) {
    return ComparisonResult::kALower;
  }

  if (a.first > b.second + min_effect) {
    return ComparisonResult::kAHigher;
  }

  return ComparisonResult::kInconclusive;
}
}  // namespace

struct QuantileTest::Impl {
  Impl(absl::Span<const double> quantiles_in)
      : quantiles(quantiles_in.begin(), quantiles_in.end()) {}

  // Returns the interval `<values[index_lo], values[index_hi]>`,
  // with defaults to +/-infinity when out of bounds.
  static std::pair<double, double> QuantileInterval(
      const std::map<double, uint64_t>& values, uint64_t index_lo,
      uint64_t index_hi);

  // The list of quantiles to compare.
  const absl::FixedArray<double> quantiles;

  // These maps go from value to cardinality: we expect a lot of duplicates.
  //
  // TODO: we should use attributed binary search trees to search by
  // rank on the sum of cardinalities in log(n) time, but this is good
  // enough.
  std::map<double, uint64_t> a_values;
  std::map<double, uint64_t> b_values;
  size_t num_values{0};
};

/*static*/ std::pair<double, double> QuantileTest::Impl::QuantileInterval(
    const std::map<double, uint64_t>& values, uint64_t index_lo,
    uint64_t index_hi) {
  double min = -std::numeric_limits<double>::infinity();
  double max = std::numeric_limits<double>::infinity();

  uint64_t num_obs = 0;
  for (const auto& entry : values) {
    if (num_obs <= index_lo && index_lo < num_obs + entry.second) {
      min = entry.first;
    }

    if (num_obs <= index_hi && index_hi < num_obs + entry.second) {
      max = entry.first;
      break;
    }

    num_obs += entry.second;
  }

  return std::make_pair(min, max);
}

QuantileTest::QuantileTest(absl::Span<const double> quantiles,
                           TestParams params)
    // For each quantile, we'll compute confidence intervals for A and
    // for B, hence the 2x factor.
    : params_(params.SetLogEpsForNTests(2 * quantiles.size())),
      impl_(absl::make_unique<Impl>(quantiles)) {}

QuantileTest::~QuantileTest() = default;

void QuantileTest::Observe(
    absl::Span<const std::pair<double, double>> observations) {
  for (const auto& cycles : observations) {
    ++impl_->a_values[cycles.first];
    ++impl_->b_values[cycles.second];
    ++impl_->num_values;
  }
}

bool QuantileTest::Done() const {
  bool all_conclusive = true;
  const size_t n = impl_->num_values;
  for (const double quantile : impl_->quantiles) {
    // Get the rank of the low and high endpoints for the confidence
    // interval on the `quantile`'s value.
    const uint64_t index_lo =
        csm_quantile_index(n, quantile, -1, params_.log_eps);
    const uint64_t index_hi =
        csm_quantile_index(n, quantile, 1, params_.log_eps);

    const auto a_range =
        Impl::QuantileInterval(impl_->a_values, index_lo, index_hi);
    const auto b_range =
        Impl::QuantileInterval(impl_->b_values, index_lo, index_hi);

    const ComparisonResult result =
        CompareRanges(a_range, b_range, params_.min_effect);
    if (result == ComparisonResult::kInconclusive) {
      all_conclusive = false;
    }

    if (params_.stop_on_first.has_value() &&
        params_.stop_on_first.value() == result) {
      return true;
    }
  }

  return all_conclusive;
}

std::vector<QuantileTest::Result> QuantileTest::Summary(
    std::ostream* out) const {
  std::vector<QuantileTest::Result> ret;

  const size_t n = impl_->num_values;
  for (const double quantile : impl_->quantiles) {
    const uint64_t index_lo =
        csm_quantile_index(n, quantile, -1, params_.log_eps);
    const uint64_t index_hi =
        csm_quantile_index(n, quantile, 1, params_.log_eps);

    const auto a_range =
        Impl::QuantileInterval(impl_->a_values, index_lo, index_hi);
    const auto b_range =
        Impl::QuantileInterval(impl_->b_values, index_lo, index_hi);

    ret.push_back(Result{
        quantile, CompareRanges(a_range, b_range, params_.min_effect), a_range,
        b_range, n, params_.eps,
    });

    if (out != nullptr) {
      *out << ret.back() << ".\n";
    }
  }

  return ret;
}

std::ostream& operator<<(std::ostream& out,
                         const QuantileTest::Result& result) {
  out << "QuantileTest @ " << result.quantile << " " << result.result << ": a=["
      << result.a_range.first << ", " << result.a_range.second << "] "
      << "b=[" << result.b_range.first << ", " << result.b_range.second << "] "
      << "(n=" << result.n_obs << ", p < " << result.level << ")";
  return out;
}
}  // namespace bench
