#ifndef BENCH_SIGN_TEST_
#define BENCH_SIGN_TEST_
#include <cmath>
#include <cstdint>
#include <ostream>
#include <tuple>

#include "absl/types/span.h"
#include "bench/test-params.h"

namespace bench {
// Tests whether A < B more than 50% of the time, or A > B more than
// 50% of the time, or whether both each happen less than 50% of the
// time.
//
// The default comparator compares runtimes, but any other scalar
// value could be compared.
//
// This class is thread-compatible.
class SignTest {
 public:
  class Comparator;

  explicit SignTest(TestParams params = TestParams());

  // The comparator returns 0 for a tie, -1 if A < B, 1 if A > B.
  Comparator comparator() const;

  TestParams params() const { return params_; }

  // `Observe` is called with a list of ints returned by `comparator()`.
  void Observe(absl::Span<const int> cmps);
  bool Done() const;

  struct Result {
    ComparisonResult result;
    // How often is A < B or A > B.
    double a_lower_ratio;
    double a_higher_ratio;
    // Total number of observations (including ties).
    uint64_t n_obs;
    // Confidence level.
    double level;

    bool operator==(const Result& other) const {
      return std::tie(result, a_lower_ratio, a_higher_ratio, n_obs, level) ==
             std::tie(other.result, other.a_lower_ratio, other.a_higher_ratio,
                      other.n_obs, other.level);
    }
  };

  Result Summary(std::ostream* out = nullptr) const;

 private:
  const TestParams params_;
  uint64_t total_observations_{0};
  uint64_t a_is_lower_{0};
  uint64_t a_is_higher_{0};
};

std::ostream& operator<<(std::ostream& out, const SignTest::Result& result);

class SignTest::Comparator {
 public:
  explicit Comparator(const TestParams& params) : params_(params) {}

  template <typename... T, typename... U, typename... V>
  int operator()(const std::tuple<uint64_t, T...>& a,
                 const std::tuple<uint64_t, U...>& b, const V&...) const {
    return (*this)(std::get<0>(a), std::get<0>(b));
  }

  int operator()(uint64_t a, uint64_t b) const {
    const double a_ticks = params_.Transform(a);
    const double b_ticks = b;
    const double delta = a_ticks - b_ticks;

    if (std::abs(delta) <= params_.min_effect) {
      return 0;
    }

    return (delta < 0) ? -1 : 1;
  }

 private:
  const TestParams& params_;
};

inline SignTest::Comparator SignTest::comparator() const {
  return Comparator(params_);
}
}  // namespace bench
#endif /* !BENCH_SIGN_TEST_ */
