#include "bench/sign-test.h"

#include <cstdint>
#include <iostream>
#include <random>

#include "bench/test-params.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
// Generate integer U[0, 10]. We should be able to reject both </> hypotheses.
TEST(SignTest, Equal) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 10);

  SignTest test(TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const int cmps[] = {
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

TEST(SignTest, Lower) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(0, 10);
  std::uniform_int_distribution<uint64_t> u_b(1, 11);

  SignTest test(TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const int cmps[] = {
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
  EXPECT_EQ(result.result, ComparisonResult::kALower);
}

TEST(SignTest, Higher) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u_a(1, 11);
  std::uniform_int_distribution<uint64_t> u_b(0, 10);

  SignTest test(TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const int cmps[] = {
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

// When the values span a much large range, this doesn't work anymore.
TEST(SignTest, EqualContinuous) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 1000 * 1000 * 1000ULL);

  SignTest test(TestParams().SetMinEffect(0));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < 100; ++j) {
      const int cmps[] = {
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

// Unless... we artificially create "equal enough" observations.
TEST(SignTest, EqualContinuousRange) {
  std::random_device dev;
  std::mt19937 rng(dev());

  std::uniform_int_distribution<uint64_t> u(0, 1000 * 1000 * 1000ULL);

  SignTest test(TestParams().SetMinEffect(10 * 1000 * 1000ULL));
  const auto comparator = test.comparator();
  for (size_t i = 0; i < 10000; ++i) {
    std::vector<int> cmps;

    cmps.reserve(100);
    for (size_t j = 0; j < 100; ++j) {
      cmps.push_back(
          comparator(std::make_tuple(u(rng)), std::make_tuple(u(rng))));
    }

    test.Observe(cmps);
    if (test.Done()) {
      break;
    }
  }

  EXPECT_TRUE(test.Done());
  const auto result = test.Summary(&std::cout);
  EXPECT_EQ(result.result, ComparisonResult::kTie);
}
}  // namespace
}  // namespace bench
