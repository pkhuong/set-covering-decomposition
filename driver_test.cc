#include "driver.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::ElementsAre;

// Two constraints, one iteration.
//
//   min x0 + x1 + 3 x2
// s.t.
//   x0    + x2 >= 1
//      x1 + x2 >= 1
//
// Optimal solution is [1, 1, 0], and found
// after the first iteration.
TEST(Driver, FirstIterationDone) {
  CoverConstraint constraints[] = {
      CoverConstraint({0, 2}), CoverConstraint({1, 2}),
  };

  const double costs[] = {1.0, 1.0, 3.0};
  DriverState state(costs);

  DriveOneIteration(absl::MakeSpan(constraints), &state);
  EXPECT_EQ(state.max_last_solution_infeasibility, 0.0);
  EXPECT_THAT(state.last_solution, ElementsAre(1.0, 1.0, 0.0));
  EXPECT_EQ(state.num_iterations, 1);
  EXPECT_EQ(state.sum_mix_gap, 0.0);
  EXPECT_EQ(state.prev_num_non_zero, 4);
  EXPECT_EQ(state.prev_min_loss, 0.0);
  EXPECT_EQ(state.prev_max_loss, 0.0);
  EXPECT_EQ(state.best_bound, 2.0);

  // First iteration: all 4 clone linking constraints have weight 1.
  //
  // -x0 - x1 - 2 x2 <= -2
  //
  // Minimised at [1, 1, 0].
  EXPECT_THAT(state.sum_solutions,
              ElementsAre(DoubleEq(1.0), DoubleEq(1.0), 0.0));
  EXPECT_THAT(state.sum_solution_value, DoubleEq(2.0));
  EXPECT_THAT(state.sum_solution_feasibility, DoubleEq(0.0));
}

// Two constraints, two iteration.
//
//   min x0 + x1 + x2
// s.t.
//   x0    + x2 >= 1
//      x1 + x2 >= 1
//
// Optimal solution is [0, 0, 1]
TEST(Driver, TwoIterations) {
  CoverConstraint constraints[] = {
      CoverConstraint({0, 2}), CoverConstraint({1, 2}),
  };

  const double costs[] = {1.0, 1.0, 1.0};
  DriverState state(costs);

  DriveOneIteration(absl::MakeSpan(constraints), &state);
  EXPECT_EQ(state.max_last_solution_infeasibility, 1.0);
  EXPECT_EQ(state.num_iterations, 1);
  // Mprev = 0
  // M = -1.0
  // loss = 0. -> 0 - (-1.0 - 0) = 1.0.
  EXPECT_EQ(state.sum_mix_gap, 1.0);
  EXPECT_EQ(state.prev_num_non_zero, 4);
  EXPECT_EQ(state.prev_min_loss, -1.0);
  EXPECT_EQ(state.prev_max_loss, 1.0);
  EXPECT_EQ(state.best_bound, 1.0);

  // First iteration: all 4 clone linking constraints have weight 1.
  //
  // -x0 - x1 - 2 x2 <= -2
  //
  // Minimised at [0, 0, 1].
  EXPECT_THAT(constraints[0].loss(), ElementsAre(-1.0, 1.0));
  EXPECT_THAT(constraints[1].loss(), ElementsAre(-1.0, 1.0));

  EXPECT_THAT(state.sum_solutions, ElementsAre(0.0, 0.0, DoubleEq(1.0)));
  EXPECT_THAT(state.sum_solution_value, DoubleEq(1.0));
  EXPECT_THAT(state.sum_solution_feasibility, DoubleEq(0.0));

  DriveOneIteration(absl::MakeSpan(constraints), &state);
  EXPECT_EQ(state.max_last_solution_infeasibility, 1.0);
  EXPECT_EQ(state.num_iterations, 2);

  // Eta = log(4).
  // Mprev = -1 - log([1 + exp(-2 log(4)) + 1 + exp(-2 log(4))] / 4)/log(4)
  //       = -0.54731
  // M = -1 - log([1 + exp(-log(4)) + exp(-log(4)) + exp(-log(4))]/4)/log(4)
  //   = -0.403677
  // loss = (1 - 1/8.0) / (1 + 1 + 1/8.0) -> 0 - (-1.0 - 0) = 1.0.

  {
    const double feasibility = (1 - 1 / 8.0) / (1 + 1 + 1 / 8.0);
    const double Mprev =
        -1 - std::log((2 + 2 * std::exp(-2 * std::log(4))) / 4) / std::log(4);
    const double M = -1 - std::log((1 + 3 / 4.0) / 4.0) / std::log(4.0);
    const double kMixGap = feasibility - (M - Mprev);
    EXPECT_THAT(state.sum_mix_gap, DoubleEq(1 + kMixGap));
  }

  EXPECT_EQ(state.prev_num_non_zero, 4);
  EXPECT_THAT(state.prev_min_loss, DoubleEq(-1.0));
  EXPECT_THAT(state.prev_max_loss, DoubleEq(0.0));
  EXPECT_EQ(state.best_bound, 1.0);

  // Second iteration: best loss is -1, step size is eta = log(4).
  // weights are:  x0   x1   x2
  //          c0  1.0        exp(-2 log 4)
  //          c1        1.0  exp(-2 log 4)
  //
  // Both constraints pick x2.
  //
  //   -x0 - x1 - 1/8 x2  <= -1/8
  //
  // We could stop at x0 = 1/8. However, we know that we have bound
  // at x = 1.0, so we'll stop at x0 = 1.0.
  //
  // Loss for c0 / x0 is 0. Loss for c0 / x2 is -1.
  // Loss for c1 / x1 is 1. Loss for c1 / x2 is -1.

  EXPECT_THAT(constraints[0].loss(), ElementsAre(-1.0, 0.0));
  EXPECT_THAT(constraints[1].loss(), ElementsAre(0.0, 0.0));

  EXPECT_THAT(state.sum_solutions,
              ElementsAre(0.0, DoubleEq(1.0), DoubleEq(1.0)));
  EXPECT_THAT(state.sum_solution_value, DoubleEq(2.0));
  EXPECT_THAT(state.sum_solution_feasibility,
              DoubleEq((1 - 1 / 8.0) / (1 + 1 + 1 / 8.0)));
}

// Two constraints, provide exact lower bound.
//
//   min x0 + x1 + 1.5x2
// s.t.
//   x0    + x2 >= 1
//      x1 + x2 >= 1
//
// Optimal solution is [0, 0, 1]
TEST(Driver, TwoIterationsWithExactLowerBound) {
  CoverConstraint constraints[] = {
      CoverConstraint({0, 2}), CoverConstraint({1, 2}),
  };

  const double costs[] = {1.0, 1.0, 1.5};
  DriverState state(costs);
  state.best_bound = 1.5;

  DriveOneIteration(absl::MakeSpan(constraints), &state);
  EXPECT_EQ(state.max_last_solution_infeasibility, 1.0);
  EXPECT_EQ(state.num_iterations, 1);
  EXPECT_EQ(state.sum_mix_gap, 1.0);
  EXPECT_EQ(state.prev_num_non_zero, 4);
  EXPECT_EQ(state.prev_min_loss, -1.0);
  EXPECT_EQ(state.prev_max_loss, 1.0);
  EXPECT_EQ(state.best_bound, 1.5);

  // First iteration: all 4 clone linking constraints have weight 1.
  //
  // -x0 - x1 - 2 x2 <= -2
  //
  // Minimised at [0, 0, 1].
  EXPECT_THAT(constraints[0].loss(), ElementsAre(-1.0, 1.0));
  EXPECT_THAT(constraints[1].loss(), ElementsAre(-1.0, 1.0));

  EXPECT_THAT(state.sum_solutions, ElementsAre(0.0, 0.0, DoubleEq(1.0)));
  EXPECT_THAT(state.sum_solution_value, DoubleEq(1.5));
  EXPECT_THAT(state.sum_solution_feasibility, DoubleEq(0.0));

  DriveOneIteration(absl::MakeSpan(constraints), &state);
  EXPECT_EQ(state.max_last_solution_infeasibility, 1.0);

  // Second iteration. Both constraints pick x2, but knapsack solution
  // is [0.5, -1, 0].
  //
  // Eta is log(4), weights are:
  //
  //      x0   x1   x2
  // c0   1        1 / 16
  // c1        1   1 / 16
  EXPECT_EQ(state.prev_min_loss, -0.5);
  EXPECT_EQ(state.prev_max_loss, 0.0);

  EXPECT_THAT(constraints[0].loss(), ElementsAre(-0.5, 0.0));
  EXPECT_THAT(constraints[1].loss(), ElementsAre(0.0, 0.0));

  EXPECT_THAT(state.sum_solutions, ElementsAre(0.5, 1.0, 1.0));
  EXPECT_THAT(state.sum_solution_value, DoubleEq(3.0));
  EXPECT_THAT(state.sum_solution_feasibility,
              DoubleEq((1.5 - 1 / 8.0) / (2 + 1 / 8.0)));

  const double kDelta = state.sum_mix_gap;
  DriveOneIteration(absl::MakeSpan(constraints), &state);
  EXPECT_EQ(state.max_last_solution_infeasibility, 1.0);
  EXPECT_EQ(state.prev_min_loss, -1.0);
  EXPECT_EQ(state.prev_max_loss, 0.5);

  // Third iteration. First constraint picks x2, second x1.
  //
  // Knapsack goes for [1, 0, 1/3].
  //
  //  Weights are
  //
  //     x0  x1                    x2
  // c0  1                         exp(-log(4)/2kDelta)
  // c1      exp(-log(4)/2kDelta)  exp(-log(4)/2kDelta)
  EXPECT_THAT(constraints[0].loss(), ElementsAre(0.5, DoubleEq(-2.0 / 3)));
  EXPECT_THAT(constraints[1].loss(), ElementsAre(-1.0, DoubleEq(1.0 / 3)));

  EXPECT_THAT(state.sum_solutions,
              ElementsAre(1.5, 1.0, DoubleEq(1.0 + 1.0 / 3)));
  EXPECT_THAT(state.sum_solution_value, DoubleEq(4.5));

  const double kWeight = std::exp(-std::log(4) / (2 * kDelta));
  EXPECT_THAT(
      state.sum_solution_feasibility,
      DoubleNear((1.5 - 1 / 8.0) / (2 + 1 / 8.0) +
                     (1 + 2.0 / 3 * kWeight - 2 * kWeight) / (1 + 3 * kWeight),
                 1e-6));
}
