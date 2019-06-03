#include "knapsack-impl.h"

#include <cmath>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace internal {
namespace {
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Range;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

TEST(NormalizeInstance, Trivial) {
  BigVecArenaContext ctx;

  // min / <=
  const std::vector<double> obj_values = {1, 2};
  const std::vector<double> weights = {-1, -2};
  std::vector<double> candidates(2, -1.0);

  const NormalizedInstance ret =
      NormalizeKnapsack(obj_values, weights, absl::MakeSpan(candidates));
  EXPECT_THAT(candidates, ElementsAre(1.0, 1.0));
  EXPECT_THAT(ret.to_exclude, UnorderedElementsAre(NormalizedEntry{1, 1, 0},
                                                   NormalizedEntry{2, 2, 1}));
  EXPECT_EQ(ret.sum_candidate_values, -3);
  EXPECT_EQ(ret.sum_candidate_weights, -3);
}

TEST(NormalizeInstance, AllCases) {
  BigVecArenaContext ctx;

  // weights are non-positive. values are arbitrary.
  const std::vector<double> obj_values = {-1, 0, 1, -1, 0, 1};
  const std::vector<double> weights = {0, 0, 0, -1, -2, -3};
  std::vector<double> candidates(6, 42.0);

  // In a min / <= knapsack...
  // element 0 (-1, 0) is always taken.
  //
  // element 1 (0, 0) is irrelevant (assume taken)
  //
  // element 2 (1, 0) is useless
  //
  // element 3 (-1, -1) is always taken
  //
  // element 4 (0, -2) is always taken
  //
  // element 5 (1, -3) is unknown

  const NormalizedInstance ret =
      NormalizeKnapsack(obj_values, weights, absl::MakeSpan(candidates));
  EXPECT_THAT(candidates, ElementsAre(1.0, 1.0, 0.0, 1.0, 1.0, 1.0));
  EXPECT_THAT(ret.to_exclude, UnorderedElementsAre(NormalizedEntry{3, 1, 5}));

  // If we pick element 5, the real solution is {0, 1, 3, 4}, with
  // value -2, i.e., -(1 + 1), and weight -3, i.e., -6 + 3.
  //
  // If we don't pick element 5, the real solution is {0, 1, 3, 4, 5},
  // which value -1 and weight -6.
  EXPECT_EQ(ret.sum_candidate_values, 1);
  EXPECT_EQ(ret.sum_candidate_weights, -6);
}

TEST(PartitionInstance, MaxIter) {
  std::vector<NormalizedEntry> a_entries(4);
  const PartitionInstance a(absl::MakeSpan(a_entries), 0, 0);

  std::vector<NormalizedEntry> b_entries(7);
  const PartitionInstance b(absl::MakeSpan(a_entries), 0, 0);

  std::vector<NormalizedEntry> c_entries(12);
  const PartitionInstance c(absl::MakeSpan(c_entries), 0, 0);

  std::vector<NormalizedEntry> d_entries(0);
  const PartitionInstance d(absl::MakeSpan(d_entries), 0, 0);

  EXPECT_EQ(a.max_iter, b.max_iter);
  EXPECT_GT(c.max_iter, a.max_iter);
  EXPECT_LT(d.max_iter, a.max_iter);
}

TEST(PartitionEntries, Empty) {
  const PartitionResult ret = PartitionEntries(
      PartitionInstance({}, 0.1, 0.5, 2, 0, /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 2);
  EXPECT_EQ(ret.remaining_weight, 0.1);
  EXPECT_EQ(ret.remaining_value, 0.5);
}

TEST(PartitionEntries, OneTooSmall) {
  std::vector<NormalizedEntry> entries = {{1.5, 2, 0}};
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 2, 3, 0, 0,
                                         /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 1);
  EXPECT_EQ(ret.remaining_weight, 0.5);
  EXPECT_EQ(ret.remaining_value, 1);
}

TEST(PartitionEntries, OneTooSmallValue) {
  std::vector<NormalizedEntry> entries = {{2, 2.5, 0}};
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 4, 3, 0, 0,
                                         /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 1);
  EXPECT_EQ(ret.remaining_weight, 2);
  EXPECT_EQ(ret.remaining_value, 0.5);
}

TEST(PartitionEntries, OneTooBig) {
  std::vector<NormalizedEntry> entries = {{2.5, 2, 0}};
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 2, 3, 0, 0,
                                         /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 0);
  EXPECT_EQ(ret.remaining_weight, 2);
  EXPECT_EQ(ret.remaining_value, 3);
}

TEST(PartitionEntries, OneTooBigValue) {
  std::vector<NormalizedEntry> entries = {{2, 4, 0}};
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 4, 3, 0, 0,
                                         /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 0);
  EXPECT_EQ(ret.remaining_weight, 4);
  EXPECT_EQ(ret.remaining_value, 3);
}

TEST(PartitionEntries, OneJustRightBoth) {
  std::vector<NormalizedEntry> entries = {{2, 4, 0}};
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 2, 4, 0, 0,
                                         /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 1);
  EXPECT_EQ(ret.remaining_weight, 0);
  EXPECT_EQ(ret.remaining_value, 0);
}

TEST(PartitionEntries, OneJustRightWeight) {
  std::vector<NormalizedEntry> entries = {{2, 3, 0}};
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 2, 4, 0, 0,
                                         /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 1);
  EXPECT_EQ(ret.remaining_weight, 0);
  EXPECT_EQ(ret.remaining_value, 1);
}

TEST(PartitionEntries, OneJustRightValue) {
  std::vector<NormalizedEntry> entries = {{1.5, 4, 0}};
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 2, 4, 0, 0,
                                         /*min_partition_size_=*/0));

  EXPECT_EQ(ret.partition_index, 1);
  EXPECT_EQ(ret.remaining_weight, 0.5);
  EXPECT_EQ(ret.remaining_value, 0);
}

TEST(PartitionEntries, ThreeWay1Both) {
  const std::vector<NormalizedEntry> init_entries = {
      {1.0, 2.0, 0}, {2.0, 3.0, 1}, {0.5, 2.5, 2}};

  // With max weight 2 and max value 5, we hit max weight after 1 and 0.5
  std::vector<NormalizedEntry> entries = init_entries;
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 2, 5, 0, 0,
                                         /*min_partition_size_=*/0));
  // Should be a permutation.
  EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

  EXPECT_EQ(ret.partition_index, 2);
  EXPECT_EQ(ret.remaining_weight, 0.5);
  EXPECT_EQ(ret.remaining_value, 0.5);
  EXPECT_THAT(absl::MakeConstSpan(entries.data(), ret.partition_index),
              UnorderedElementsAre(init_entries[0], init_entries[2]));
}

TEST(PartitionEntries, ThreeWay1ByValue) {
  const std::vector<NormalizedEntry> init_entries = {
      {1.0, 2.0, 0}, {2.0, 3.0, 1}, {0.5, 2.5, 2}};

  // With max weight 10 and max value 5, we hit max value after 2 and 2.5
  std::vector<NormalizedEntry> entries = init_entries;
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 10, 5, 0, 0,
                                         /*min_partition_size_=*/0));
  EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

  EXPECT_EQ(ret.partition_index, 2);
  EXPECT_EQ(ret.remaining_weight, 8.5);
  EXPECT_EQ(ret.remaining_value, 0.5);
  EXPECT_THAT(absl::MakeConstSpan(entries.data(), ret.partition_index),
              UnorderedElementsAre(init_entries[0], init_entries[2]));
}

TEST(PartitionEntries, ThreeWay1ByWeight) {
  const std::vector<NormalizedEntry> init_entries = {
      {1.0, 2.0, 0}, {2.0, 3.0, 1}, {0.5, 2.5, 2}};

  // With max weight 10 and max value 5, we hit max value after 2 and 2.5
  std::vector<NormalizedEntry> entries = init_entries;
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 2, 10, 0, 0,
                                         /*min_partition_size_=*/0));
  EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

  EXPECT_EQ(ret.partition_index, 2);
  EXPECT_EQ(ret.remaining_weight, 0.5);
  EXPECT_EQ(ret.remaining_value, 5.5);
  EXPECT_THAT(absl::MakeConstSpan(entries.data(), ret.partition_index),
              UnorderedElementsAre(init_entries[0], init_entries[2]));
}

TEST(PartitionEntries, ThreeWay2) {
  const std::vector<NormalizedEntry> init_entries = {
      {1.0, 2.0, 0}, {2.0, 3.0, 1}, {0.5, 2.5, 2}};

  // With max weight 1 and max value 5, we hit max weight after 0.5
  std::vector<NormalizedEntry> entries = init_entries;
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 1, 5, 0, 0,
                                         /*min_partition_size_=*/0));
  EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

  EXPECT_EQ(ret.partition_index, 1);
  EXPECT_EQ(ret.remaining_weight, 0.5);
  EXPECT_EQ(ret.remaining_value, 2.5);
  EXPECT_THAT(absl::MakeConstSpan(entries.data(), ret.partition_index),
              UnorderedElementsAre(init_entries[2]));
}

TEST(PartitionEntries, ThreeWay3) {
  const std::vector<NormalizedEntry> init_entries = {
      {1.0, 2.0, 0}, {2.0, 3.0, 1}, {0.5, 2.5, 2}};

  // With max weight 1.5 and max value 5, we hit max weight after 0.5 + 1
  std::vector<NormalizedEntry> entries = init_entries;
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 1.5, 5, 0, 0,
                                         /*min_partition_size_=*/0));
  EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

  EXPECT_EQ(ret.partition_index, 2);
  EXPECT_EQ(ret.remaining_weight, 0);
  EXPECT_EQ(ret.remaining_value, 0.5);
  EXPECT_THAT(absl::MakeConstSpan(entries.data(), ret.partition_index),
              UnorderedElementsAre(init_entries[0], init_entries[2]));
}

TEST(PartitionEntries, ThreeWay4) {
  const std::vector<NormalizedEntry> init_entries = {
      {1.0, 2.0, 0}, {2.0, 3.0, 1}, {0.5, 2.5, 2}};

  // Enough rope for all 3 elements
  std::vector<NormalizedEntry> entries = init_entries;
  const PartitionResult ret =
      PartitionEntries(PartitionInstance(absl::MakeSpan(entries), 3.5, 10, 0, 0,
                                         /*min_partition_size_=*/0));
  EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

  EXPECT_EQ(ret.partition_index, 3);
  EXPECT_EQ(ret.remaining_weight, 0);
  EXPECT_EQ(ret.remaining_value, 2.5);
  EXPECT_THAT(
      absl::MakeConstSpan(entries.data(), ret.partition_index),
      UnorderedElementsAre(init_entries[0], init_entries[1], init_entries[2]));
}

class PartitionEntriesLarge : public TestWithParam<size_t> {};

// Weight sequence 1, 2, 3, ...; value sequence 0.5, 1.5, ...
TEST_P(PartitionEntriesLarge, MonotonicBelow) {
  const size_t n = GetParam();

  std::vector<NormalizedEntry> init_entries;
  for (size_t i = 0; i < n; ++i) {
    init_entries.push_back({i + 1.0, i + 0.5, i});
  }

  for (size_t max_iter = 0; max_iter < 10; ++max_iter) {
    // Stop on max weight.
    //
    // Later entries are better. Total weight is n * (n + 1) / 2.
    // Subtract first 20%.
    std::vector<NormalizedEntry> entries = init_entries;
    const size_t prefix = n / 5;
    const double suffix_weight =
        (n * (n + 1) / 2) - (prefix * (prefix + 1) / 2);
    const PartitionResult result = PartitionEntries(PartitionInstance(
        absl::MakeSpan(entries), 0.5 + suffix_weight, 1.0 * n * n, 0, max_iter,
        /*min_partition_size_=*/5));
    // Should be a permutation.
    EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

    EXPECT_EQ(result.remaining_weight, 0.5);
    EXPECT_EQ(result.remaining_value,
              n * n - suffix_weight + 0.5 * (n - prefix));
    EXPECT_EQ(result.partition_index, n - prefix);
    EXPECT_THAT(absl::MakeSpan(entries.data(), result.partition_index),
                UnorderedElementsAreArray(
                    absl::MakeSpan(init_entries).subspan(prefix)));
    if (prefix > 0) {
      EXPECT_EQ(entries[result.partition_index], init_entries[prefix - 1]);
    }
  }
}

TEST_P(PartitionEntriesLarge, MonotonicBelowValue) {
  const size_t n = GetParam();

  std::vector<NormalizedEntry> init_entries;
  for (size_t i = 0; i < n; ++i) {
    init_entries.push_back({i + 0.5, i + 1.0, i});
  }

  for (size_t max_iter = 0; max_iter < 10; ++max_iter) {
    // Stop on max value.
    //
    // Earlier entries are better, keep first 20%.
    std::vector<NormalizedEntry> entries = init_entries;
    const size_t prefix = n / 5;
    const double prefix_value = prefix * (prefix + 1) / 2;
    const PartitionResult result = PartitionEntries(PartitionInstance(
        absl::MakeSpan(entries), 1.0 * n * n, 0.5 + prefix_value, 0, max_iter,
        /*min_partition_size_=*/5));
    // Should be a permutation.
    EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

    EXPECT_EQ(result.remaining_weight, n * n - (prefix_value - 0.5 * prefix));
    EXPECT_EQ(result.remaining_value, 0.5);
    EXPECT_EQ(result.partition_index, prefix);
    EXPECT_THAT(absl::MakeSpan(entries.data(), result.partition_index),
                UnorderedElementsAreArray(
                    absl::MakeSpan(init_entries).subspan(0, prefix)));
    EXPECT_EQ(entries[result.partition_index], init_entries[prefix]);
  }
}

// Weight sequence 1, 2, 3, ...; value sequence 1.5, 2.5, ...
TEST_P(PartitionEntriesLarge, MonotonicAbove) {
  const size_t n = GetParam();

  std::vector<NormalizedEntry> init_entries;
  for (size_t i = 0; i < n; ++i) {
    init_entries.push_back({i + 1.0, i + 1.5, i});
  }

  for (size_t max_iter = 0; max_iter < 10; ++max_iter) {
    // Stop on max weight.
    //
    // Earlier entries are better, keep first 20%.
    //
    // Later entries are better. Total weight is n * (n + 1) / 2.
    // Subtract first 20%.
    std::vector<NormalizedEntry> entries = init_entries;
    const size_t prefix = n / 5;
    const double prefix_weight = prefix * (prefix + 1) / 2;
    const PartitionResult result = PartitionEntries(PartitionInstance(
        absl::MakeSpan(entries), 0.5 + prefix_weight, 1.0 * n * n, 0, max_iter,
        /*min_partition_size_=*/5));
    // Should be a permutation.
    EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

    EXPECT_EQ(result.remaining_weight, 0.5);
    EXPECT_EQ(result.remaining_value, n * n - (prefix_weight + 0.5 * prefix));
    EXPECT_EQ(result.partition_index, prefix);
    EXPECT_THAT(absl::MakeSpan(entries.data(), result.partition_index),
                UnorderedElementsAreArray(
                    absl::MakeSpan(init_entries).subspan(0, prefix)));
    EXPECT_EQ(entries[result.partition_index], init_entries[prefix]);
  }
}

TEST_P(PartitionEntriesLarge, EqualRanges) {
  const size_t n = GetParam();

  std::vector<NormalizedEntry> init_entries;
  for (size_t i = 0; i < n; ++i) {
    init_entries.push_back({i + 1.0, i + 1.0, 3 * i});
    init_entries.push_back({i + 1.0, 2 * (i + 1.0), 3 * i + 1});
    init_entries.push_back({i + 1.0, 3 * (i + 1.0), 3 * i + 2});
  }

  // Partition only the best.
  // The best n entries have profit ratio 3.
  for (size_t max_iter = 0; max_iter < 10; ++max_iter) {
    std::vector<NormalizedEntry> entries = init_entries;
    double prefix_weight = n * (n + 1) / 4.0;
    const PartitionResult result = PartitionEntries(PartitionInstance(
        absl::MakeSpan(entries), prefix_weight, 4.0 * n * n, 0, max_iter,
        /*min_partition_size_=*/5));
    // Should be a permutation.
    EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

    EXPECT_LE(result.partition_index, n);
    EXPECT_LE(result.remaining_weight, entries[result.partition_index].weight);

    double sum_weights = 0;
    for (size_t j = 0; j <= result.partition_index; ++j) {
      EXPECT_EQ(entries[j].value / entries[j].weight, 3.0);
      if (j < result.partition_index) {
        sum_weights += entries[j].weight;
      }
    }

    EXPECT_LE(std::abs(prefix_weight - sum_weights - result.remaining_weight),
              1e-5);
  }

  // Keep enough value for the best 1.5n entries.
  for (size_t max_iter = 0; max_iter < 10; ++max_iter) {
    std::vector<NormalizedEntry> entries = init_entries;
    double prefix_value = 3 * n * (n + 1) / 2 + n * (n + 1) / 2;
    const PartitionResult result = PartitionEntries(PartitionInstance(
        absl::MakeSpan(entries), 5.0 * n * n, prefix_value, 0, max_iter,
        /*min_partition_size_=*/5));
    // Should be a permutation.
    EXPECT_THAT(entries, UnorderedElementsAreArray(init_entries));

    // There are many ways to partition what's left after the most valuable
    // items are taken, so add some freedom here.
    EXPECT_LE(result.partition_index, 2 + 3 * n / 2);
    EXPECT_LE(result.remaining_value, entries[result.partition_index].value);

    double sum_values = 0;
    size_t num_threes = 0;
    for (size_t j = 0; j <= result.partition_index; ++j) {
      num_threes += (entries[j].value == 3 * entries[j].weight);
      EXPECT_THAT(entries[j].value / entries[j].weight, AnyOf(3.0, 2.0));
      if (j < result.partition_index) {
        sum_values += entries[j].value;
      }
    }

    EXPECT_EQ(num_threes, n);
    EXPECT_LE(std::abs(prefix_value - sum_values - result.remaining_value),
              1e-5);
  }
}

INSTANTIATE_TEST_SUITE_P(PartitionEntriesLarge, PartitionEntriesLarge,
                         Range<size_t>(1, 100));

}  // namespace
}  // namespace internal
