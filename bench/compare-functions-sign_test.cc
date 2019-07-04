#include "bench/compare-functions.h"

#include <tuple>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "bench/sign-test.h"
#include "bench/test-params.h"
#include "bench/wrap-function.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
const auto FastNop = [] {};

// This slower Nop compiles to a direct call to `NopCallee`.  That
// introduces a bit more noise, but we should still get a useful
// result.
__attribute__((__noinline__)) void NopCallee() { asm volatile(""); }
const WRAP_FUNCTION(NopCallee) Nop;

TEST(CompareFunctionsSignTest, FastAA) {
  SignTest test(
      StrictTestParams().SetMaxComparisons(10000000).SetNumThreads(1));

  const auto result = CompareFunctions(FastNop, FastNop, FastNop, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}

TEST(CompareFunctionsSignTest, SlowAA) {
  SignTest test(
      StrictTestParams().SetMaxComparisons(10000000).SetNumThreads(1));

  const auto result = CompareFunctions(Nop, Nop, Nop, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}

TEST(CompareFunctionsSignTest, ALtB) {
  SignTest test(StrictTestParams().SetMaxComparisons(1000000).SetNumThreads(1));

  const auto result =
      CompareFunctions(FastNop, FastNop, [] { asm volatile("pause"); }, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.result, ComparisonResult::kALower);
}

TEST(CompareFunctionsSignTest, AGtB) {
  SignTest test(StrictTestParams().SetMaxComparisons(1000000).SetNumThreads(1));

  const auto result =
      CompareFunctions(FastNop, [] { asm volatile("pause"); }, FastNop, &test);
  EXPECT_EQ(result, test.Summary());
  EXPECT_EQ(result.result, ComparisonResult::kAHigher);
}
}  // namespace
}  // namespace bench
