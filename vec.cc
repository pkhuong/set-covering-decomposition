#include "vec.h"

#include <assert.h>
#include <immintrin.h>

#include <array>
#include <cstddef>

#include "avx_mathfun.h"

namespace internal {
namespace {
using v4sf = __m128;
using v4df = __m256d;
using v4i = __m256i;

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

}  // namespace

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

std::pair<size_t, double> FindMinValue(absl::Span<const double> xs) {
  size_t index, i = 4, n;
  double min_val;
  double vals_array[4];
  size_t indices_array[4];

  n = xs.size();

  if (n < 4) {
    index = 0;
    min_val = xs[0];
  } else {
    const v4i increment = _mm256_set1_epi64x(4);
    v4i indices = _mm256_setr_epi64x(0, 1, 2, 3);
    v4i min_indices = indices;
    v4df min_vals = _mm256_loadu_pd(xs.data());

    for (; i < n; i +=4) {
      indices = _mm256_add_epi64(indices, increment);
      const v4df vals_i = _mm256_loadu_pd(xs.data() + i);
      v4i lt = _mm256_castpd_si256(_mm256_cmp_pd(vals_i, min_vals, _CMP_LT_OS));
      min_indices = _mm256_blendv_epi8(min_indices, indices, lt);
      min_vals = _mm256_min_pd(vals_i, min_vals);
    }

    _mm256_storeu_pd(vals_array, min_vals);
    _mm256_storeu_si256((v4i *)indices_array, min_indices);

    min_val = vals_array[0];
    index = indices_array[0];

    for (size_t j = 0; j < 4; j++) {
      const double val_j = vals_array[j];
      index = (val_j < min_val) ? j : index;
      min_val = (val_j < min_val) ? val_j : min_val;
    }
  }

  if (i > n) {
    i -= 4;

    for (; i < n; i++) {
      const double val_i = xs[i];
      index = (val_i < min_val) ? i : index;
      min_val = (val_i < min_val) ? val_i : min_val;
    }
  }

  return std::make_pair(index, min_val);
}
}  // namespace internal
