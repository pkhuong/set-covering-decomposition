#ifndef BENCH_COMPARE_LIBRARIES_H
#define BENCH_COMPARE_LIBRARIES_H
#include <ostream>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/strings/string_view.h"
#include "bench/compare-functions.h"
#include "bench/internal/dynamic-loading.h"
#include "bench/internal/meta.h"
#include "bench/test-params.h"
#include "bench/timing-function.h"

// The `CompareLibraries` overloads defined in this header perform
// an A/B comparison in parallel.
//
// See `compare-functions.h` for more details.  The only difference is
// that the comparison in this header grabs the functions to time from
// shared objects that may have been built arbitrarily.
namespace bench {
// General case: the functions to time return `ResultType`,
// we call `generator()` to generate new inputs, get timing
// data from the two shared objects, compare with `comparator`,
// and analyse with `Analysis`.
//
// Each `path_fun` pair has the path to the shared object file as the
// first element, and the appropriately mangled name of the nullary
// function to call to get the result of `MakeTimingFunction`.
template <typename ResultType = std::tuple<>, typename Generator,
          typename Comparator, typename Analysis>
auto CompareLibraries(
    const TestParams& params, Generator generator,
    std::pair<absl::string_view, absl::string_view> path_fun_a,
    std::pair<absl::string_view, absl::string_view> path_fun_b,
    Comparator comparator, Analysis* analysis)
    -> decltype(analysis->Summary(std::declval<std::ostream*>())) {
  using GenResult =
      decltype(internal::CallAndTuplify()(generator, std::make_tuple()));

  auto fun_a = ExtractTimingFunction<ResultType, GenResult>(path_fun_a.first,
                                                            path_fun_a.second);

  auto fun_b = ExtractTimingFunction<ResultType, GenResult>(path_fun_b.first,
                                                            path_fun_b.second);

  return CompareFunctions(params, std::move(generator), std::move(fun_a.first),
                          std::move(fun_b.first), std::move(comparator),
                          analysis);
}

// In this overload, `analysis->params()` provides the test parameters.
template <typename ResultType = std::tuple<>, typename Generator,
          typename Comparator, typename Analysis>
auto CompareLibraries(
    Generator generator,
    std::pair<absl::string_view, absl::string_view> path_fun_a,
    std::pair<absl::string_view, absl::string_view> path_fun_b,
    Comparator comparator, Analysis* analysis) ->
    typename std::enable_if<
        !std::is_same<Generator, TestParams>::value,
        decltype(analysis->Summary(std::declval<std::ostream*>()))>::type {
  using GenResult =
      decltype(internal::CallAndTuplify()(generator, std::make_tuple()));

  auto fun_a = ExtractTimingFunction<ResultType, GenResult>(path_fun_a.first,
                                                            path_fun_a.second);

  auto fun_b = ExtractTimingFunction<ResultType, GenResult>(path_fun_b.first,
                                                            path_fun_b.second);

  return CompareFunctions(analysis->params(), std::move(generator),
                          std::move(fun_a.first), std::move(fun_b.first),
                          std::move(comparator), analysis);
}

// In this overload, `analysis->params()` provides the test parameters,
// and `analysis->comparator()` the comparator.
template <typename ResultType = std::tuple<>, typename Generator,
          typename Analysis>
auto CompareLibraries(
    Generator generator,
    std::pair<absl::string_view, absl::string_view> path_fun_a,
    std::pair<absl::string_view, absl::string_view> path_fun_b,
    Analysis* analysis) ->
    typename std::enable_if<
        !std::is_same<Generator, TestParams>::value,
        decltype(analysis->Summary(std::declval<std::ostream*>()))>::type {
  using GenResult =
      decltype(internal::CallAndTuplify()(generator, std::make_tuple()));

  auto fun_a = ExtractTimingFunction<ResultType, GenResult>(path_fun_a.first,
                                                            path_fun_a.second);

  auto fun_b = ExtractTimingFunction<ResultType, GenResult>(path_fun_b.first,
                                                            path_fun_b.second);

  return CompareFunctions(analysis->params(), std::move(generator),
                          std::move(fun_a.first), std::move(fun_b.first),
                          analysis->comparator(), analysis);
}

// This overloaded uses a temporary instance of its explicit `Analysis` type.
template <typename Analysis, typename ResultType = std::tuple<>,
          typename Generator>
auto CompareLibraries(
    const TestParams& params, Generator generator,
    std::pair<absl::string_view, absl::string_view> path_fun_a,
    std::pair<absl::string_view, absl::string_view> path_fun_b)
    -> decltype(
        Analysis(params), std::declval<Analysis>().comparator(),
        std::declval<Analysis>().Done(),
        std::declval<Analysis>().Summary(std::declval<std::ostream*>())) {
  using GenResult =
      decltype(internal::CallAndTuplify()(generator, std::make_tuple()));

  auto fun_a = ExtractTimingFunction<ResultType, GenResult>(path_fun_a.first,
                                                            path_fun_a.second);

  auto fun_b = ExtractTimingFunction<ResultType, GenResult>(path_fun_b.first,
                                                            path_fun_b.second);

  Analysis analysis(params);
  return CompareFunctions(params, std::move(generator), std::move(fun_a.first),
                          std::move(fun_b.first), analysis.comparator(),
                          &analysis);
}
}  // namespace bench
#endif /*!BENCH_COMPARE_LIBRARIES_H */
