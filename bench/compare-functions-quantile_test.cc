#include "bench/compare-functions.h"

#include <limits>
#include <vector>

#include "bench/quantile-test.h"
#include "bench/test-params.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "prng.h"

namespace bench {
namespace {
using ::testing::AnyOf;
using ::testing::Ne;
using ::testing::UnorderedElementsAre;
using ::testing::_;

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
    asm volatile("pause" ::: "memory");
  }
};

const auto FastNop = [] {};
__attribute__((__noinline__)) void NopCallee() { asm volatile(""); }
const auto Nop = [] { NopCallee(); };

TEST(CompareFunctionsQuantileTest, FastAA) {
  QuantileTest quantiles(
      {0.2, 0.5, 0.99},
      TestParams().SetMaxComparisons(1000000).SetNumThreads(1));

  const auto results = CompareFunctions(FastNop, FastNop, FastNop, &quantiles);
  EXPECT_EQ(results, quantiles.Summary());
  EXPECT_THAT(results[0].result,
              AnyOf(ComparisonResult::kInconclusive, ComparisonResult::kTie));
  EXPECT_THAT(results[1].result,
              AnyOf(ComparisonResult::kInconclusive, ComparisonResult::kTie));
  EXPECT_THAT(results[2].result,
              AnyOf(ComparisonResult::kInconclusive, ComparisonResult::kTie));
}

TEST(CompareFunctionsQuantileTest, FastAAWithMinEffect) {
  QuantileTest quantiles(
      {0.2, 0.5, 0.9},
      TestParams().SetMaxComparisons(10000000).SetNumThreads(1).SetMinEffect(
          3));

  const auto results = CompareFunctions(FastNop, FastNop, FastNop, &quantiles);
  EXPECT_EQ(results, quantiles.Summary());
  auto all_results = {results[0].result, results[1].result, results[2].result};
  EXPECT_THAT(all_results, UnorderedElementsAre(ComparisonResult::kTie,
                                                ComparisonResult::kTie, _));
}

TEST(CompareFunctionsQuantileTest, SlowAA) {
  QuantileTest quantiles(
      {0.2, 0.5, 0.99},
      TestParams().SetMaxComparisons(1000000).SetNumThreads(1).SetMinEffect(2));

  const auto results = CompareFunctions(Nop, Nop, Nop, &quantiles);
  EXPECT_EQ(results, quantiles.Summary());
  auto all_results = {results[0].result, results[1].result, results[2].result};
  EXPECT_THAT(all_results, UnorderedElementsAre(ComparisonResult::kTie,
                                                ComparisonResult::kTie, _));
}

TEST(CompareFunctionsQuantileTest, ALower) {
  QuantileTest quantiles({0.2, 0.5, 0.99},
                         TestParams()
                             .SetMaxComparisons(100000)
                             .SetMinEffect(3)
                             .SetNumThreads(1)
                             .SetStopOnFirst(ComparisonResult::kALower));

  xs256 prng;
  const auto results =
      CompareFunctions(Bernoulli(0.1), SometimesSlow(), Slow(), &quantiles);
  EXPECT_EQ(results, quantiles.Summary());
  EXPECT_THAT(results[0].result, AnyOf(ComparisonResult::kInconclusive,
                                       ComparisonResult::kALower));
  EXPECT_THAT(results[1].result, AnyOf(ComparisonResult::kInconclusive,
                                       ComparisonResult::kALower));
  EXPECT_THAT(results[2].result, Ne(ComparisonResult::kALower));
}

TEST(CompareFunctionsQuantileTest, AHigher) {
  QuantileTest quantiles({0.2, 0.5, 0.99},
                         TestParams()
                             .SetMaxComparisons(100000)
                             .SetMinEffect(3)
                             .SetNumThreads(1)
                             .SetStopOnFirst(ComparisonResult::kAHigher));

  const auto results =
      CompareFunctions(Bernoulli(0.1), Slow(), SometimesSlow(), &quantiles);
  EXPECT_EQ(results, quantiles.Summary());
  EXPECT_THAT(results[0].result, AnyOf(ComparisonResult::kInconclusive,
                                       ComparisonResult::kAHigher));
  EXPECT_THAT(results[1].result, AnyOf(ComparisonResult::kInconclusive,
                                       ComparisonResult::kAHigher));
  EXPECT_THAT(results[2].result, Ne(ComparisonResult::kAHigher));
}
}  // namespace
}  // namespace bench
