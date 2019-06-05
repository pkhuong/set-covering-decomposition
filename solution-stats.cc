#include "solution-stats.h"

#include <assert.h>

#include <limits>

#include "absl/strings/str_format.h"

double ComputeObjectiveValue(absl::Span<const double> solution,
                             absl::Span<const double> costs) {
  assert(solution.size() == costs.size());

  double acc = 0;
  for (size_t i = 0; i < solution.size(); ++i) {
    acc += solution[i] * costs[i];
  }

  return acc;
}

std::pair<double, std::vector<double>> ComputeCoverInfeasibility(
    absl::Span<const double> solution,
    absl::Span<const std::vector<uint32_t>> constraints) {
  double least_coverage = std::numeric_limits<double>::infinity();
  std::vector<double> infeas;
  infeas.reserve(constraints.size());
  for (const std::vector<uint32_t>& sets : constraints) {
    double coverage = 0.0;
    for (uint32_t set : sets) {
      coverage += solution[set];
    }

    least_coverage = std::min(least_coverage, coverage);
    infeas.push_back(1.0 - coverage);
  }

  return std::make_pair(1.0 - least_coverage, std::move(infeas));
}

std::vector<std::pair<std::string, double>> BinValues(
    absl::Span<const double> values, size_t num_intervals, double eps) {
  size_t num_zero = 0;
  size_t num_almost_zero = 0;
  size_t num_almost_one = 0;
  size_t num_one = 0;

  std::vector<size_t> buckets;
  buckets.resize(num_intervals, 0);

  for (const double value : values) {
    if (value <= 0) {
      ++num_zero;
    } else if (value < eps) {
      ++num_almost_zero;
    } else if (value >= 1.0) {
      ++num_one;
    } else if (value > 1.0 - eps) {
      ++num_almost_one;
    } else {
      ++buckets[static_cast<size_t>(buckets.size() * value)];
    }
  }

  const double to_frac = 1.0 / values.size();
  std::vector<std::pair<std::string, double>> rows = {
      {"0", to_frac * num_zero}, {"< eps", to_frac * num_almost_zero}};

  const double bucket_size = 1.0 / buckets.size();
  for (size_t i = 0; i < buckets.size(); ++i) {
    rows.emplace_back(
        absl::StrFormat("%4.2f-%4.2f", bucket_size * i, bucket_size * (i + 1)),
        to_frac * buckets[i]);
  }

  rows.emplace_back("> 1 - eps", to_frac * num_almost_one);
  rows.emplace_back("1", to_frac * num_one);

  return rows;
}

void OutputHistogram(std::ostream& out,
                     absl::Span<const std::pair<std::string, double>> rows,
                     double step, bool cumulative) {
  double cdf_accumulator = 0;
  for (const auto& row : rows) {
    const double rhs = cumulative ? cdf_accumulator + row.second : row.second;
    cdf_accumulator += row.second;

    out << absl::StrFormat("%20s: %7.3f%% ", row.first, 100 * rhs);
    if (row.second > 0.0) {
      if (row.second < step / 2) {
        out << "'";
      } else {
        for (double x = 0.0; x + step / 2 <= row.second; x += step) {
          out << "*";
        }
      }
    }

    out << "\n";
  }
}
