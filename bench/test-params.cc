#include "bench/test-params.h"

#include <cmath>

namespace bench {
std::ostream& operator<<(std::ostream& out, ComparisonResult result) {
  switch (result) {
    case ComparisonResult::kInconclusive:
      out << "Inconclusive";
      break;
    case ComparisonResult::kTie:
      out << "Tie";
      break;
    case ComparisonResult::kALower:
      out << "A < B";
      break;
    case ComparisonResult::kAHigher:
      out << "A > B";
      break;
    case ComparisonResult::kDifferent:
      out << "A <> B";
      break;
  }

  return out;
}

TestParams StrictTestParams() {
  TestParams ret;
  ret.confirm_done = 0;
  ret.retry_after_thread_cancel = false;
  return ret;
}

TestParams& TestParams::SetLogEpsForNTests(size_t n) {
  if (n == 0) {
    log_eps = 0;
  } else {
    log_eps = std::log(eps / n);
  }

  return *this;
}
}  // namespace bench
