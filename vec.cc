#include "vec.h"

#include "assert.h"

#include <array>
#include <cstddef>

#include "avx_mathfun.h"

namespace internal {
namespace {
constexpr size_t kChunkSize = 8;

// Computes weights[i] = exp(-step_size * (losses[i] - min_loss))
// over a chunk of values.
void ChunkApplyHedgeLoss(absl::Span<const double> losses, double min_loss,
                         double step_size, absl::Span<double> weights) {
  assert(losses.size() == kChunkSize);
  assert(weights.size() >= kChunkSize);

  union {
    float arr[kChunkSize];
    v8sf vec;
  } temp __attribute__((__aligned__(64)));
  for (size_t i = 0; i < kChunkSize; ++i) {
    temp.arr[i] = step_size * (min_loss - losses[i]);
  }

  temp.vec = exp256_ps(temp.vec);

  for (size_t i = 0; i < kChunkSize; ++i) {
    weights[i] = temp.arr[i];
  }
}

}  // namesoace

void ApplyHedgeLoss(absl::Span<const double> losses, double min_loss,
                    double step_size, absl::Span<double> weights) {
  while (losses.size() >= kChunkSize) {
    ChunkApplyHedgeLoss(losses.subspan(0, kChunkSize), min_loss, step_size,
                        weights.subspan(0, kChunkSize));
    losses.remove_prefix(kChunkSize);
    weights.remove_prefix(kChunkSize);
  }

  if (!losses.empty()) {
    double padded_tail[kChunkSize] = {0};

    const size_t n = losses.size();
    if (n > kChunkSize) {
      __builtin_unreachable();
    }

    for (size_t i = 0; i < n; ++i) {
      padded_tail[i] = losses[i];
    }

    ChunkApplyHedgeLoss(padded_tail, min_loss, step_size, weights);
  }
}
}  // namespace internal
