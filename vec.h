#include "absl/types/span.h"

// Various vectorized operations
namespace internal {
// weights[i] = exp(-step_size * (losses[i] - min_loss))
// weights.size() must be rounded up to a multiple of 8.
void ApplyHedgeLoss(absl::Span<const double> losses, double min_loss,
                    double step_size, absl::Span<double> weights);
}  // namespace internal
