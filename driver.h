#ifndef DRIVER_H
#define DRIVER_H
#include <cstddef>
#include <limits>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"

#include "big-vec.h"
#include "cover-constraint.h"

struct DriverState {
  explicit DriverState(absl::Span<const double> obj_values_in);

  // Make sure arena is deallocated after all its potential children.
  BigVecArena arena;

  absl::Span<const double> obj_values;

  size_t num_iterations{0};
  double sum_mix_gap{0};

  size_t prev_num_non_zero{0};
  double prev_min_loss{0};
  double prev_max_loss{-std::numeric_limits<double>::infinity()};
  double best_bound{0.0};

  double sum_solution_value{0};
  double sum_solution_feasibility{0};
  BigVec<double> sum_solutions;

  double max_last_solution_infeasibility{
      std::numeric_limits<double>::infinity()};
  double last_solution_value{-std::numeric_limits<double>::infinity()};
  BigVec<double> last_solution;

  bool feasible{true};

  absl::Duration total_time;
  absl::Duration prepare_time;
  absl::Duration knapsack_time;
  absl::Duration observe_time;
  absl::Duration update_time;

  absl::Duration last_iteration_time;
  absl::Duration last_prepare_time;
  absl::Duration last_knapsack_time;
  absl::Duration last_observe_time;
  absl::Duration last_update_time;
};

void DriveOneIteration(absl::Span<CoverConstraint> constraints,
                       DriverState* state);
#endif /* !DRIVER_H */
