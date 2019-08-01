#include "kolmogorov-smirnov-test.h"

#include <cstdint>
#include <iostream>
#include <random>

#include "bench/test-params.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
// Generate integers in U[0, 10]. We statistically can't detect strict
// equality.
TEST(KSTest, EqualTie) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 10);

  KolmogorovSmirnovTest test(TestParams().SetMinEffect(0).SetMinCount(1000));
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
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.result, ComparisonResult::kInconclusive);
}

// But we can detect equality +/- 1.
TEST(KSTest, Equal) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 10);

  KolmogorovSmirnovTest test(
      TestParams().SetMinEffect(1).SetMinDfEffect(1e-2).SetMinCount(1000));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
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
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}

TEST(KSTest, EqualShiftedByOne) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 10);

  KolmogorovSmirnovTest test(
      TestParams().SetMinEffect(1 + 1e-6).SetMinDfEffect(1e-2).SetMinCount(
          1000));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
    for (size_t j = 0; j < 1000; ++j) {
      const std::pair<double, double> cmps[] = {
          comparator(std::make_tuple(u(rng)), std::make_tuple(u(rng) + 1)),
      };

      test.Observe(cmps);
    }

    if (test.Done()) {
      break;
    }
  }

  EXPECT_TRUE(test.Done());
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}

TEST(KSTest, Lower) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(0, 10);
  std::uniform_int_distribution<uint64_t> u_b(3, 13);

  KolmogorovSmirnovTest test(TestParams().SetMinEffect(1).SetMinCount(1000));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 100000; ++i) {
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

  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.result, ComparisonResult::kALower);
}

TEST(KSTest, Higher) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(3, 13);
  std::uniform_int_distribution<uint64_t> u_b(0, 10);

  KolmogorovSmirnovTest test(
      TestParams().SetMinEffect(1).SetMinDfEffect(1e-2).SetMinCount(1000));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 100000; ++i) {
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
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.result, ComparisonResult::kAHigher);
}

TEST(KSTest, Different) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(0, 13);
  std::uniform_int_distribution<uint64_t> u_b(3, 10);

  KolmogorovSmirnovTest test(
      TestParams().SetMinEffect(1).SetMinDfEffect(1e-2).SetMinCount(1000));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
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
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.result, ComparisonResult::kDifferent);
}
}  // namespace
}  // namespace bench
