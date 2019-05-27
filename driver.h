#ifndef DRIVER_H
#define DRIVER_H
#include <cstddef>
#include <limits>
#include <vector>

#include "absl/types/span.h"

#include "cover-constraint.h"

struct DriverState {
  explicit DriverState(absl::Span<const double> obj_values_in)
      : obj_values(obj_values_in), sum_solutions(obj_values.size(), 0.0) {}

  absl::Span<const double> obj_values;

  size_t num_iterations{0};
  double sum_mix_gap{0};

  size_t prev_num_non_zero{0};
  double prev_min_loss{0};
  double prev_max_loss{std::numeric_limits<double>::lowest()};
  double best_bound{0};
  std::vector<double> sum_solutions;
};

void DriveOneIteration(absl::Span<CoverConstraint> constraints,
                       DriverState* state);
#endif /* !DRIVER_H */
