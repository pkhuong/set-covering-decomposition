#include "random-set-cover-instance.h"

#include <random>

RandomSetCoverInstance GenerateRandomInstance(size_t num_sets,
                                              size_t num_values,
                                              size_t min_set_per_value,
                                              size_t max_set_per_value) {
  std::vector<double> obj_values;
  std::vector<std::vector<uint32_t>> sets_per_value;

  {
    std::random_device dev;
    std::mt19937 rng(dev());

    // First, generate all the sets.
    std::uniform_int_distribution<size_t> num_set_dist(min_set_per_value,
                                                       max_set_per_value);
    std::uniform_real_distribution<> obj_value_dist(0.0, 10.0);
    std::vector<uint32_t> set_ids;
    for (size_t i = 0; i < num_sets; ++i) {
      set_ids.push_back(i);
      obj_values.push_back(obj_value_dist(rng));
    }

    // Now assign covering sets for each value randomly.
    for (size_t i = 0; i < num_values; ++i) {
      sets_per_value.emplace_back();
      std::vector<uint32_t>& dst = sets_per_value.back();

      // Perform a partial Fisher-Yates shuffle and consume
      // newly-generated elements as they come.
      std::uniform_real_distribution<> u01(0.0, 1.0);
      for (size_t j = 0, n = num_set_dist(rng); j < n; ++j) {
        size_t src = j + (set_ids.size() - j) * u01(rng);
        std::swap(set_ids[j], set_ids[src]);
        dst.push_back(set_ids[j]);
      }
    }
  }

  std::vector<CoverConstraint> constraints;
  for (const std::vector<uint32_t>& sets : sets_per_value) {
    constraints.emplace_back(sets);
  }

  return RandomSetCoverInstance{
      std::move(obj_values), std::move(sets_per_value), std::move(constraints),
  };
}
