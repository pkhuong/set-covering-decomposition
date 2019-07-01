#include "bench/time.h"

#include <sched.h>
#include <iostream>
#include <limits>

#include "absl/algorithm/container.h"
#include "absl/synchronization/mutex.h"

namespace bench {
namespace {
uint64_t EstimateGetTicksOverhead() {
  static constexpr uint64_t kNoEstimate = std::numeric_limits<uint64_t>::max();
  static constexpr size_t kInnerLoopCount = 100;

#ifndef NDEBUG
  std::cerr << "Running benchmarks without NDEBUG. Are you benchmarking a "
               "debug build?"
            << std::endl;
#endif

  uint64_t estimate = kNoEstimate;
  // 3000 outer iterations suffice to get a stable value in a noisy
  // environment (virtualised Linux on a laptop running on battery
  // power).  Add a 33% safety margin on top.
  for (size_t i = 0; i < 4000; ++i) {
    // kInnerLoopCount is low enough that the inner loop should run in
    // less than 1 ms, which should be short enough to usually not be
    // interrupted.
    std::array<uint64_t, kInnerLoopCount> estimates;

    // Cooperatively yield to the scheduler here instead of waiting to
    // be interrupted in the next loop.  This really isn't guaranteed
    // to work, but doesn't hurt.
    sched_yield();

    // Set to true if we observe ticks going backwards. That only
    // happens when the timing sequence is interrupted.
    bool noise = false;
    SetupInterruptDetection();
    for (uint64_t& estimate : estimates) {
      uint64_t begin = GetTicksBegin();
      asm volatile("" ::: "memory");
      uint64_t end = GetTicksEnd();

      // Clock went backward. This timing inner loop is tainted; drop
      // all the data and try again.
      if (end <= begin) {
        noise = true;
        break;
      }

      estimate = end - begin;
    }

    if (!InterruptDetected() && !noise) {
      // Skip the two lowest estimates: if interrupt detection did not
      // work, a very fast (~1 microsecond) CPU migration might make
      // us underestimate the time elapsed between `end` and `begin`.
      //
      // Unless someone is actively messing with the system,
      // migrations happen rarely (a couple times per second on each
      // core), so we shouldn't observe two migrations in the inner
      // timing loop.  Still, skip two observations to be extra safe.
      //
      // Once clock skew has been taken care of, aim for the minimum
      // value because other sources of noise should be one-sided: by
      // definition, any other effect can only *add* to the minimum
      // timing overhead.

      // Partial sort to swap the in correct value for `estimates[2]`.
      absl::c_nth_element(estimates, estimates.begin() + 2);
      estimate = std::min(estimate, estimates[2]);
    }
  }

  if (estimate == kNoEstimate) {
    return 0;
  }

  return estimate;
}
}  // namespace

uint64_t GetTicksOverhead() {
  static const uint64_t overhead = EstimateGetTicksOverhead();
  return overhead;
}

// This trick was described in
// https://lackingrhoticity.blogspot.com/2018/01/observing-interrupts-from-userland-on-x86.html
// A segment descriptor of 1 is invalid, but that doesn't matter as
// long as no one uses it (and %gs isn't a system segment descriptor
// in the x86-64 ABIs).  However, IRET will convert this invalid
// descriptor to a 0 if an interrupt occurs, so we can look at that.
void SetupInterruptDetection() {
#ifdef __x86_64__
  uint16_t bad_segment = 1;
  asm volatile("mov %0, %%gs" ::"r"(bad_segment) : "memory");
#endif
  // TODO: add more (!x86-64 specific) detection logic. Maybe look at
  // the CPU index to at least detect migration?
}

bool InterruptDetected() {
#ifdef __x86_64__
  uint16_t current_segment = 0;
  asm volatile("mov %%gs, %0" : "=r"(current_segment)::"memory");
  return current_segment != 1;
#else
  return false;
#endif
}

bool WarnOnRepeatedInterrupts(size_t runs, size_t interrupted) {
  // Only consume the data once we have multiple drops (to avoid
  // triggering on one-offs) and some amount of total observations.
  // to protect against short-lived but sustained blips.
  if (interrupted < 5 || runs < 500) {
    return false;
  }

  // A correctly isolated environment sees <<< 1% interruptions.  A
  // noisy laptop seems to regularly hit 1.5-2% interruptions.  Log on
  // 2.5% to avoid log spam, while still triggering on the laptop.
  if (interrupted >= 0.025 * runs) {
    const double percent = 100.0 * interrupted / runs;
    static auto& lock = *new absl::Mutex();

    if (lock.TryLock()) {
      std::cerr << "Results tainted by context switching in " << percent
                << "% (" << interrupted << ") of the last " << runs << " runs."
                << std::endl;
      lock.Unlock();
    }
  }

  return true;
}
}  // namespace bench
