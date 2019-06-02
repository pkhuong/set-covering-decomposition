#include "vec.h"

#include "assert.h"
#include <immintrin.h>

#include <array>
#include <cstddef>

#include "avx_mathfun.h"

namespace internal {
namespace {
using v4sf = __m128;
using v4df = __m256d;

constexpr size_t kChunkSize = 8;

// Computes weights[i] = exp(-step_size * (losses[i] - min_loss))
// over a chunk of values.
v4df ChunkApplyHedgeLoss(absl::Span<const double> losses, double min_loss,
                         double step_size, absl::Span<double> weights,
                         v4df acc) {
  assert(losses.size() == kChunkSize);
  assert(weights.size() >= kChunkSize);

  // __m256d _mm256_zextpd128_pd256 (__m128d a)
  // __m256 _mm256_zextps128_ps256 (__m128 a)
  const v4df vmin_loss = _mm256_set1_pd(min_loss);
  const v4df vneg_step_size = _mm256_set1_pd(-step_size);

  // to_exp = -step_size * (loss[i] - vmin_loss)
  v8sf to_exp = {0};
  {
    const v4df losses_lo = _mm256_loadu_pd(losses.data());
    const v4df to_exp_lo = vneg_step_size * (losses_lo - vmin_loss);
    to_exp = _mm256_insertf128_ps(to_exp, _mm256_cvtpd_ps(to_exp_lo), 0);

    const v4df losses_hi = _mm256_loadu_pd(losses.data() + 4);
    const v4df to_exp_hi = vneg_step_size * (losses_hi - vmin_loss);
    to_exp = _mm256_insertf128_ps(to_exp, _mm256_cvtpd_ps(to_exp_hi), 1);
  }

  v8sf float_weights = exp256_ps(to_exp);

  {
    v4df lo = _mm256_cvtps_pd(_mm256_extractf128_ps(float_weights, 0));
    acc += lo;
    _mm256_storeu_pd(weights.data(), lo);
  }

  {
    v4df hi = _mm256_cvtps_pd(_mm256_extractf128_ps(float_weights, 1));
    acc += hi;
    _mm256_storeu_pd(weights.data() + 4, hi);
  }

  return acc;
}

}  // namesoace

double ApplyHedgeLoss(absl::Span<const double> losses, double min_loss,
                      double step_size, absl::Span<double> weights) {
  v4df vacc = {0};

  while (losses.size() >= kChunkSize) {
    vacc = ChunkApplyHedgeLoss(losses.subspan(0, kChunkSize), min_loss,
                               step_size, weights.subspan(0, kChunkSize), vacc);
    losses.remove_prefix(kChunkSize);
    weights.remove_prefix(kChunkSize);
  }

  double acc = vacc[0] + vacc[1] + vacc[2] + vacc[3];
  if (!losses.empty()) {
    double padded_tail[kChunkSize] = {0};

    const size_t n = losses.size();
    if (n > kChunkSize) {
      __builtin_unreachable();
    }

    for (size_t i = 0; i < n; ++i) {
      padded_tail[i] = losses[i];
    }

    ChunkApplyHedgeLoss(padded_tail, min_loss, step_size, weights, vacc);
    for (size_t i = 0; i < n; ++i) {
      acc += weights[i];
    }
  }

  return acc;
}
}  // namespace internal
