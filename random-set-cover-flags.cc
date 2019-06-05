#include "random-set-cover-flags.h"

#include "absl/flags/flag.h"

ABSL_FLAG(double, feas_eps, 5e-3,
          "Allowed error for the clone linking constraints");

ABSL_FLAG(size_t, num_sets, 1000 * 1000, "Number of sets to pick from");

ABSL_FLAG(size_t, num_values, 10000, "Number of values to cover");

ABSL_FLAG(size_t, max_set_per_value, 2100,
          "Maximum number of set that may cover any value");

ABSL_FLAG(size_t, min_set_per_value, 1,
          "Minimum number of set that may cover any value");

ABSL_FLAG(size_t, max_iter, 100000, "Iteration limit");

ABSL_FLAG(bool, check_feasible, false,
          "Whether the solver should look for and return solutions to "
          "relaxations that happen to be feasible and optimal for the "
          "original problem");
