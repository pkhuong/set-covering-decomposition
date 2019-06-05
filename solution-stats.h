#ifndef SOLUTION_STATS_H
#define SOLUTION_STATS_H
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/span.h"

// Returns the cost of the `solution`.
double ComputeObjectiveValue(absl::Span<const double> solution,
                             absl::Span<const double> costs);

// Computes the coverage infeasibility for a set of coverage constraints,
// given the current solution.
//
// Returns the worst infeasibility and the vector of all
// infeasibilities.
std::pair<double, std::vector<double>> ComputeCoverInfeasibility(
    absl::Span<const double> solution,
    absl::Span<const std::vector<uint32_t>> constraints);

// Returns a frequency histogram of the `values`, which must be in the
// unit interval [0, 1].
//
// The values are split in `num_intervals` buckets, with special
// buckets for 0, < eps, 1, and > 1 - eps.
std::vector<std::pair<std::string, double>> BinValues(
    absl::Span<const double> values, size_t num_intervals, double eps);

// Prints a text representation of the histogram returned by
// `BinValues` to `out`. The representation will use one character per
// `step` in frequency.
//
// If `cumulative` is true, prints the cumulative distribution
// frequency rather than the probability density.
void OutputHistogram(std::ostream& out,
                     absl::Span<const std::pair<std::string, double>> rows,
                     double step = 2.5e-2, bool cumulative = false);
#endif /*!SOLUTION_STATS_H */
