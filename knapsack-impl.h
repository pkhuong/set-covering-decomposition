#ifndef KNAPSACK_IMPL_H
#define KNAPSACK_IMPL_H
#include <cstddef>
#include <vector>

#include "absl/types/span.h"

namespace internal {
struct NormalizedEntry {
  double weight;  // positive
  double value;   // positive
  size_t index;

  bool operator==(const NormalizedEntry& other) const;
  bool operator!=(const NormalizedEntry& other) const {
    return !(*this == other);
  }
};

struct NormalizedInstance {
  std::vector<size_t> candidate_indices;
  std::vector<NormalizedEntry> to_exclude;
  double sum_candidate_values{0};
  double sum_candidate_weights{0};
};

// Accepts the coefficients for a min / <= knapsack, where weights are
// non-positive.
// Converts to a regular max / <= knapsack by flipping the objective values.
// Then handles the negative weights by flipping the decision variables'
// meanings.
// The normalized knapsack subproblem will select variables to exclude.
NormalizedInstance NormalizeKnapsack(absl::Span<const double> obj_values,
                                     absl::Span<const double> weights);

struct PartitionResult {
  size_t partition_index;
  double remaining_weight;
  double remaining_value;
};

struct PartitionInstance {
  // By default, use sort as soon as we hit 4 elements or fewer.
  static constexpr size_t kMinPartitionSize = 5;

  PartitionInstance(absl::Span<NormalizedEntry> entries_, double max_weight_,
                    double max_value_);

  PartitionInstance(absl::Span<NormalizedEntry> entries_, double max_weight_,
                    double max_value_, size_t initial_offset_, size_t max_iter_,
                    size_t min_partition_size_ = kMinPartitionSize);

  // Remaining entries to partition
  absl::Span<NormalizedEntry> entries;
  // Maximum weight we can use by selecting entries.
  double max_weight;
  // Maximum value we can generate by selecting entries
  double max_value;
  // How many entries were removed to the left of the `entries` span.
  size_t initial_offset;
  // Number of partitioning iterations before switching to a sort.
  size_t max_iter;
  // Minimum size of entries to use the partition; otherwise, sort.
  size_t min_partition_size;
};

// Given a list of normalized entries from NormalizeKnapsack,
// finds the maximum value that hits max_weight, or the min weight
// that achieves max_value.
//
// The partition index in the result is thw first element in the
// (re-ordered) entries that does not fully fit in the knapsack.
PartitionResult PartitionEntries(PartitionInstance instance);
}  // namespace internal
#endif /*!KNAPSACK_IMPL_H */
