#include "bench/compare-functions.h"

#include <cmath>

#include "bench/bounded-mean-test.h"
#include "bench/test-params.h"
#include "bench/wrap-function.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
using ::testing::AnyOf;
using ::testing::Ne;

const auto FastNop = [] {};
__attribute__((__noinline__)) void NopCallee() { asm volatile(""); }
const WRAP_FUNCTION(NopCallee) Nop;

TEST(CompareFunctionsBoundedMeanTest, FastAA) {
  BoundedMeanTest test(StrictTestParams()
                           .SetMaxComparisons(10000000)
                           .SetStopOnFirst(ComparisonResult::kTie)
                           .SetMinEffect(3)
                           .SetOutlierLimit(100, 1e-4)
                           .SetNumThreads(1));

  const auto results = CompareFunctions(FastNop, FastNop, FastNop, &test);
  EXPECT_EQ(results, test.Summary());
  EXPECT_THAT(results.mean_result, ComparisonResult::kTie);
}

TEST(CompareFunctionsBoundedMeanTest, SlowAA) {
  BoundedMeanTest test(StrictTestParams()
                           .SetMaxComparisons(10000000)
                           .SetStopOnFirst(ComparisonResult::kTie)
                           .SetMinEffect(3)
                           .SetOutlierLimit(100, 1e-4)
                           .SetNumThreads(1));

  const auto results = CompareFunctions(Nop, Nop, Nop, &test);
  EXPECT_EQ(results, test.Summary());
  EXPECT_THAT(results.mean_result, ComparisonResult::kTie);
}

TEST(CompareFunctionsBoundedMeanTest, AB) {
  BoundedMeanTest test(StrictTestParams()
                           .SetMaxComparisons(10000000)
                           .SetMinEffect(3)
                           .SetOutlierLimit(200, 1e-4)
                           .SetNumThreads(1));

  const auto result = CompareFunctions(
      FastNop, FastNop,
      []() __attribute__((__noinline__)) {
        long rax = 42;
        long rdx = 45;
        long rcx = 100000;
        // We don't use pause here: the mean is a fundamentally
        // unstable statistic, and the pause instruction seems to
        // cause too many outliers when hyperthreading is enabled.
        asm volatile("divq %2" : "+a"(rax), "+d"(rdx), "+c"(rcx));
      },
      &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.mean_result, ComparisonResult::kALower);
}

TEST(CompareFunctionsBoundedMeanTest, Outliers) {
  BoundedMeanTest test(StrictTestParams()
                           .SetMaxComparisons(1000000)
                           .SetStopOnFirst(ComparisonResult::kALower)
                           .SetMinEffect(0)
                           .SetOutlierLimit(100, 1e-4)
                           .SetNumThreads(1));

  const auto result =
      CompareFunctions([] { return 42.0; }, [](double) { return; },
                       [](double x) __attribute__((__noinline__)) {
                         for (size_t i = 0; i < 20; ++i) {
                           asm volatile("pause" : "+r"(i)::"memory");
                         }
                       },
                       &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.outlier_result, ComparisonResult::kALower);
}
}  // namespace
}  // namespace bench
