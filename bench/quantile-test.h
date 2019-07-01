#ifndef BENCH_QUANTILE_TEST_
#define BENCH_QUANTILE_TEST_
#include <cstdint>
#include <memory>
#include <ostream>
#include <tuple>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/types/span.h"
#include "bench/test-params.h"

namespace bench {
// Compares a set of quantiles for A and B, pairwise (e.g., the median
// of A with the median of B, and the 99th percentile of A with that
// of B).  For each quantile, the confidence intervals for that
// quantile of A and B can be tied (equal +/- min effect), disjoint,
// in which case A < B or A > B, or overlapping while spanning a wide
// range.  In the last case, the test is inconclusive, and may need
// access to more data points.
//
// By default this test only stops once the comparison has a
// conclusive result for each quantile.  It's also possible to stop
// early when one quantile yields the result passed to
// `TestParams::SetStopOnFirst` (e.g., if A if the proposed new
// version, one might want to stop as soon as A is found slower than B
// for even one quantile).
//
// This class is thread-compatible.
class QuantileTest {
 public:
  class Comparator;

  explicit QuantileTest(absl::Span<const double> quantiles,
                        TestParams params = TestParams());

  ~QuantileTest();

  Comparator comparator() const;

  TestParams params() const { return params_; }

  void Observe(absl::Span<const std::pair<double, double>> observations);
  bool Done() const;

  struct Result {
    double quantile;
    ComparisonResult result;
    std::pair<double, double> a_range;
    std::pair<double, double> b_range;
    uint64_t n_obs;
    double level;

    bool operator==(const Result& other) const {
      return std::tie(quantile, result, a_range, b_range, n_obs, level) ==
             std::tie(other.quantile, other.result, other.a_range,
                      other.b_range, other.n_obs, other.level);
    }
  };

  std::vector<Result> Summary(std::ostream* out = nullptr) const;

 private:
  struct Impl;

  const TestParams params_;
  // Hide the boost-ey implementation behind a pointer.
  std::unique_ptr<Impl> impl_;
};

std::ostream& operator<<(std::ostream& out, const QuantileTest::Result& result);

class QuantileTest::Comparator {
 public:
  explicit Comparator(const TestParams& params) : params_(params) {}

  template <typename... T, typename... U, typename... V>
  std::pair<double, double> operator()(const std::tuple<uint64_t, T...>& a,
                                       const std::tuple<uint64_t, U...>& b,
                                       const V&...) const {
    return (*this)(std::get<0>(a), std::get<0>(b));
  }

  std::pair<double, double> operator()(uint64_t a, uint64_t b) const {
    return {params_.Transform(a), b};
  }

 private:
  const TestParams& params_;
};

inline QuantileTest::Comparator QuantileTest::comparator() const {
  return Comparator(params_);
}
}  // namespace bench
#endif /* !BENCH_QUANTILE_TEST_ */
