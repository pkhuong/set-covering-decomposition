#include "knapsack.h"

#include <assert.h>

#include <iostream>
#include <tuple>

#include "knapsack-impl.h"

using ::internal::NormalizedEntry;
using ::internal::NormalizedInstance;
using ::internal::NormalizeKnapsack;
using ::internal::PartitionInstance;
using ::internal::PartitionResult;
using ::internal::PartitionEntries;

bool KnapsackSolution::operator==(const KnapsackSolution& other) const {
  return std::tie(solution, objective_value, feasibility, feasible) ==
         std::tie(other.solution, other.objective_value, other.feasibility,
                  other.feasible);
}

std::ostream& operator<<(std::ostream& stream,
                         const KnapsackSolution& solution) {
  stream << "KnapsackSolution{[";

  bool first = true;
  for (const double value : solution.solution) {
    stream << (first ? "" : ", ") << value;
    first = false;
  }
  stream << "], " << solution.objective_value << ", " << solution.feasibility
         << ", " << (solution.feasible ? "feasible" : "infeasible") << "}";
  return stream;
}

// The weights are all non-positive, so we want to flip the meaning of
// the decision variables: we'll select items that should not be in
// the knapsack.
KnapsackSolution SolveKnapsack(absl::Span<const double> obj_values,
                               absl::Span<const double> weights, double rhs,
                               double best_bound) {
  assert(obj_values.size() == weights.size());
  for (double weight : weights) {
    (void)weight;
    assert(weight <= 0);
  }

  KnapsackSolution ret;
  // We obtain a regular max / <= knapsack by flipping the objective function.
  // The weights are negative, so the goal is to exclude items.
  NormalizedInstance knapsack = NormalizeKnapsack(obj_values, weights);

  // If we don't remove anything, the sum of weights is
  // knapsack.sum_candidate_values.
  // The maximum weight increase incurred by removing items is
  const double max_weight_increase = rhs - knapsack.sum_candidate_weights;
  if (max_weight_increase < 0) {
    ret.feasible = false;
    return ret;
  }

  // We want to keep the objective value for the min knapsack at
  // least >= best_bound.  We're also removing items, so that is
  // actually
  //     -sum_candidate_values - \sum_{removed i} value_i >= best_bound,
  //
  // i.e.,
  //     sum_candidate_values + sum_{removed i} value_i <= -best_bound,
  // or
  //     sum_{removed i} value_i <= -best_bound - sum_candidate_values;

  // If the best bound is such that our most feasible solution is still
  // too good, disregard that information and just return that most
  // feasible solution.
  best_bound = std::min(best_bound, -knapsack.sum_candidate_values);
  const double max_value_increase = -best_bound - knapsack.sum_candidate_values;
  // best_bound <= -knapsack.sum_candidate_values
  //  -> -best_bound >= knapsack.sum_candidate_values
  //  -> max_weight_increase >= 0;
  assert(max_value_increase >= 0);

  PartitionResult partition =
      PartitionEntries(PartitionInstance(absl::MakeSpan(knapsack.to_exclude),
                                         /*max_weight_=*/max_weight_increase,
                                         /*max_value_=*/max_value_increase));

  ret.solution.resize(weights.size(), 0);
  for (size_t i : knapsack.candidate_indices) {
    ret.solution[i] = 1;
  }

  for (const auto& elem : absl::MakeConstSpan(knapsack.to_exclude)
                              .subspan(0, partition.partition_index)) {
    ret.solution[elem.index] = 0;
  }

  if (partition.partition_index < knapsack.to_exclude.size()) {
    const NormalizedEntry break_elem =
        knapsack.to_exclude[partition.partition_index];
    const double remaining =
        std::min(partition.remaining_weight / break_elem.weight,
                 partition.remaining_value / break_elem.value);
    ret.solution[break_elem.index] = 1 - remaining;
    partition.remaining_weight -= remaining * break_elem.weight;
    partition.remaining_value -= remaining * break_elem.value;
  }

  // We have remainining_value = -best_bound - sum_candidate_values -
  // sum_{removed i} value_i.
  //
  // value = -sum_candidate_values + sum_{removed i} value_i
  //       = remaining_value + best_bound.
  ret.objective_value = best_bound + partition.remaining_value;
  ret.feasibility = partition.remaining_weight;
  ret.feasible = true;
  return ret;
}
