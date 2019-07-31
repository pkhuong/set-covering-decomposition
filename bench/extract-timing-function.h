#ifndef BENCH_EXTRACT_TIMING_FUNCTION_H
#define BENCH_EXTRACT_TIMING_FUNCTION_H
#include "absl/base/casts.h"
#include "absl/strings/string_view.h"
#include "bench/internal/dynamic-loading.h"
#include "bench/timing-function.h"

namespace bench {
// Attempts to dlopen `shared_open_path`, and calls `function_name()`
// (which must be appropriately mangled, or simply declared `extern
// "C"`) to obtain a `TimingFunction<ResultType, ..., Genresult>`,
// i.e., an
// `internal::ExplicitFunction<internal::TimedResult<ResultType>,
// GenResult>`.
//
// This type does its own consistency checking internally, so obvious
// type mismatches should be detected that way, at runtime.
//
// This function dies noisily on error.
template <typename ResultType, typename GenResult>
std::pair<TimingFunction<ResultType, void, GenResult>, internal::DLCloser>
ExtractTimingFunction(absl::string_view shared_object_path,
                      absl::string_view function_name) {
  using FnRet = TimingFunction<ResultType, void, GenResult>;
  static constexpr bool kIsProbablyABISafe = FnRet::kIsProbablyABISafe;

  std::pair<void*, internal::DLCloser> opened =
      internal::OpenOrDie<kIsProbablyABISafe>()(shared_object_path,
                                                function_name);

  auto timing_fn_generator = absl::bit_cast<FnRet (*)()>(opened.first);
  return std::make_pair(timing_fn_generator(), std::move(opened.second));
}
}  // namespace bench
#endif /*!BENCH_EXTRACT_TIMING_FUNCTION_H */
