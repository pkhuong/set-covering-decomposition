#include <utility>

#include "bench/timing-function.h"
#include "perf-test/find-min-value.h"
#include "vec.h"

namespace {
__attribute__((__noinline__)) std::pair<size_t, double> BaseFindMinValue(
    absl::Span<const double> values) {
  size_t index = 0;
  double min_val = values[0];

  for (size_t i = 1, n = values.size(); i < n; ++i) {
    if (values[i] < min_val) {
      index = i;
      min_val = values[i];
    }
  }

  return std::make_pair(index, min_val);
}

const auto ProtoFindMinValueInstance = [] {
  return MakeFindMinValueInstance(10);
};

const auto WrappedFindMinValue = [](absl::Span<const double> values) {
  return internal::FindMinValue(values).first;
};

const auto WrappedBaseFindMinValue = [](absl::Span<const double> values) {
  return BaseFindMinValue(values).first;
};
}  // namespace

// Expose MakeTimingFunction callbacks to time the current
// implementation in `internal::FindMinValue`, and the baseline
// implementation above.
DEFINE_MAKE_TIMING_FUNCTION(MakeFindMinValue,
                            decltype(ProtoFindMinValueInstance),
                            PrepFindMinValueInstance, WrappedFindMinValue);

DEFINE_MAKE_TIMING_FUNCTION(MakeBaseFindMinValue,
                            decltype(ProtoFindMinValueInstance),
                            PrepFindMinValueInstance, WrappedBaseFindMinValue);
