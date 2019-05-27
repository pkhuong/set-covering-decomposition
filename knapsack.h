#ifndef KNAPSACK_H
#define KNAPSACK_H
#include <ostream>
#include <utility>
#include <vector>

#include "absl/types/span.h"

struct KnapsackSolution {
  explicit KnapsackSolution(std::vector<double> solution_ = {},
                            double objective_value_ = 0,
                            double feasibility_ = 0, bool feasible_ = false);

  bool operator==(const KnapsackSolution& other) const;
  friend std::ostream& operator<<(std::ostream& stream,
                                  const KnapsackSolution& solution);

  std::vector<double> solution;
  double objective_value{0};
  double feasibility{0};
  bool feasible{false};
};

// Solves a min-cost knapsack of the form
//
//  \min \sum x_i obj_values_i
// s.t.
//  \sum x_i weights_i \leq rhs.
//
// In our case, rhs is non-positive, and so are the weights.
//
// Best bound is a lower bound on the objective value.  If the optimal
// value for this knapsack is less than `best_bound`, stop as soon as
// we hit `best_bound` and instead return a "more feasible" solution.
//
// eps is the allowed leeway on feasibility.
//
// `scratch` is used to pre-allocate the solution vector in the return value.
KnapsackSolution SolveKnapsack(absl::Span<const double> obj_values,
                               absl::Span<const double> weights, double rhs,
                               double eps, double best_bound,
                               std::vector<double> scratch = {});
#endif /* !KNAPSACK_H */
