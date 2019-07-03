#include "bench/bounded-mean-test.h"

#include <cstdint>
#include <iostream>
#include <random>

#include "bench/test-params.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
using ::testing::AnyOf;

TEST(BoundedMeanTest, Equal) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 100);

  BoundedMeanTest test(TestParams().SetMinEffect(1).SetOutlierLimit(100));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const std::pair<double, double> cmps[] = {
          comparator(std::make_tuple(u(rng)), std::make_tuple(u(rng))),
      };

      test.Observe(cmps);
    }

    if (test.Done()) {
      break;
    }
  }

  EXPECT_TRUE(test.Done());
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.mean_result, ComparisonResult::kTie);
  EXPECT_EQ(result.outlier_result, ComparisonResult::kTie);
}

TEST(BoundedMeanTest, ALower) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(0, 98);
  std::uniform_int_distribution<uint64_t> u_b(2, 100);

  BoundedMeanTest test(TestParams().SetMinEffect(1).SetOutlierLimit(100));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const std::pair<double, double> cmps[] = {
          comparator(std::make_tuple(u_a(rng)), std::make_tuple(u_b(rng))),
      };

      test.Observe(cmps);
    }

    if (test.Done()) {
      break;
    }
  }

  EXPECT_TRUE(test.Done());
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.mean_result, ComparisonResult::kALower);
  EXPECT_EQ(result.outlier_result, ComparisonResult::kTie);
}

TEST(BoundedMeanTest, AHigher) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(2, 100);
  std::uniform_int_distribution<uint64_t> u_b(0, 98);

  BoundedMeanTest test(TestParams().SetMinEffect(1).SetOutlierLimit(100));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const std::pair<double, double> cmps[] = {
          comparator(std::make_tuple(u_a(rng)), std::make_tuple(u_b(rng))),
      };

      test.Observe(cmps);
    }

    if (test.Done()) {
      break;
    }
  }

  EXPECT_TRUE(test.Done());
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.mean_result, ComparisonResult::kAHigher);
  EXPECT_EQ(result.outlier_result, ComparisonResult::kTie);
}

TEST(BoundedMeanTest, EqualWithOutliers) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 102);

  BoundedMeanTest test(
      TestParams().SetMinEffect(1).SetOutlierLimit(100, 1e-4).SetStopOnFirst(
          ComparisonResult::kTie));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const std::pair<double, double> cmps[] = {
          comparator(std::make_tuple(u(rng)), std::make_tuple(u(rng))),
      };

      test.Observe(cmps);
    }

    if (test.Done()) {
      break;
    }
  }

  EXPECT_TRUE(test.Done());
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.mean_result, ComparisonResult::kTie);
  EXPECT_EQ(result.outlier_result, ComparisonResult::kInconclusive);
}

TEST(BoundedMeanTest, EqualWithAOutliers) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(0, 101);
  std::uniform_int_distribution<uint64_t> u_b(0, 100);

  BoundedMeanTest test(
      TestParams().SetMinEffect(1).SetOutlierLimit(100, 1e-4).SetStopOnFirst(
          ComparisonResult::kAHigher));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const std::pair<double, double> cmps[] = {
          comparator(std::make_tuple(u_a(rng)), std::make_tuple(u_b(rng))),
      };

      test.Observe(cmps);
    }

    if (test.Done()) {
      break;
    }
  }

  EXPECT_TRUE(test.Done());
  const auto result = test.Summary(&std::cout);
  EXPECT_THAT(result.mean_result,
              AnyOf(ComparisonResult::kInconclusive, ComparisonResult::kTie));
  EXPECT_EQ(result.outlier_result, ComparisonResult::kAHigher);
}
}  // namespace
}  // namespace bench
