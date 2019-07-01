#include "bench/internal/constructable-array.h"

#include <array>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace internal {
namespace {
using ::testing::ElementsAre;

// Fully instantiate a ConstructableArray of `Foo`s. Each `Foo` should
// have been construct correctly, and each destructor should be called
// exactly once.
TEST(ConstructableArray, SmokeTest) {
  struct Foo {
    explicit Foo(int* counter_) : counter(counter_) {}

    ~Foo() { ++(*counter); }

    int* const counter;
  };

  std::array<int, 2> counters = {0, 0};
  ConstructableArray<Foo, 2> arr;

  arr.EmplaceAt(0, &counters[0]);
  arr.EmplaceAt(1, &counters[1]);

  EXPECT_EQ(arr[0].counter, &counters[0]);
  EXPECT_EQ(arr[1].counter, &counters[1]);

  arr.Destroy();
  EXPECT_THAT(counters, ElementsAre(1, 1));
}

}  // namespace
}  // namespace internal
}  // namespace bench
