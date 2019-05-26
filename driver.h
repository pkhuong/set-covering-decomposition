#ifndef DRIVER_H
#define DRIVER_H
#include <cstddef>
#include <limits>
#include <vector>

#include "cover-constraint.h"

struct DriverState {
  explicit DriverState(const std::vector<double>& obj_values_in)
      : obj_values(obj_values_in) {}

  const std::vector<double>& obj_values;

  size_t num_iterations{0};
  double sum_mix_gap{0};

  size_t prev_num_non_zero{2};
  double prev_min_loss{0};
  double prev_max_loss{std::numeric_limits<double>::lowest()};
  double best_bound{0};
  std::vector<double> sum_solutions;
};

void DriveOneIteration(std::vector<CoverConstraint>& constraints,
                       DriverState* state);
#endif /* !DRIVER_H */
