#include "bench/compare-functions.h"

#include <limits>
#include <vector>

#include "bench/kolmogorov-smirnov-test.h"
#include "bench/test-params.h"
#include "bench/wrap-function.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "prng.h"

namespace bench {
namespace {
using ::testing::AnyOf;
using ::testing::Ne;

class Bernoulli {
 public:
  explicit Bernoulli(double p) : p_(std::numeric_limits<uint64_t>::max() * p) {}

  Bernoulli(const Bernoulli& other) : p_(other.p_) {}

  bool operator()() { return prng_() < p_; }

 private:
  uint64_t p_;
  xs256 prng_;
};

struct SometimesSlow {
  __attribute__((__noinline__)) void operator()(bool slow) const {
    if (slow) {
      asm volatile("pause" ::: "memory");
      asm volatile("pause" ::: "memory");
      asm volatile("pause" ::: "memory");
      asm volatile("pause" ::: "memory");
    }
  }
};

struct Slow {
  __attribute__((__noinline__)) void operator()(bool) const {
    asm volatile("pause" ::: "memory");
  }
};

// FastNop will be inlined into nothingness.  That's the trivial A/A
// case.
const auto FastNop = [] {};

// Nop will inline an immediate call to `NopCallee`, which will remain
// out of line. That gives us a slightly more realistic A/A test case.
__attribute__((__noinline__)) void NopCallee() { asm volatile(""); }
const WRAP_FUNCTION(NopCallee) Nop;

TEST(CompareFunctionsKSTest, FastAA) {
  KolmogorovSmirnovTest test(StrictTestParams()
                                 .SetMaxComparisons(100000)
                                 .SetNumThreads(1)
                                 .SetMinEffect(2)
                                 .SetMinDfEffect(1e-3));

  const auto result = CompareFunctions(FastNop, FastNop, FastNop, &test);
  EXPECT_EQ(result, test.Summary());
  // With such aggressive minimum effect sizes, we'd need 100M+
  // iterations to consistently reach a kTie result.
  //
  // The next test case shows how loosening the minimum effect size
  // lets us find ties.
  EXPECT_THAT(result.result,
              AnyOf(ComparisonResult::kInconclusive, ComparisonResult::kTie));
}

TEST(CompareFunctionsKSTest, FastAASufficientMinEffectToDetectTie) {
  KolmogorovSmirnovTest test(StrictTestParams()
                                 .SetMaxComparisons(10000000)
                                 .SetNumThreads(1)
                                 // Looser MinEffect and MinDfEffect means
                                 // we can ignore noise more aggressively,
                                 // which lets use consistently find ties.
                                 .SetMinEffect(5)
                                 .SetMinDfEffect(2e-2));

  const auto result = CompareFunctions(FastNop, FastNop, FastNop, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_THAT(result.result, ComparisonResult::kTie);
}

// Slower `Nop` calls are slightly more challenging, since they
// introduce more noise.
TEST(CompareFunctionsKSTest, SlowAA) {
  KolmogorovSmirnovTest test(StrictTestParams()
                                 .SetMaxComparisons(100000)
                                 .SetNumThreads(1)
                                 .SetMinEffect(2)
                                 .SetMinDfEffect(1e-3));

  const auto result = CompareFunctions(Nop, Nop, Nop, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_THAT(result.result,
              AnyOf(ComparisonResult::kInconclusive, ComparisonResult::kTie));
}

TEST(CompareFunctionsKSTest, SlowAASufficientMinEffectToDetectTie) {
  KolmogorovSmirnovTest test(StrictTestParams()
                                 .SetMaxComparisons(10000000)
                                 .SetNumThreads(1)
                                 // Looser MinEffect and MinDfEffect means
                                 // we can ignore noise more aggressively,
                                 // which lets use consistently find ties.
                                 .SetMinEffect(5)
                                 .SetMinDfEffect(2e-2));

  const auto result = CompareFunctions(Nop, Nop, Nop, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_THAT(result.result, ComparisonResult::kTie);
}

TEST(CompareFunctionsKSTest, ALower) {
  KolmogorovSmirnovTest test(StrictTestParams()
                                 .SetMaxComparisons(1000000)
                                 .SetNumThreads(1)
                                 .SetMinEffect(1)
                                 .SetStopOnFirst(ComparisonResult::kALower));

  const auto result =
      CompareFunctions(Bernoulli(0.1), SometimesSlow(), Slow(), &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_THAT(result.result,
              AnyOf(ComparisonResult::kDifferent, ComparisonResult::kALower));
}

TEST(CompareFunctionsKSTest, AHigher) {
  KolmogorovSmirnovTest test(StrictTestParams()
                                 .SetMaxComparisons(10000000)
                                 .SetNumThreads(1)
                                 .SetMinEffect(1)
                                 .SetStopOnFirst(ComparisonResult::kAHigher));

  const auto result =
      CompareFunctions(Bernoulli(0.1), Slow(), SometimesSlow(), &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_THAT(result.result,
              AnyOf(ComparisonResult::kDifferent, ComparisonResult::kAHigher));
}
}  // namespace
}  // namespace bench
