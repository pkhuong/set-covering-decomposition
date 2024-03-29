#ifndef COVER_CONSTRAINT_H
#define COVER_CONSTRAINT_H
#include <assert.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"

struct MixLossInfo {
  explicit MixLossInfo(double min_loss_in, double eta_in)
      : min_loss(min_loss_in), eta(eta_in) {}

  const double min_loss;
  const double eta;
  size_t num_weights{0};
  double sum_weights{0};

  void Merge(const MixLossInfo& in);
};

struct PrepareWeightsState {
  explicit PrepareWeightsState(size_t num_knapsack_weights, double min_loss,
                               double eta)
      : PrepareWeightsState(
            absl::FixedArray<double, 0>(num_knapsack_weights, 0.0), min_loss,
            eta) {}

  template <typename T, typename = typename T::value_type>
  explicit PrepareWeightsState(T data, double min_loss, double eta)
      : mix_loss(min_loss, eta) {
    auto backing = std::make_shared<T>(std::move(data));
    knapsack_weights = absl::MakeSpan(*backing);
    backing_storage = std::move(backing);

    // Avoid too many resizes at the beginning.
    scratch.reserve(1024);
  }

  // Not copyable, movable.
  PrepareWeightsState(const PrepareWeightsState&) = delete;
  PrepareWeightsState(PrepareWeightsState&&) = default;
  PrepareWeightsState& operator=(const PrepareWeightsState&) = delete;
  PrepareWeightsState& operator=(PrepareWeightsState&&) = default;

  MixLossInfo mix_loss;
  std::vector<double> scratch;

  absl::Span<double> knapsack_weights;
  double knapsack_rhs{0};

  void Merge(const PrepareWeightsState& in);

  std::shared_ptr<void> backing_storage;
};

struct ObserveLossState {
  explicit ObserveLossState(absl::Span<const double> solution_in)
      : knapsack_solution(solution_in) {}

  const absl::Span<const double> knapsack_solution;

  double min_loss{std::numeric_limits<double>::max()};
  double max_loss{std::numeric_limits<double>::lowest()};
  // For this specific iteration.
  double max_infeasibility{0.0};

  void Merge(const ObserveLossState& in);
};

struct UpdateMixLossState {
  explicit UpdateMixLossState(double min_loss, double eta)
      : mix_loss(min_loss, eta) {
    // Avoid too many resizes at the beginning.
    scratch.reserve(1024);
  }

  MixLossInfo mix_loss;
  std::vector<double> scratch;

  void Merge(const UpdateMixLossState& in);
};

// A cover constraint represents the requirement that a given location
// be touched by at least one tour in the solution.
class CoverConstraint {
 public:
  explicit CoverConstraint(absl::Span<const uint32_t> tours_in);

  // Copyable and movable, but not assignable.
  CoverConstraint(const CoverConstraint&) = default;
  CoverConstraint(CoverConstraint&&) = default;

  // Computes the posterior weights and mix loss for this constraint,
  // and updates its `last_solution` accordingly.
  // `state->sum_weights` is updated with the sum of un-normalised
  // weights, and so are the entries corresponding to `potential_tours`
  // in `state->knapsack_Weights`.
  void PrepareWeights(PrepareWeightsState* state);

  // Updates the cover constraint's internal loss accumulator, and
  // updates `state` accordingly.
  //
  // `min_loss` and `max_loss` are merged with the updated cumulative
  // loss values.
  void ObserveLoss(ObserveLossState* state);

  // Recomputes the posterior mix loss given the end-of-iteration `state`.
  // Increments `sum_weights` with the un-normalised posterior weights.
  void UpdateMixLoss(UpdateMixLossState* state) const;

  // These getters are only exposed for testing.
  size_t last_solution() const { return last_solution_; }
  absl::Span<const double> loss() const { return loss_; }

 private:
  // Populates `weights` with the un-normalised weights for this iteration, and
  // updates the accumulators in `info`.
  //
  // If `to_decrement` is provided, to_decerement[potential_tours_[i]] -=
  // (*weights)[i];
  void PopulateWeights(
      MixLossInfo* info, std::vector<double>* weights,
      absl::optional<absl::Span<double>> to_decrement = absl::nullopt) const;

  // Finds the min weight solution to this covering constraint, and
  // stores it in `last_solution`.
  double SolveSubproblem(absl::Span<const double> weights);

  // The cloning constraints are of the form [x_clone - x_orig <= 0].
  // The loss corresponds to the satisfaction of this constraint,
  // i.e., loss is x_orig - x_clone.
  //
  // Given weight w for this constraint, the surrogate subproblem's
  // linear constraint gains [-w x_orig <= - w x_clone].
  const absl::FixedArray<uint32_t, 0> potential_tours_;

  size_t last_solution_{-1ULL};
  // Loss tracks satisfaction for the cloning constraints, i.e., how
  // often the master picks a variable that we didn't.
  //
  // The values in this array are cumulative.
  absl::FixedArray<double, 0> loss_;
};
#endif /* !COVER_CONSTRAINT_H */
