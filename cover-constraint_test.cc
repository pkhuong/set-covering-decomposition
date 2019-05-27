#include "cover-constraint.h"

#include <limits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::AnyOf;
using ::testing::DoubleEq;
using ::testing::ElementsAre;

// Tests are a bit sucky because the interaction with constraints is
// extremely stateful. However, given that constraints are an
// implementation detail for a loop where the statefulness doesn't
// really leak out of each iteration, I think this is better than
// adding test-only abstraction breaks.

TEST(CoverConstraint, FirstIteration) {
  CoverConstraint constraint({0, 3, 1});

  PrepareWeightsState prep_state(4, 0.0,
                                 std::numeric_limits<double>::infinity());

  // All tours are equal; the current implementation will use tour 0,
  // but that's an implementation detail.
  constraint.PrepareWeights(&prep_state);
  EXPECT_EQ(constraint.last_solution(), 0);

  EXPECT_EQ(prep_state.mix_loss.num_weights, 3);
  EXPECT_EQ(prep_state.mix_loss.sum_weights, 3.0);
  EXPECT_THAT(prep_state.knapsack_weights, ElementsAre(-1.0, -1.0, 0.0, -1.0));
  EXPECT_EQ(prep_state.knapsack_rhs, -1);

  // Let's say we picked 1 arbitrarily.
  std::vector<double> knapsack_solution = {0.1, 1.0, 1.0, 0.0};
  ObserveLossState loss_state(knapsack_solution);

  constraint.ObserveLoss(&loss_state);
  // loss = satisfaction. The forcing constraint for tour 0 was
  // violated by 0.9, that for tour 1 was satisfied by 1, and the
  // last tour, tour 3, was satisfied at equality.
  EXPECT_THAT(constraint.loss(), ElementsAre(-0.9, 1.0, 0));
  EXPECT_EQ(loss_state.min_loss, -0.9);
  EXPECT_EQ(loss_state.max_loss, 1.0);
  EXPECT_THAT(loss_state.max_infeasibility, DoubleEq(0.9));

  {
    UpdateMixLossState update_state(loss_state.min_loss,
                                    std::numeric_limits<double>::infinity());

    // We still use eta (step size) = infinity, so we only give weights
    // to expects with optimal loss (equal to min_loss).
    constraint.UpdateMixLoss(&update_state);
    EXPECT_EQ(update_state.mix_loss.num_weights, 3);
    EXPECT_EQ(update_state.mix_loss.sum_weights, 1.0);
  }

  // Same thing, but now with a finite step size
  {
    UpdateMixLossState update_state(-1.5, 2.0);

    update_state.mix_loss.num_weights = 2;
    update_state.mix_loss.sum_weights = 4;

    constraint.UpdateMixLoss(&update_state);
    EXPECT_EQ(update_state.mix_loss.num_weights, 2 + 3);
    EXPECT_EQ(update_state.mix_loss.sum_weights,
              4.0 + std::exp(2 * (-1.5 - -0.9)) + std::exp(2 * (-1.5 - 1.0)) +
                  std::exp(2 * -1.5));
  }
}

// Similar test, but with a non-zero state.
TEST(CoverConstraint, SecondInfinityIteration) {
  CoverConstraint constraint({0, 3, 1});

  {
    PrepareWeightsState prep_state(4, 0.0,
                                   std::numeric_limits<double>::infinity());
    constraint.PrepareWeights(&prep_state);
    ASSERT_EQ(constraint.last_solution(), 0);

    std::vector<double> knapsack_solution = {0.1, 1.0, 1.0, 0.0};
    ObserveLossState loss_state(knapsack_solution);

    constraint.ObserveLoss(&loss_state);
    ASSERT_THAT(constraint.loss(), ElementsAre(-0.9, 1.0, 0));
  }

  // We now have a constraint in the non-zero state
  // [-0.9, 1.0, 0].
  //
  // See what happens when we pass in an infinity step.
  {
    // Set min loss equal to -0.9.
    //
    // The weight vector is [1.0, 0.0, 0.0].
    PrepareWeightsState prep_state(4, -0.9,
                                   std::numeric_limits<double>::infinity());

    prep_state.knapsack_weights[0] = -0.5;
    prep_state.knapsack_weights[1] = -0.5;
    prep_state.knapsack_weights[2] = -0.5;
    prep_state.knapsack_weights[3] = -0.5;

    prep_state.knapsack_rhs = -1.0;

    constraint.PrepareWeights(&prep_state);
    EXPECT_THAT(prep_state.scratch, ElementsAre(1.0, 0.0, 0.0));

    // The last round had the master knapsack problem pick 1, and a
    // tiny bit of 0.
    //
    // This round, we'll incentivize the master to pick 0, and ourselves
    // pick 1 or 2.
    EXPECT_THAT(constraint.last_solution(), AnyOf(1, 2));
    EXPECT_EQ(prep_state.mix_loss.num_weights, 3);
    EXPECT_EQ(prep_state.mix_loss.sum_weights, 1.0);
    EXPECT_THAT(prep_state.knapsack_weights,
                ElementsAre(-1.5, -0.5, -0.5, -0.5));
    EXPECT_EQ(prep_state.knapsack_rhs, -1.0);
  }

  // Pass in a finite step size.
  {
    PrepareWeightsState prep_state(4, -1.0, 1.0);

    prep_state.knapsack_weights[0] = -0.5;
    prep_state.knapsack_weights[1] = -0.5;
    prep_state.knapsack_weights[2] = -0.5;
    prep_state.knapsack_weights[3] = -0.5;

    prep_state.knapsack_rhs = -1.0;

    // Loss is [-0.9, 1.0, 0.0]
    static constexpr double tenth = -1.0 + 0.9;
    constraint.PrepareWeights(&prep_state);
    EXPECT_THAT(prep_state.scratch,
                ElementsAre(std::exp(tenth), std::exp(-2.0), std::exp(-1.0)));

    // The last round had the master knapsack problem pick 1 and 2, and
    // a tiny bit of 0.
    //
    // This round, we'll incentivize the master to pick 0, and ourselves
    // pick 1.
    EXPECT_EQ(constraint.last_solution(), 1);
    EXPECT_EQ(prep_state.mix_loss.num_weights, 3);
    EXPECT_EQ(prep_state.mix_loss.sum_weights,
              std::exp(tenth) + std::exp(-2.0) + std::exp(-1.0));
    EXPECT_THAT(prep_state.knapsack_weights,
                ElementsAre(-0.5 - std::exp(tenth), -0.5 - std::exp(-2.0), -0.5,
                            -0.5 - std::exp(-1.0)));
    EXPECT_EQ(prep_state.knapsack_rhs, -1.0 - std::exp(-2.0));
  }

  // Make sure we detect feasibility of this individual relaxation.
  {
    CoverConstraint copy(constraint);

    std::vector<double> solution = {0.0, 1.0, 0.0, 0.0};
    ObserveLossState loss_state(solution);
    copy.ObserveLoss(&loss_state);
    EXPECT_EQ(loss_state.max_infeasibility, 0.0);
  }

  // And that we AND feasibility.
  {
    CoverConstraint copy(constraint);

    std::vector<double> solution = {0.0, 1.0, 0.0, 0.0};
    ObserveLossState loss_state(solution);
    loss_state.max_infeasibility = 0.5;
    copy.ObserveLoss(&loss_state);
    EXPECT_EQ(loss_state.max_infeasibility, 0.5);
  }

  // Update for a master solution.
  //
  // Master picks 0.
  //
  // Previous loss: [-0.9, 1.0, 0]
  {
    std::vector<double> solution = {1.0, 0.0, 0.0, 0.0};
    ObserveLossState loss_state(solution);

    ASSERT_EQ(constraint.last_solution(), 1);
    constraint.ObserveLoss(&loss_state);
    EXPECT_THAT(constraint.loss(), ElementsAre(1.0 - 0.9, 0.0, 0.0));
    EXPECT_EQ(loss_state.min_loss, 0.0);
    EXPECT_EQ(loss_state.max_loss, 1.0 - 0.9);
    EXPECT_THAT(loss_state.max_infeasibility, 1.0);
  }

  // Recompute the mix loss.
  {
    UpdateMixLossState update_state(0.0, 1.0);

    update_state.mix_loss.num_weights = 2;
    update_state.mix_loss.sum_weights = 0.5;
    constraint.UpdateMixLoss(&update_state);
    EXPECT_EQ(update_state.mix_loss.num_weights, 5);
    EXPECT_EQ(update_state.mix_loss.sum_weights,
              0.5 + std::exp(0.9 - 1.0) + 1.0 + 1.0);
  }
}
