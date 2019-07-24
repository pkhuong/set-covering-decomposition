#include "bench/compare-libraries.h"

#include "bench/bounded-mean-test.h"
#include "bench/kolmogorov-smirnov-test.h"
#include "bench/quantile-test.h"
#include "bench/sign-test.h"
#include "bench/test-params.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
using ::testing::UnorderedElementsAre;
using ::testing::_;

const auto FastNop = [] {};

// The different test cases make sure we can open a bunch of different shared
// objects, an covers the various overloads for `CompareLibraries`.
TEST(CompareLibraries, SignTestFastAA) {
  SignTest test(
      StrictTestParams().SetMaxComparisons(10000000).SetNumThreads(1));

  const auto result =
      CompareLibraries(FastNop, {"bench/libfastnop.so", "MakeNop"},
                       {"bench/libfastnop.so", "MakeNop"}, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}

TEST(CompareLibraries, BoundedMeanSlowAA) {
  const auto result = CompareLibraries<BoundedMeanTest>(
      StrictTestParams()
          .SetMaxComparisons(10000000)
          .SetStopOnFirst(ComparisonResult::kTie)
          .SetMinEffect(3)
          .SetOutlierLimit(100, 1e-4)
          .SetNumThreads(1),
      FastNop, {"bench/libslownop.so", "MakeNop"},
      {"bench/libslownop.so", "MakeNop"});
  EXPECT_EQ(result.mean_result, ComparisonResult::kTie);
}

TEST(CompareLibraries, QuantileFastAAWithMinEffect) {
  QuantileTest quantiles({0.2, 0.5, 0.99},
                         StrictTestParams()
                             .SetMaxComparisons(1000000)
                             .SetMinEffect(3)
                             .SetNumThreads(1));

  const auto results =
      CompareLibraries(FastNop, {"bench/libfastnop.so", "MakeNop"},
                       {"bench/libfastnop.so", "MakeNop"}, &quantiles);
  auto all_results = {results[0].result, results[1].result, results[2].result};
  EXPECT_THAT(all_results, UnorderedElementsAre(ComparisonResult::kTie,
                                                ComparisonResult::kTie, _));
}

TEST(CompareLibraries, KSSlowAASufficientMinEffectToDetectTie) {
  const auto result = CompareLibraries<KolmogorovSmirnovTest>(
      StrictTestParams()
          .SetMaxComparisons(10000000)
          .SetMaxComparisons(10000000)
          .SetNumThreads(1)
          // Looser MinEffect and MinDfEffect means
          // we can ignore noise more aggressively,
          // which lets use consistently find ties.
          .SetMinEffect(5)
          .SetMinDfEffect(2e-2),
      FastNop, {"bench/libslownop.so", "MakeNop"},
      {"bench/libslownop.so", "MakeNop"});
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}

TEST(CompareLibraries, SignTestBadAAShouldWarnAtCompileTime) {
  SignTest test(
      StrictTestParams().SetMaxComparisons(10000000).SetNumThreads(1));

  const auto result = CompareLibraries<std::tuple<int, int>>(
      FastNop, {"bench/libbadnop.so", "MakeNop"},
      {"bench/libbadnop.so", "MakeNop"}, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}

TEST(CompareLibraries, SignTestManyBadAA) {
  // dlmopen can't deal with too many linkmaps.  However, let's make sure
  // we don't also leak shared object handles by opening and re-closing
  // the same shared object a ton of times.
  for (size_t i = 0; i < 100; ++i) {
    SignTest test(
        StrictTestParams().SetMaxComparisons(10000000).SetNumThreads(1));

    const auto result = CompareLibraries<std::tuple<int, int>>(
        FastNop, {"bench/libbadnop.so", "MakeNop"},
        {"bench/libbadnop.so", "MakeNop"}, &test);
    EXPECT_EQ(result, test.Summary());
    EXPECT_EQ(result.result, ComparisonResult::kTie);
  }
}
}  // namespace
}  // namespace bench
