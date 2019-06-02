#include "driver.h"

#include <cmath>
#include <limits>

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

double ComputeMixLoss(const MixLossInfo& info) {
  return info.min_loss -
         std::log(info.sum_weights / info.num_weights) / info.eta;
}

void dxpy(absl::Span<const double> src, absl::Span<double> acc) {
  assert(src.size() == acc.size());

  for (size_t i = 0, n = src.size(); i < n; ++i) {
    acc[i] += src[i];
  }
}

PrepareWeightsState PrepareAllWeights(absl::Span<CoverConstraint> constraints,
                                      DriverState* state) {
  // Step size.
  double eta = std::numeric_limits<double>::infinity();
  if (state->sum_mix_gap > 0) {
    eta = std::log(std::max<size_t>(2, state->prev_num_non_zero)) /
          state->sum_mix_gap;
  }

  PrepareWeightsState ret(state->arena.Create<double>(state->obj_values.size()),
                          state->prev_min_loss, eta);
  for (auto& constraint : constraints) {
    constraint.PrepareWeights(&ret);
  }

  return ret;
}

double ComputeTargetObjectiveValue(const DriverState& state) {
  const double best_bound = state.best_bound;
  const double sum_value = state.sum_solution_value;

  assert(std::isfinite(best_bound));
  assert(std::isfinite(sum_value));

  double sum_best_bound = best_bound * (state.num_iterations + 1);
  assert(sum_best_bound + kEps >= sum_value);
  sum_best_bound = std::max(sum_best_bound, sum_value);
  // If the objective value hits target_objective_value, the new sum
  // of solution values will yield an average of exactly
  // state->best_bound.
  return sum_best_bound - sum_value;
}

// Finds a solution for the new knapsack master problem, and updates
// all the counters related to the master knapsack solutions, and
// returns the observed loss (rescaled for probabilities).
//
// Stores the master solution in `state->last_solution`
double UpdateStateWithNewRelaxedSolution(
    const PrepareWeightsState& prepare_weights, DriverState* state) {
  const double target_objective_value = ComputeTargetObjectiveValue(*state);

  // Borrow storage for the solution, and move it back into place
  // before returning.
  state->last_solution.clear();
  KnapsackSolution master_sol =
      SolveKnapsack(state->obj_values, prepare_weights.knapsack_weights,
                    prepare_weights.knapsack_rhs, kEps, target_objective_value,
                    &state->arena);

  dxpy(master_sol.solution, absl::MakeSpan(state->sum_solutions));
  state->sum_solution_value += master_sol.objective_value;
  state->num_iterations++;

  // Only update the objective value bound if we stopped for feasibility.
  if (master_sol.feasibility <= kEps) {  // XXX: should we look for exactly 0?
    state->best_bound = std::max(state->best_bound, master_sol.objective_value);
  }

  state->last_solution_value = master_sol.objective_value;

  const double observed_loss =
      master_sol.feasibility / prepare_weights.mix_loss.sum_weights;
  state->sum_solution_feasibility += observed_loss;

  state->last_solution = std::move(master_sol.solution);
  state->feasible = master_sol.feasible;
  return observed_loss;
}

// Observes losses for all constraints and updates the loss trackers in state.
ObserveLossState ObserveAllLosses(absl::Span<CoverConstraint> constraints,
                                  DriverState* state) {
  ObserveLossState observe_state(state->last_solution);
  for (auto& constraint : constraints) {
    constraint.ObserveLoss(&observe_state);
  }

  state->prev_min_loss = observe_state.min_loss;
  state->prev_max_loss = observe_state.max_loss;
  state->max_last_solution_infeasibility = observe_state.max_infeasibility;
  return observe_state;
}

UpdateMixLossState UpdateAllMixLosses(
    absl::Span<CoverConstraint> constraints,
    const PrepareWeightsState& prepare_weights,
    const ObserveLossState& observe_state, DriverState* state) {
  UpdateMixLossState update_state(observe_state.min_loss,
                                  prepare_weights.mix_loss.eta);
  for (auto& constraint : constraints) {
    constraint.UpdateMixLoss(&update_state);
  }

  state->prev_num_non_zero = update_state.mix_loss.num_weights;
  return update_state;
}
}  // namespace

DriverState::DriverState(absl::Span<const double> obj_values_in)
    : obj_values(obj_values_in),
      best_bound(LowerBoundObjectiveValue(obj_values)) {
  sum_solutions = arena.Create<double>(obj_values.size(), 0.0);
}

void DriveOneIteration(absl::Span<CoverConstraint> constraints,
                       DriverState* state) {
  const PrepareWeightsState prepare_weights =
      PrepareAllWeights(constraints, state);
  const double prev_mix_loss = ComputeMixLoss(prepare_weights.mix_loss);

  const double observed_loss =
      UpdateStateWithNewRelaxedSolution(prepare_weights, state);
  // Nothing to do here: we have an infeasible problem!
  if (!state->feasible) {
    return;
  }

  const ObserveLossState observe_state = ObserveAllLosses(constraints, state);
  const UpdateMixLossState update_state =
      UpdateAllMixLosses(constraints, prepare_weights, observe_state, state);

  const double mix_loss = ComputeMixLoss(update_state.mix_loss);
  state->sum_mix_gap +=
      std::max(0.0, observed_loss - (mix_loss - prev_mix_loss));
}
