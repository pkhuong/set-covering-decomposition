#include "prng.h"

#include <random>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"

namespace {
using State = std::array<uint64_t, 4>;

struct GlobalState {
  absl::Mutex mu;
  absl::optional<State> state GUARDED_BY(mu);
};

uint64_t SplitMix(uint64_t x) {
  uint64_t z = x + 0x9e3779b97f4a7c15;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  return z ^ (z >> 31);
}

// Take random bits from the system RNG and mix them around with
// SplitMix64, as recommended by Vigna.
State InitializeGlobalState() {
  State ret;

  std::random_device dev;
  for (uint64_t& x : ret) {
    uint64_t bits = dev();
    bits = bits * (dev.max() + uint64_t{1}) + dev();

    x = SplitMix(bits);
  }

  return ret;
}

void AdvanceState(const State params, State* state) {
  uint64_t s0 = 0;
  uint64_t s1 = 0;
  uint64_t s2 = 0;
  uint64_t s3 = 0;
  for (const uint64_t param : params) {
    for (int b = 0; b < 64; b++) {
      if (param & (uint64_t{1} << b)) {
        s0 ^= (*state)[0];
        s1 ^= (*state)[1];
        s2 ^= (*state)[2];
        s3 ^= (*state)[3];
      }

      xs256::Advance(state);
    }

    (*state)[0] = s0;
    (*state)[1] = s1;
    (*state)[2] = s2;
    (*state)[3] = s3;
  }
}

// Advances `state` by 2^192 calls to `operator()()`.
void AdvanceGlobalState(State* state) {
  static const State long_jump_params = {
      0x76e15d3efefdcbbf, 0xc5004e441c522fb3, 0x77710069854ee241,
      0x39109bb02acbe635,
  };

  AdvanceState(long_jump_params, state);
}

// Advance `state` by 2^128 calls to `operator()()`.
void AdvanceLocalState(State* state) {
  static const State jump_params = {
      0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa,
      0x39abdc4529b1661c,
  };

  AdvanceState(jump_params, state);
}

State InitializeThreadLocalState() {
  static GlobalState& global_state = *new GlobalState;

  absl::MutexLock ml(&global_state.mu);
  if (!global_state.state.has_value()) {
    global_state.state = InitializeGlobalState();
  }

  State ret = global_state.state.value();
  AdvanceGlobalState(&global_state.state.value());
  return ret;
}
}  // namespace

xs256::xs256() {
  static thread_local absl::optional<State> local_state;

  if (!local_state.has_value()) {
    local_state = InitializeThreadLocalState();
  }

  state_ = local_state.value();
  AdvanceLocalState(&local_state.value());
}
