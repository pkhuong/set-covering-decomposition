#ifndef RANDOM_SET_COVER_FLAGS_H
#define RANDOM_SET_COVER_FLAGS_H
#include <cstddef>

#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(double, feas_eps);

ABSL_DECLARE_FLAG(size_t, num_sets);

ABSL_DECLARE_FLAG(size_t, num_values);

ABSL_DECLARE_FLAG(size_t, max_set_per_value);

ABSL_DECLARE_FLAG(size_t, min_set_per_value);

ABSL_DECLARE_FLAG(size_t, max_iter);

ABSL_DECLARE_FLAG(bool, check_feasible);
#endif /* !RANDOM_SET_COVER_FLAGS_H */
