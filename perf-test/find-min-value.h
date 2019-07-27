#ifndef REGRESSION_FIND_MIN_VALUE_H
#define REGRESSION_FIND_MIN_VALUE_H
#include <cstddef>

#include "absl/types/span.h"
#include "bench/stable-unique-ptr.h"

struct FindMinValueInstance {
  const double* values;
  size_t num_values;
};

bench::StableUniquePtr<const FindMinValueInstance> MakeFindMinValueInstance(
    size_t n);

inline absl::Span<const double> PrepFindMinValueInstance(
    const bench::StableUniquePtr<const FindMinValueInstance>& instance) {
  return absl::MakeConstSpan(instance->values, instance->num_values);
}
#endif /* !REGRESSION_FIND_MIN_VALUE_H */
