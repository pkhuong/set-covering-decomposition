#include "driver.h"

#include <cmath>

#include "cover-constraint.h"
#include "knapsack.h"

namespace {
double ComputeMixLoss(double min_loss, double eta, double sum_weights,
                      size_t num_weights) {
  return min_loss - std::log(sum_weights / num_weights) / eta;
}

void dxpy(const std::vector<double>& src, std::vector<double>* acc) {
  if (acc->size() < src.size()) {
    acc->resize(src.size(), 0.0);
  }

  for (size_t i = 0, n = src.size(); i < n; ++i) {
    (*acc)[i] += src[i];
  }
}
}  // namespace

void DriveOneIteration(std::vector<CoverConstraint>& constraints,
                       DriverState* state) {
  const double eta = std::log(state->prev_num_non_zero) / state->sum_mix_gap;

  PrepareWeightsState prepare_weights(state->prev_min_loss, eta);
  for (auto& constraint : constraints) {
    constraint.PrepareWeights(&prepare_weights);
  }

  const double prev_mix_loss =
      ComputeMixLoss(prepare_weights.min_loss, prepare_weights.eta,
                     prepare_weights.sum_weights, prepare_weights.num_weights);

  KnapsackSolution master_sol =
      SolveKnapsack(state->obj_values, prepare_weights.knapsack_weights,
                    prepare_weights.knapsack_rhs, state->best_bound);

  dxpy(master_sol.solution, &state->sum_solutions);
  state->num_iterations++;
  state->best_bound = std::max(state->best_bound, master_sol.objective_value);

  const double observed_loss =
      master_sol.feasibility / prepare_weights.sum_weights;

  ObserveLossState observe_state(master_sol.solution);
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
}
