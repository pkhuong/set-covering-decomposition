#include "bench/quantile-test.h"

#include <cstdint>
#include <iostream>
#include <random>

#include "bench/test-params.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
// Generate integer U[0, 10]. We should quiclky detect equality.
TEST(QuantileTest, Equal) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 10);

  // A/A compare the median and the 95th percentile.
  QuantileTest test({0.5, 0.95}, TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 1000; ++j) {
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
  const auto results = test.Summary(&std::cout);
  EXPECT_EQ(results[0].result, ComparisonResult::kTie);
  EXPECT_EQ(results[1].result, ComparisonResult::kTie);
}

TEST(QuantileTest, Lower) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(0, 10);
  std::uniform_int_distribution<uint64_t> u_b(1, 11);

  // Compare the median of `u_a` with that of `u_b`, and the 95th
  // percentile of `u_a` with that of `u_b`.
  QuantileTest test({0.5, 0.95}, TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 1000; ++j) {
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
  const auto results = test.Summary(&std::cout);
  EXPECT_EQ(results[0].result, ComparisonResult::kALower);
  EXPECT_EQ(results[1].result, ComparisonResult::kALower);
}

TEST(QuantileTest, Higher) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(1, 11);
  std::uniform_int_distribution<uint64_t> u_b(0, 10);

  QuantileTest test({0.5, 0.95}, TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 1000; ++j) {
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
  const auto results = test.Summary(&std::cout);
  EXPECT_EQ(results[0].result, ComparisonResult::kAHigher);
  EXPECT_EQ(results[1].result, ComparisonResult::kAHigher);
}

// We can't conclude anything when two continuous distributions are very
// similar.
TEST(QuantileTest, EqualContinuous) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 1000 * 1000 * 1000ULL);

  QuantileTest test({0.5, 0.95}, TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 1000; ++j) {
      const std::pair<double, double> cmps[] = {
          comparator(std::make_tuple(u(rng)), std::make_tuple(u(rng))),
      };

      test.Observe(cmps);
    }

    if (test.Done()) {
      break;
    }
  }

  EXPECT_FALSE(test.Done());
  const auto results = test.Summary(&std::cout);
  EXPECT_EQ(results[0].result, ComparisonResult::kInconclusive);
  EXPECT_EQ(results[1].result, ComparisonResult::kInconclusive);
}

// Unles we create equal enough observations.
TEST(QuantileTest, EqualContinuousRange) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 1000 * 1000 * 1000ULL);

  QuantileTest test({0.5, 0.95},
                    TestParams().SetMinEffect(100 * 1000 * 1000ULL));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 1000; ++j) {
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
  const auto results = test.Summary(&std::cout);
  EXPECT_EQ(results[0].result, ComparisonResult::kTie);
  EXPECT_EQ(results[1].result, ComparisonResult::kTie);
}

}  // namespace
}  // namespace bench
