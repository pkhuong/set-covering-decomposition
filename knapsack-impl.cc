#include "knapsack-impl.h"

#include <assert.h>

#include <climits>
#include <cstdlib>
#include <tuple>

#include "absl/algorithm/container.h"
#include "prng.h"

namespace internal {
bool NormalizedEntry::operator==(const NormalizedEntry& other) const {
  return std::tie(weight, value, index) ==
         std::tie(other.weight, other.value, other.index);
}

NormalizedInstance NormalizeKnapsack(absl::Span<const double> obj_values,
                                     absl::Span<const double> weights,
                                     absl::Span<double> candidates) {
  assert(obj_values.size() == weights.size());
  assert(obj_values.size() == candidates.size());

  NormalizedInstance ret;
  ret.to_exclude.reserve(obj_values.size());
  for (size_t i = 0, n = obj_values.size(); i < n; ++i) {
    const double weight = weights[i];
    const double value = -obj_values[i];  // flip for max

    assert(weight <= 0);
    if (weight == 0 && value < 0) {
      candidates[i] = 0.0;
      continue;
    }

    candidates[i] = 1.0;
    ret.sum_candidate_values += value;
    ret.sum_candidate_weights += weight;

    // non-positive weight and positive value is always taken.
    // otherwise, add to normalized knapsack.
    if (value < 0) {
      assert(weight < 0);
      ret.to_exclude.push_back({-weight, -value, i});
    }
  }

  return ret;
}

namespace {
PartitionResult PartitionEntriesDispatch(PartitionInstance instance,
                                         xs256* prng);

// Trivial implementation: sort and scan.
PartitionResult PartitionEntriesBaseCase(PartitionInstance instance) {
  absl::c_sort(instance.entries,
               [](const NormalizedEntry& x, const NormalizedEntry& y) {
                 // Move higher profit ratio first.
                 //
                 //      x.value / x.weight > y.value / y.weight
                 // <->  x.value * y.weight > y.value * x.weight
                 return x.value * y.weight > y.value * x.weight;
               });

  size_t i = instance.initial_offset;
  double remaining_weight = instance.max_weight;
  double remaining_value = instance.max_value;
  assert(remaining_weight >= 0);
  assert(remaining_value >= 0);

  for (const NormalizedEntry& entry : instance.entries) {
    const double updated_remaining_weight = remaining_weight - entry.weight;
    const double updated_remaining_value = remaining_value - entry.value;

    if (updated_remaining_weight < 0 || updated_remaining_value < 0) {
      break;
    }

    ++i;
    remaining_weight = updated_remaining_weight;
    remaining_value = updated_remaining_value;
  }

  assert(remaining_weight >= 0);
  assert(remaining_value >= 0);
  return PartitionResult{i, remaining_weight, remaining_value};
}

double FindPivot(absl::Span<const NormalizedEntry> entries, xs256* prng) {
  assert(!entries.empty());

  std::array<double, 3> ratios;
  for (double& ratio : ratios) {
    const size_t index = prng->Uniform(entries.size());
    const auto& entry = entries[index];
    ratio = entry.value / entry.weight;
  }

  absl::c_sort(ratios);
  return ratios[1];
}

// Recursive implementation: partition and search in either the left or right
// half.
//
// This will do very badly with equally good entries (e.g., the
// PartitionEntriesLarge.EqualRanges test).
PartitionResult PartitionEntriesDivision(PartitionInstance instance,
                                         xs256* prng) {
  const double pivot = FindPivot(instance.entries, prng);

  // We want elements better or equal to pivot to the left.
  // That is, if entry.value / entry.weight >= pivot
  //       <==>  entry.value * pivot >= entry.weight.
  const auto splitter = [pivot](const NormalizedEntry& entry) {
    return entry.value * pivot >= entry.weight;
  };
  const size_t first_right =
      absl::c_partition(instance.entries, splitter) - instance.entries.begin();
  auto left_span = instance.entries.first(first_right);
  auto right_span = instance.entries.subspan(first_right);

  // XXX: re-partition equal values here!
  double left_weight = 0;
  double left_value = 0;
  for (const NormalizedEntry& entry : left_span) {
    left_weight += entry.weight;
    left_value += entry.value;
  }

  // The left half already violates one of the conditions. Keep looking there.
  if (left_weight > instance.max_weight || left_value > instance.max_value) {
    instance.entries = left_span;
    return PartitionEntriesDispatch(instance, prng);
  }

  instance.entries = right_span;
  // left_weight <= instance.max_weight
  instance.max_weight -= left_weight;
  // left_value <= instance.max_value
  instance.max_value -= left_value;
  instance.initial_offset += first_right;
  return PartitionEntriesDispatch(instance, prng);
}

size_t Log2Ceiling(size_t n) {
  return CHAR_BIT * sizeof(unsigned long long) - __builtin_clzll(n);
}

PartitionResult PartitionEntriesDispatch(PartitionInstance instance,
                                         xs256* prng) {
  if (instance.entries.empty() || instance.max_weight <= 0 ||
      instance.max_value <= 0) {
    return PartitionResult{instance.initial_offset, instance.max_weight,
                           instance.max_value};
  }

  if (instance.max_iter <= 1 ||
      instance.entries.size() < instance.min_partition_size) {
    return PartitionEntriesBaseCase(instance);
  }

  --instance.max_iter;
  return PartitionEntriesDivision(instance, prng);
}
}  // namespace

PartitionInstance::PartitionInstance(absl::Span<NormalizedEntry> entries_,
                                     double max_weight_, double max_value_)
    : PartitionInstance(entries_, max_weight_, max_value_, 0,
                        2 + 2 * Log2Ceiling(entries_.size() | 1),
                        kMinPartitionSize) {}

PartitionInstance::PartitionInstance(absl::Span<NormalizedEntry> entries_,
                                     double max_weight_, double max_value_,
                                     size_t initial_offset_, size_t max_iter_,
                                     size_t min_partition_size_)
    : entries(entries_),
      max_weight(max_weight_),
      max_value(max_value_),
      initial_offset(initial_offset_),
      max_iter(max_iter_),
      min_partition_size(min_partition_size_) {}

PartitionResult PartitionEntries(PartitionInstance instance) {
  xs256 prng;
  return PartitionEntriesDispatch(instance, &prng);
}
}  // namespace internal
