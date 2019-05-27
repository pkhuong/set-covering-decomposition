#include "driver.h"

#include <cmath>

#include "cover-constraint.h"
#include "knapsack.h"

namespace {
constexpr double kEps = 1e-8;

// Minimize x ' values, subject to x in [0, 11]^d.
// That's just the sum of negative values.
double LowerBoundObjectiveValue(absl::Span<const double> values) {
  double acc = 0.0;
  for (double v : values) {
    acc += (v < 0.0) ? v : 0.0;
  }

  return acc;
}

double ComputeMixLoss(double min_loss, double eta, double sum_weights,
                      size_t num_weights) {
  return min_loss - std::log(sum_weights / num_weights) / eta;
}

void dxpy(absl::Span<const double> src, absl::Span<double> acc) {
  assert(src.size() == acc.size());

  for (size_t i = 0, n = src.size(); i < n; ++i) {
    acc[i] += src[i];
  }
}
}  // namespace

DriverState::DriverState(absl::Span<const double> obj_values_in)
    : obj_values(obj_values_in),
      best_bound(LowerBoundObjectiveValue(obj_values)),
      sum_solutions(obj_values.size(), 0.0) {}

absl::optional<std::vector<double>> DriveOneIteration(
    absl::Span<CoverConstraint> constraints, DriverState* state) {
  // Step size.
  const double eta = std::log(std::max<size_t>(2, state->prev_num_non_zero)) /
                     state->sum_mix_gap;

  PrepareWeightsState prepare_weights(state->obj_values.size(),
                                      state->prev_min_loss, eta);
  for (auto& constraint : constraints) {
    constraint.PrepareWeights(&prepare_weights);
  }

  const double prev_mix_loss =
      ComputeMixLoss(prepare_weights.min_loss, prepare_weights.eta,
                     prepare_weights.sum_weights, prepare_weights.num_weights);

  double target_objective_value = state->best_bound;
  if (std::isfinite(state->best_bound)) {
    double sum_best_bound = state->best_bound * (state->num_iterations + 1);
    const double sum_value = state->sum_solution_value;

    assert(sum_best_bound + kEps >= sum_value);
    if (sum_best_bound < sum_value) {
      sum_best_bound = sum_value;
    }

    // If the objective value hits target_objective_value, the new sum
    // of solution values will yield an average of exactly
    // state->best_bound.
    target_objective_value = sum_best_bound - sum_value;
  }

  KnapsackSolution master_sol =
      SolveKnapsack(state->obj_values, prepare_weights.knapsack_weights,
                    prepare_weights.knapsack_rhs, kEps, target_objective_value);

  dxpy(master_sol.solution, absl::MakeSpan(state->sum_solutions));
  state->sum_solution_value += master_sol.objective_value;
  state->num_iterations++;

  // Only update the objective value bound if we stopped for feasibility.

  if (master_sol.feasibility <= kEps) {
    state->best_bound = std::max(state->best_bound, master_sol.objective_value);
  }

  const double observed_loss =
      master_sol.feasibility / prepare_weights.sum_weights;
  state->sum_solution_feasibility += observed_loss;

  ObserveLossState observe_state(master_sol.solution, kEps);
  for (auto& constraint : constraints) {
    constraint.ObserveLoss(&observe_state);
  }

  state->prev_min_loss = observe_state.min_loss;
  state->prev_max_loss = observe_state.max_loss;

  UpdateMixLossState update_state(observe_state.min_loss, eta);
  for (auto& constraint : constraints) {
    constraint.UpdateMixLoss(&update_state);
  }

  state->prev_num_non_zero = update_state.num_weights;

  const double mix_loss =
      ComputeMixLoss(update_state.min_loss, update_state.eta,
                     update_state.sum_weights, update_state.num_weights);

  state->sum_mix_gap +=
      std::max(0.0, observed_loss - (mix_loss - prev_mix_loss));

  if (observe_state.all_feasible) {
    return std::move(master_sol.solution);
  }

  return absl::nullopt;
}
