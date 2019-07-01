#include "bench/sign-test.h"

#include "csm.h"

namespace bench {
SignTest::SignTest(TestParams params) : params_(params.SetLogEpsForNTests(2)) {}

void SignTest::Observe(absl::Span<const int> cmps) {
  total_observations_ += cmps.size();
  for (const int cmp : cmps) {
    a_is_lower_ += (cmp < 0) ? 1 : 0;
    a_is_higher_ += (cmp > 0) ? 1 : 0;
  }
}

bool SignTest::Done() const {
  return Summary().result != ComparisonResult::kInconclusive;
}

SignTest::Result SignTest::Summary(std::ostream* out) const {
  double lower_level;
  const bool lower_conclusive =
      csm(total_observations_, 0.5, a_is_lower_, params_.log_eps, &lower_level);
  double higher_level;
  const bool higher_conclusive = csm(total_observations_, 0.5, a_is_higher_,
                                     params_.log_eps, &higher_level);

  const double inv_total = 1.0 / std::max<uint64_t>(1, total_observations_);

  Result result = {
      ComparisonResult::kInconclusive,
      inv_total * a_is_lower_,
      inv_total * a_is_higher_,
      total_observations_,
      std::exp(lower_level) + std::exp(higher_level),
  };

  if (result.a_lower_ratio < 0.5 && result.a_higher_ratio < 0.5 &&
      lower_conclusive && higher_conclusive) {
    result.result = ComparisonResult::kTie;
  } else if (result.a_lower_ratio > 0.5 && lower_conclusive) {
    result.result = ComparisonResult::kALower;
  } else if (result.a_higher_ratio > 0.5 && higher_conclusive) {
    result.result = ComparisonResult::kAHigher;
  }

  if (out != nullptr) {
    (*out) << result << ".\n";
  }

  return result;
}

std::ostream& operator<<(std::ostream& out, const SignTest::Result& result) {
  out << "SignTest " << result.result << " lower=" << result.a_lower_ratio
      << " higher=" << result.a_higher_ratio << " (n = " << result.n_obs
      << ", p ~ " << result.level << ")";
  return out;
}
}  // namespace bench
