#include "absl/types/span.h"

// Various vectorized operations
namespace internal {
// weights[i] = exp(-step_size * (losses[i] - min_loss))
// weights.size() must be rounded up to a multiple of 8.
//
// Returns the sum of generated weights.
double ApplyHedgeLoss(absl::Span<const double> losses, double min_loss,
                      double step_size, absl::Span<double> weights);

template <typename Fn>
double ApplyHedgeLossWithForEach(absl::Span<const double> losses,
                                 const double min_loss, const double step_size,
                                 const Fn& fn, absl::Span<double> weights,
                                 const size_t block_size = 64) {
  double ret = 0;

  const size_t n = losses.size();
  size_t i;

  // Tile calls 64 elements at a time (1 SIMD line ~= 8 elements).
  for (i = 0; i + block_size <= n; i += block_size) {
    ret += ApplyHedgeLoss(losses.first(block_size), min_loss, step_size,
                          weights.first(block_size));
    for (size_t j = 0; j < block_size; ++j) {
      fn(i + j, weights[j]);
    }

    losses.remove_prefix(block_size);
    weights.remove_prefix(block_size);
  }

  ret += ApplyHedgeLoss(losses, min_loss, step_size, weights);
  for (size_t j = 0; j < losses.size(); ++j) {
    fn(i + j, weights[j]);
  }

  return ret;
}
}  // namespace internal
