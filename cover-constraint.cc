#include "cover-constraint.h"

#include <assert.h>

#include <algorithm>
#include <cmath>

#include "absl/algorithm/container.h"
#include "vec.h"

#define PREFETCH_DISTANCE 32

namespace {
double dsum(absl::Span<const double> vec) {
  double acc = 0;
  for (double x : vec) {
    acc += x;
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
#ifdef PREFETCH_DISTANCE
    __builtin_prefetch(&dst[indices[std::min(n - 1, i + PREFETCH_DISTANCE)]]);
#endif
    dst[indices[i]] -= weights[i];
  }
}

absl::FixedArray<uint32_t, 0> usorted(absl::Span<const uint32_t> tours_in) {
  absl::FixedArray<uint32_t, 0> ret(tours_in.begin(), tours_in.end());
  absl::c_sort(ret);
  return ret;
}
}  // namespace

void MixLossInfo::Merge(const MixLossInfo& in) {
  num_weights += in.num_weights;
  sum_weights += in.sum_weights;
}

void PrepareWeightsState::Merge(const PrepareWeightsState& in) {
  mix_loss.Merge(in.mix_loss);

  assert(knapsack_weights.size() == in.knapsack_weights.size());
  vinc(in.knapsack_weights, absl::MakeSpan(knapsack_weights));
  knapsack_rhs += in.knapsack_rhs;
}

void ObserveLossState::Merge(const ObserveLossState& in) {
  min_loss = std::min(min_loss, in.min_loss);
  max_loss = std::max(max_loss, in.max_loss);
  max_infeasibility = std::max(max_infeasibility, in.max_infeasibility);
}

void UpdateMixLossState::Merge(const UpdateMixLossState& in) {
  mix_loss.Merge(in.mix_loss);
}

CoverConstraint::CoverConstraint(absl::Span<const uint32_t> tours_in)
    : potential_tours_(usorted(tours_in)),
      loss_(potential_tours_.size(), 0.0) {}

void CoverConstraint::PrepareWeights(PrepareWeightsState* state) {
  std::vector<double>& scratch = state->scratch;

  if (potential_tours_.empty()) {
    return;
  }

  PopulateWeights(&state->mix_loss, &scratch);
  state->knapsack_rhs -= SolveSubproblem(scratch);

  assert(state->knapsack_weights.size() > potential_tours_.back());
  sdec(potential_tours_, scratch, absl::MakeSpan(state->knapsack_weights));
}

void CoverConstraint::ObserveLoss(ObserveLossState* state) {
  const double infeasibility =
      1.0 - state->knapsack_solution[potential_tours_[last_solution_]];

  double min_loss = std::numeric_limits<double>::max();
  double max_loss = std::numeric_limits<double>::lowest();

  loss_[last_solution_] -= 1;
  for (size_t i = 0, n = potential_tours_.size(); i < n; ++i) {
#ifdef PREFETCH_DISTANCE
    __builtin_prefetch(&state->knapsack_solution[potential_tours_[std::min(
        n - 1, i + PREFETCH_DISTANCE)]]);
#endif
    double cur_loss = loss_[i] + state->knapsack_solution[potential_tours_[i]];
    loss_[i] = cur_loss;
    min_loss = (min_loss < cur_loss) ? min_loss : cur_loss;
    max_loss = (max_loss > cur_loss) ? max_loss : cur_loss;
  }

  ObserveLossState current_loss(state->knapsack_solution);
  current_loss.min_loss = min_loss;
  current_loss.max_loss = max_loss;
  current_loss.max_infeasibility = infeasibility;

  state->Merge(current_loss);
}

void CoverConstraint::UpdateMixLoss(UpdateMixLossState* state) const {
  std::vector<double>& scratch = state->scratch;

  if (potential_tours_.empty()) {
    return;
  }

  PopulateWeights(&state->mix_loss, &scratch);
}

void CoverConstraint::PopulateWeights(MixLossInfo* info,
                                      std::vector<double>* weights) const {
  // Ensure geometric growth works despite repeated `resize` calls.
  const size_t padded_size = potential_tours_.size() + 7;
  if (padded_size > weights->capacity()) {
    weights->reserve(std::max(2 * weights->capacity(), padded_size));
  }

  weights->resize(padded_size);
  const double eta = info->eta;
  const double min_loss = info->min_loss;
  if (std::isinf(eta)) {
    for (size_t i = 0, n = loss_.size(); i < n; ++i) {
      (*weights)[i] = (loss_[i] == min_loss) ? 1.0 : 0.0;
    }
  } else {
#ifdef NO_VECTORIZE
    for (size_t i = 0, n = loss_.size(); i < n; ++i) {
      (*weights)[i] = std::exp(-eta * (loss_[i] - min_loss));
    }
#else
    internal::ApplyHedgeLoss(loss_, min_loss, eta, absl::MakeSpan(*weights));
#endif
  }

  weights->resize(potential_tours_.size());
  info->num_weights += weights->size();
  info->sum_weights += dsum(*weights);
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
