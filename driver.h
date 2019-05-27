#ifndef DRIVER_H
#define DRIVER_H
#include <cstddef>
#include <limits>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/span.h"

#include "cover-constraint.h"

struct DriverState {
  explicit DriverState(absl::Span<const double> obj_values_in);

  absl::Span<const double> obj_values;

  size_t num_iterations{0};
  double sum_mix_gap{0};

  size_t prev_num_non_zero{0};
  double prev_min_loss{0};
  double prev_max_loss{-std::numeric_limits<double>::infinity()};
  double best_bound{0.0};

  double sum_solution_value{0};
  double sum_solution_feasibility{0};
  std::vector<double> sum_solutions;

  double max_last_solution_infeasibility{
      std::numeric_limits<double>::infinity()};
  double last_solution_value{-std::numeric_limits<double>::infinity()};
  std::vector<double> last_solution;

  bool feasible{true};
};

void DriveOneIteration(absl::Span<CoverConstraint> constraints,
                       DriverState* state);
#endif /* !DRIVER_H */
