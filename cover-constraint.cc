#include "cover-constraint.h"

#include <assert.h>

#include <algorithm>
#include <cmath>

#include "absl/algorithm/container.h"

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

void vinc(absl::Span<const double> src, absl::Span<double> dst) {
  for (size_t i = 0; i < src.size(); ++i) {
    dst[i] += src[i];
  }
}

void sdec(absl::Span<const uint32_t> indices, absl::Span<const double> weights,
          absl::Span<double> dst) {
  for (size_t i = 0, n = indices.size(); i < n; ++i) {
    dst[indices[i]] -= weights[i];
  }
}

absl::FixedArray<uint32_t, 0> usorted(absl::Span<const uint32_t> tours_in) {
  absl::FixedArray<uint32_t, 0> ret(tours_in.begin(), tours_in.end());
  absl::c_sort(ret);
  return ret;
}
}  // namespace

void PrepareWeightsState::Merge(PrepareWeightsState in) {
  num_weights += in.num_weights;
  sum_weights += in.sum_weights;

  assert(knapsack_weights.size() == in.knapsack_weights.size());
  vinc(in.knapsack_weights, absl::MakeSpan(knapsack_weights));
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

  assert(state->knapsack_weights.size() > potential_tours_.back());
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

void CoverConstraint::UpdateMixLoss(UpdateMixLossState* state) const {
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
  // Ensure geometric growth works despite repeated `resize` calls.
  if (potential_tours_.size() > weights->capacity()) {
    weights->reserve(
        std::max(2 * weights->capacity(), potential_tours_.size()));
  }

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
    const double weight_i = weights[i];
    index = (weight_i < min_weight) ? i : index;
    min_weight = (weight_i < min_weight) ? weight_i : min_weight;
  }

  last_solution_ = index;
  return min_weight;
}