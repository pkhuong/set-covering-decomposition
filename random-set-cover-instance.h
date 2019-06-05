#ifndef RANDOM_SET_COVER_INSTANCE_H
#define RANDOM_SET_COVER_INSTANCE_H
#include <cstdint>
#include <utility>
#include <vector>

#include "cover-constraint.h"

struct RandomSetCoverInstance {
  std::vector<double> obj_values;
  std::vector<std::vector<uint32_t>> sets_per_value;
  std::vector<CoverConstraint> constraints;
};

RandomSetCoverInstance GenerateRandomInstance(size_t num_sets,
                                              size_t num_values,
                                              size_t max_set_per_value);
#endif /* !RANDOM_SET_COVER_INSTANCE_H */
