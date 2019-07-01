#ifndef BENCH_TIME_H
#define BENCH_TIME_H
#include <cstddef>
#include <cstdint>

#include "absl/base/optimization.h"

namespace bench {
// Returns the estimated overhead for the timing annotations.
uint64_t GetTicksOverhead();

// Call this function to setup interrupt detection state on the
// running thread.
void SetupInterruptDetection();

// Then call this function to determine whether the calling thread has
// been interrupted (preempted) since its last call to
// `SetupInterruptDetection()`.
//
// There may be false negatives (the default implementation always
// returns false), but the false positive rate should be < 1%.
bool InterruptDetected();

// Logs a warning if we're being interrupted "frequently".  Returns
// true if the counters should be cleared.
bool WarnOnRepeatedInterrupts(size_t runs, size_t interrupted);

namespace {
// See "How to Benchmark Code Execution Times on Intel® IA-32 and
// IA-64 Instruction Set Architectures" by Gabriele Paoloni for the
// reasoning behind the pair of GetTicks{Begin,End} functions.
//
// https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/ia-32-ia-64-benchmark-code-execution-paper.pdf
//
// Use this function in short (~ cacheline sized) timing loops.
inline uint64_t GetTicksBegin() {
  uint32_t lo, hi;
  asm volatile(
      "cpuid\n\t"
      "rdtsc"
      : "=a"(lo), "=d"(hi)::"%rbx", "%rcx", "memory", "cc");

  return (static_cast<uint64_t>(hi) << 32) | lo;
}

// This also gets the TSC for the beginning of a timing sequence, but
// ensures alignment of the code for better runtime consistency in
// the code that will be timed, downstream.  The method also doubles
// as a compiler barrier that ensures `x` is fully materialised,
// which is useful to make sure the compiler doesn't move prep work
// into the timing sequence.
//
// This version gives more consistent values when the timed sequence
// isn't in a small loop: the alignment helps mitigate the I$ noise
// introduced by surrounding code.
template <typename T>
inline uint64_t GetTicksBeginWithBarrier(const T& x) {
  uint32_t lo, hi;
  // We should be able to use ".align %3p", and pass the alignment as
  // an immediate integer. However, while GCC has long supported the
  // "p" modifier to strip the leading dollar sign from literals,
  // (older?)  clang does not.
  //
  // Enumerate all interesting cases here.
  asm volatile(
#if ABSL_CACHELINE_SIZE >= 128
      ".align 128\n\t"
#elif ABSL_CACHELINE_SIZE == 64
      ".align 64\n\t"
#else
      ".align 32\n\t"
#endif
      "cpuid\n\t"
      "rdtsc"
      : "=a"(lo), "=d"(hi)
      : "X"(x)
      : "%rbx", "%rcx", "memory", "cc");

  return (static_cast<uint64_t>(hi) << 32) | lo;
}

inline uint64_t GetTicksEnd() {
  uint32_t lo, hi;
  asm volatile(
      "rdtscp\n\t"
      "mov %%eax, %0\n\t"
      "mov %%edx, %1\n\t"
      "cpuid"
      : "=r"(lo), "=r"(hi)::"%rax", "%rdx", "%rbx", "%rcx", "memory", "cc");

  return (static_cast<uint64_t>(hi) << 32) | lo;
}
}  // namespace
}  // namespace bench
#endif /* !BENCH_TIME_H */
