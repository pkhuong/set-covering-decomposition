#ifndef BENCH_KOLMOGOROV_SMIRNOV_TEST_
#define BENCH_KOLMOGOROV_SMIRNOV_TEST_
#include <cstdint>
#include <map>
#include <ostream>
#include <tuple>

#include "absl/types/span.h"
#include "bench/test-params.h"

namespace bench {
// This class performs a pair of two-sided Kolmogorov-Smirnov goodness
// of fit tests.
//
// The more classical use case rejects the hypothesis that A and B are
// the same +/- some min effect.  We can compare the CDFs at all
// points, and determine when they differ so much that they can't come
// from the same underlying hypothesis.  The min effect here is used
// to offset the comparison.  For example, we compare A at 10 cycles
// with B at 10 + min_effect cycles. If the cumulative frequency for A
// at 10 cycles is much higher than that for B at 10 + min_effect, we
// can conclude that A is lower (faster) than B, and not just with a
// small effect.  We want to set a minimum on the effect size because
// we're practically assured that there is *a* difference, however
// negligible, between two alternatives (or even between calls to
// identical functions, given the complexity of contemporary
// microarchitectures.  Without a bound on theminimum effect size of
// interest, statistical tests become tests of patience: if one is
// patient enough, they will always find a statistically significant
// result, even without p-hacking.
//
// The KS test compares cumulative probabilities, so we also use the
// `MinDfEffect`: if the probability for A at a given value differs
// from B but negligibly (e.g., 95% VS 96%), we disregard a
// statistically significant difference.
//
// Once we have this `MinDfEffect` in place, we can also determine
// that two distributions are similar enough: at some point, our
// estimate of the worst-case difference between the empirical CDFs
// and the underlying CDFs is so small that similar enough CDFs
// guarantee that the underlying DFs are also similar enough to
// positively detect a tie.
class KolmogorovSmirnovTest {
 public:
  class Comparator;

  explicit KolmogorovSmirnovTest(TestParams params = TestParams());

  ~KolmogorovSmirnovTest();

  // The comparator must not outlive `this` test, which must also not
  // move.
  Comparator comparator() const;

  const TestParams& params() const { return params_; }

  void Observe(absl::Span<const std::pair<double, double>> observations);
  bool Done() const;

  struct Result {
    ComparisonResult result;
    // Is this result true everywhere, or only in (at least) a single
    // point?
    //
    // If A > B, but `result_holds_everywhere` is false, we know that
    // A is sometimes > B, and thus can reject A <= B.  However, it is
    // possible that A < B somewhere else.  If, on the other hand.
    // `result_holds_everywhere` is true, then the evidence leads us
    // to believe that A is *always* higher (slower) than B.
    bool result_holds_everywhere;

    // Location with the strongest effect for A < B.  That effect is
    // the difference in the empirical CDF between A and B -
    // params_.min_effect, at lower_location.
    //
    // If lower_delta > 0, we have evidence that A is always lower
    // (has more mass at lower values modulo min_effect) than B.
    // Otherwise, we have evidence that A is sometimes higher than B.
    double lower_location;
    double lower_delta;

    // Location with the strongest effect for A > B.  If higher_delta
    // > 0, we have evidence that A is always higher than B.
    // Otherwise, we have evidence that A is sometimes lower than B.
    double higher_location;
    double higher_delta;

    uint64_t n_obs;
    // Confidence level on the result, e.g., `p < level = 1e-6`.
    double level;

    bool operator==(const Result& other) const {
      return std::tie(result, result_holds_everywhere, lower_location,
                      lower_delta, higher_location, higher_delta, n_obs,
                      level) ==
             std::tie(other.result, other.result_holds_everywhere,
                      other.lower_location, other.lower_delta,
                      other.higher_location, other.higher_delta, other.n_obs,
                      other.level);
    }
  };

  Result Summary(std::ostream* out = nullptr) const;

 private:
  const TestParams params_;

  uint64_t num_observations_{0};
  // TODO: consider gtl::btree_map
  std::map<double, uint64_t> a_counts;
  std::map<double, uint64_t> b_counts;
};

class KolmogorovSmirnovTest::Comparator {
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

inline KolmogorovSmirnovTest::Comparator KolmogorovSmirnovTest::comparator()
    const {
  return Comparator(params_);
}

std::ostream& operator<<(std::ostream& out,
                         const KolmogorovSmirnovTest::Result& result);
}  // namespace bench
#endif /* !BENCH_KOLMOGOROV_SMIRNOV_TEST_ */
