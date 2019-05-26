#include "cover-constraint.h"

#include <algorithm>
#include <cmath>

#include "absl/algorithm/container.h"

void PrepareWeightsState::Merge(const PrepareWeightsState& in) {
  num_weights += in.num_weights;
  sum_weights += in.sum_weights;
  if (knapsack_weights.size() < in.knapsack_weights.size()) {
    knapsack_weights.resize(in.knapsack_weights.size(), 0.0);
  }

  for (size_t i = 0; i < in.knapsack_weights.size(); ++i) {
    knapsack_weights[i] += in.knapsack_weights[i];
  }

  knapsack_rhs += in.knapsack_rhs;
}

void ObserveLossState::Merge(const ObserveLossState& in) {
  min_loss = std::min(min_loss, in.min_loss);
  max_loss = std::max(max_loss, in.max_loss);
}

void UpdateMixLossState::Merge(const UpdateMixLossState& in) {
  num_weights += in.num_weights;
  sum_weights += in.sum_weights;
}

namespace {
double dsum(absl::Span<const double> vec) {
  double acc = 0;
  for (double x : vec) {
    acc += x;
  }

  return acc;
}

double dmin(absl::Span<const double> vec) {
  double acc = std::numeric_limits<double>::max();
  for (double x : vec) {
    acc = (x < acc) ? x : acc;
  }

  return acc;
}

double dmax(absl::Span<const double> vec) {
  double acc = std::numeric_limits<double>::lowest();
  for (double x : vec) {
    acc = (x > acc) ? x : acc;
  }

  return acc;
}

void sdec(absl::Span<const uint32_t> indices, absl::Span<const double> weights,
          absl::Span<double> dst) {
  for (size_t i = 0, n = indices.size(); i < n; ++i) {
    dst[indices[i]] -= weights[i];
  }
}

absl::FixedArray<uint32_t> usorted(absl::Span<const uint32_t> tours_in) {
  absl::FixedArray<uint32_t> ret(tours_in.begin(), tours_in.end());
  absl::c_sort(ret);
  return ret;
}
}  // namespace

CoverConstraint::CoverConstraint(absl::Span<const uint32_t> tours_in)
    : potential_tours_(usorted(tours_in)),
      loss_(potential_tours_.size(), 0.0) {}

void CoverConstraint::PrepareWeights(PrepareWeightsState* state) {
  std::vector<double>& scratch = state->scratch;

  if (potential_tours_.empty()) {
    return;
  }

  PopulateWeights(state->eta, state->min_loss, &scratch);
  state->num_weights += scratch.size();
  state->sum_weights += dsum(scratch);
  state->knapsack_rhs -= SolveSubproblem(scratch);

  if (state->knapsack_weights.size() <= potential_tours_.back()) {
    state->knapsack_weights.resize(potential_tours_.back() + 1, 0.0);
  }

  sdec(potential_tours_, scratch, absl::MakeSpan(state->knapsack_weights));
}

void CoverConstraint::ObserveLoss(ObserveLossState* state) {
  loss_[last_solution_] -= 1;
  for (size_t i = 0, n = potential_tours_.size(); i < n; ++i) {
    loss_[i] += state->knapsack_solution[potential_tours_[i]];
  }

  state->min_loss = std::min(state->min_loss, dmin(loss_));
  state->max_loss = std::max(state->max_loss, dmax(loss_));
}

void CoverConstraint::UpdateMixLoss(UpdateMixLossState* state) {
  std::vector<double>& scratch = state->scratch;

  if (potential_tours_.empty()) {
    return;
  }

  PopulateWeights(state->eta, state->min_loss, &scratch);
  state->num_weights += scratch.size();
  state->sum_weights += dsum(scratch);
}

void CoverConstraint::PopulateWeights(double eta, double min_loss,
                                      std::vector<double>* weights) const {
  weights->resize(potential_tours_.size());
  if (std::isinf(eta)) {
    for (size_t i = 0, n = loss_.size(); i < n; ++i) {
      (*weights)[i] = (loss_[i] == min_loss) ? 1.0 : 0.0;
    }

    return;
  }

  // XXX: vectorize.
  for (size_t i = 0, n = loss_.size(); i < n; ++i) {
    (*weights)[i] = std::exp(-eta * (loss_[i] - min_loss));
  }
}

// The weight vector is never empty nor negative, so we're looking for (any)
// min-value weight.
double CoverConstraint::SolveSubproblem(absl::Span<const double> weights) {
  size_t index = 0;
  double min_weight = weights[0];

  // XXX vectorize.
  for (size_t i = 1, n = weights.size(); i < n; ++i) {
    index = (weights[i] < min_weight) ? i : index;
  }

  last_solution_ = index;
  return min_weight;
}
