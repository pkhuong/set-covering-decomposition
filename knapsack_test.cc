#include "knapsack.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::ElementsAre;

static constexpr double kEps = 1e-10;

TEST(Knapsack, Empty) {
  BigVecArenaContext ctx;

  EXPECT_EQ(SolveKnapsack({}, {}, 0, kEps, 0),
            KnapsackSolution({}, 0, 0, true));

  EXPECT_EQ(SolveKnapsack({}, {}, -1, kEps, 0),
            KnapsackSolution({}, 0, 0, false));

  EXPECT_EQ(SolveKnapsack({}, {}, 1, kEps, 0),
            KnapsackSolution({}, 0, 1, true));

  EXPECT_EQ(SolveKnapsack({}, {}, 0, kEps, -1),
            KnapsackSolution({}, 0, 0, true));

  EXPECT_EQ(SolveKnapsack({}, {}, 1, kEps, -1),
            KnapsackSolution({}, 0, 1, true));

  EXPECT_EQ(SolveKnapsack({}, {}, 1, kEps, 1),
            KnapsackSolution({}, 0, 1, true));
}

TEST(Knapsack, Singleton) {
  BigVecArenaContext ctx;

  // Take the whole thing to hit feasibility.
  EXPECT_EQ(SolveKnapsack({1}, {-2}, -2, kEps, 0),
            KnapsackSolution({1.0}, 1, 0, true));
  EXPECT_EQ(SolveKnapsack({1}, {-2}, -2, kEps, -10),
            KnapsackSolution({1.0}, 1, 0, true));
  // Take the whole thing to hit our lower bound.
  EXPECT_EQ(SolveKnapsack({1}, {-2}, 0, kEps, 1),
            KnapsackSolution({1.0}, 1, 2, true));

  // Take half to achieve feasibility, regardless of what the lower bound says.
  EXPECT_EQ(SolveKnapsack({1}, {-2}, -1, kEps, 0),
            KnapsackSolution({0.5}, 0.5, 0, true));
  EXPECT_EQ(SolveKnapsack({1}, {-2}, -1, kEps, -10),
            KnapsackSolution({0.5}, 0.5, 0, true));

  // Take half to hit our lower bound.
  EXPECT_EQ(SolveKnapsack({1}, {-2}, 0, kEps, 0.5),
            KnapsackSolution({0.5}, 0.5, 1, true));

  EXPECT_EQ(SolveKnapsack({1}, {-2}, 0, kEps, 0),
            KnapsackSolution({0}, 0, 0, true));
  EXPECT_EQ(SolveKnapsack({1}, {-2}, -3, kEps, 0),
            KnapsackSolution({}, 0, 0, false));
}

TEST(Knapsack, Sign) {
  BigVecArenaContext ctx;

  // Doesn't matter what the lower bound says: we always take these, they're
  // also good for feasibility.
  EXPECT_EQ(SolveKnapsack({-1}, {-2}, 0, kEps, -10),
            KnapsackSolution({1.0}, -1, 2, true));
  EXPECT_EQ(SolveKnapsack({-1}, {-2}, -1, kEps, -0.5),
            KnapsackSolution({1.0}, -1, 1, true));

  // Sometimes, we take the item and it's still not enough for feasibility.
  EXPECT_EQ(SolveKnapsack({-1}, {-2}, -10, kEps, -10),
            KnapsackSolution({}, 0, 0, false));

  // But if we add a less interesting item, we can make it, regardless of
  // the lower bound.
  EXPECT_EQ(SolveKnapsack({-1, 10}, {-2, -8}, -8, kEps, -10),
            KnapsackSolution({1, 0.75}, 6.5, 0, true));

  // and even oversatisfy if the lower bound is high enough.
  EXPECT_EQ(SolveKnapsack({-1, 10}, {-2, -8}, -8, kEps, 8.5),
            KnapsackSolution({1, 0.95}, 8.5, 1.6, true));

  EXPECT_EQ(SolveKnapsack({-1, 10}, {-2, -8}, -8, kEps, 20),
            KnapsackSolution({1, 1}, 9, 2, true));
}

TEST(Knapsack, TrivialOnWeight) {
  // Two items
  //
  // value   5 1
  // weight  -2 -4
  //
  // The second element is much more interesting.

  std::vector<double> values = {5, 1};
  std::vector<double> weights = {-2, -4};
  // Set the RHS binding to <= -2. The best we can do
  // is to take half of the second item.
  const auto result = SolveKnapsack(values, weights, -2, kEps, 0);
  EXPECT_TRUE(result.feasible);
  EXPECT_THAT(result.solution, ElementsAre(0, 0.5));
  EXPECT_EQ(result.objective_value, 0.5);
  EXPECT_EQ(result.feasibility, 0);
}

TEST(Knapsack, TrivialOnValue) {
  // Two items
  //
  // value   5 1
  // weight  -2 -4
  //
  // The second element is much more interesting.

  std::vector<double> values = {4, 1};
  std::vector<double> weights = {-2, -4};
  // Set the lower bound binding to >= 2. We should take the
  // second element and 1/4th of the first.
  const auto result = SolveKnapsack(values, weights, -1, kEps, 2);
  EXPECT_TRUE(result.feasible);
  EXPECT_THAT(result.solution, ElementsAre(0.25, 1));
  EXPECT_EQ(result.objective_value, 2);
  EXPECT_EQ(result.feasibility, 3.5);
}

TEST(Knapsack, TrivialOnValue2) {
  // Two items
  //
  // value   5 1
  // weight  -2 -4
  //
  // The second element is much more interesting.

  std::vector<double> values = {4, 1};
  std::vector<double> weights = {-2, -4};
  // Set the lower bound binding to >= 2. We should take the
  // second element and 1/4th of the first.
  const auto result = SolveKnapsack(values, weights, 1, kEps, 2);
  EXPECT_TRUE(result.feasible);
  EXPECT_THAT(result.solution, ElementsAre(0.25, 1));
  EXPECT_EQ(result.objective_value, 2);
  EXPECT_EQ(result.feasibility, 5.5);
}
