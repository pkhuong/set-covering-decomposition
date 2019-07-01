#include "bench/time.h"

#include <iostream>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
TEST(Time, GetTicksWithBarrierBuilds) {
  int i = 0;
  EXPECT_GT(GetTicksBeginWithBarrier(i), 0);
}

TEST(Time, GetTicksOverhead) {
  const uint64_t overhead = GetTicksOverhead();
  std::cout << "Overhead " << overhead << std::endl;
  EXPECT_GT(overhead, 0);
}

// Test InterruptDetected for quality of implementation.  A random
// return value would technically satisfy the contract, and the
// default implementation simply always returns false.
//
// Still, make sure that it does what we want in a trivial case, where
// supported.  We'll have to extend the `InterruptDetected()` or
// disable this test case as we add more platforms.
TEST(Time, InterruptDetected) {
  const absl::Time deadline = absl::Now() + absl::Seconds(1);
  SetupInterruptDetection();
  // Busy loop here to force preemption.  The majority of the cycles
  // are in the inner loop, so we should be interrupted, rather than
  // switched out at the end of a syscall.
  while (absl::Now() < deadline && !InterruptDetected()) {
    for (size_t i = 0; i < 100; ++i) {
      asm volatile("pause" ::: "memory");
    }
  }

  EXPECT_TRUE(InterruptDetected());
}

TEST(Time, InterruptNotDetected) {
  size_t num_interruptions = 0;
  for (size_t i = 0; i < 10; ++i) {
    // Try to get the OS to schedule us out in the sleep syscall
    // rather than between SetupInterruptDetection() and
    // InterruptDetected().
    absl::SleepFor(absl::Milliseconds(100));
    SetupInterruptDetection();
    if (InterruptDetected()) {
      ++num_interruptions;
    }
  }

  // We might get unlucky once, but more than one in 10 trials seems
  // unlikely.
  EXPECT_LE(num_interruptions, 1);
}
}  // namespace
}  // namespace bench
