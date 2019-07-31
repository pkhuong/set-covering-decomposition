#ifndef BENCH_BOUNDED_MEAN_TEST_H
#define BENCH_BOUNDED_MEAN_TEST_H
#include <cstdint>
#include <ostream>
#include <tuple>

#include "absl/types/span.h"
#include "bench/test-params.h"

namespace bench {
// Tests whether, when A and B are within bounds (0 <= value <= outlier_limit),
// the means meaningfully differ, or, when the empirical ratio of out
// of bounds observations is higher than min_outlier_ratio, whether
// their ratio of out of bounds observations differ.
//
// This test is relatively weak, especially when the outlier_limit is
// high.  Only use this when the average is definitely what matters,
// for example, when comparing the throughput of routines that will be
// used in aggregate, and when we expect the response time of the
// routines to be affected in different ways by their inputs.  For
// such a comparison to be valid, we must also have a good idea of the
// distribution of inputs, and know that the production system will be
// able to detect and abort outliers.
//
// This class is thread-compatible.
class BoundedMeanTest {
 public:
  class Comparator;

  explicit BoundedMeanTest(TestParams params = TestParams());

  Comparator comparator() const;

  TestParams params() const { return params_; }

  void Observe(absl::Span<const std::pair<double, double>> cycles);
  bool Done() const;

  struct Result {
    ComparisonResult mean_result;
    double a_mean;
    double b_mean;
    double mean_slop;
    uint64_t n_mean_obs;
    ComparisonResult outlier_result;
    double a_outlier_ratio;
    double b_outlier_ratio;
    uint64_t total_obs;
    double level;

    bool operator==(const Result& other) const {
      return std::tie(mean_result, a_mean, b_mean, mean_slop, n_mean_obs,
                      outlier_result, a_outlier_ratio, b_outlier_ratio,
                      total_obs, level) ==
             std::tie(other.mean_result, other.a_mean, other.b_mean,
                      other.mean_slop, other.n_mean_obs, other.outlier_result,
                      other.a_outlier_ratio, other.b_outlier_ratio,
                      other.total_obs, other.level);
    }
  };

  Result Summary(std::ostream* out = nullptr) const;

 private:
  const TestParams params_;
  double a_sum_{0};
  double b_sum_{0};
  uint64_t num_summands_{0};
  uint64_t a_outlier_{0};
  uint64_t b_outlier_{0};
  uint64_t n_obs_{0};
};

std::ostream& operator<<(std::ostream& out,
                         const BoundedMeanTest::Result& result);

class BoundedMeanTest::Comparator {
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

inline BoundedMeanTest::Comparator BoundedMeanTest::comparator() const {
  return Comparator(params_);
}
}  // namespace bench
#endif /* !BENCH_BOUNDED_MEAN_TEST_H */
