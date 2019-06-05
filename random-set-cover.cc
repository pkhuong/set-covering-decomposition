#include "driver.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "random-set-cover-instance.h"
#include "set-cover-solver.h"

namespace {
constexpr double kFeasEps = 5e-3;
constexpr size_t kNumTours = 1000 * 1000;
constexpr size_t kNumLocs = 10000;
constexpr size_t kMaxTourPerLoc = 2100;
constexpr size_t kMaxIter = 100000;
constexpr bool kCheckFeasible = false;

void OutputHistogram(
    const absl::Span<const std::pair<std::string, double>> rows,
    bool cumulative, const double step = 2.5e-2) {
  double cdf_accumulator = 0;
  for (const auto& row : rows) {
    const double rhs = cumulative ? cdf_accumulator + row.second : row.second;
    cdf_accumulator += row.second;

    std::cout << absl::StrFormat("%20s: %7.3f%% ", row.first, 100 * rhs);
    if (row.second > 0.0) {
      if (row.second < step / 2) {
        std::cout << "'";
      } else {
        for (double x = 0.0; x + step / 2 <= row.second; x += step) {
          std::cout << "*";
        }
      }
    }

    std::cout << "\n";
  }
}

void OutputValuesStats(const absl::Span<const double> values,
                       const double scale = 1.0, const bool cumulative = false,
                       const double eps = kFeasEps) {
  size_t num_zero = 0;
  size_t num_almost_zero = 0;
  size_t num_almost_one = 0;
  size_t num_one = 0;

  std::array<size_t, 25> buckets;
  buckets.fill(0);

  for (const double unscaled_value : values) {
    const double value = scale * unscaled_value;

    if (value <= 0) {
      ++num_zero;
    } else if (value < kFeasEps) {
      ++num_almost_zero;
    } else if (value >= 1.0) {
      ++num_one;
    } else if (value > 1.0 - kFeasEps) {
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
  OutputHistogram(rows, cumulative);
}
}  // namespace

int main(int, char**) {
  RandomSetCoverInstance instance =
      GenerateRandomInstance(kNumTours, kNumLocs, kMaxTourPerLoc);

  SetCoverSolver solver(instance.obj_values,
                        absl::MakeSpan(instance.constraints));

  solver.Drive(kMaxIter, kFeasEps, kCheckFeasible);

  absl::MutexLock ml(&solver.state().mu);
  absl::Span<const double> solution(solver.state().current_solution);

  double obj_value = 0.0;
  for (size_t i = 0; i < solution.size(); ++i) {
    obj_value += solution[i] * instance.obj_values[i];
  }

  double least_coverage = std::numeric_limits<double>::infinity();
  {
    std::vector<double> infeas;
    infeas.reserve(instance.sets_per_value.size());
    for (const std::vector<uint32_t>& tours : instance.sets_per_value) {
      double coverage = 0.0;
      for (uint32_t tour : tours) {
        coverage += solution[tour];
      }

      least_coverage = std::min(least_coverage, coverage);
      infeas.push_back(1.0 - coverage);
    }

    std::cout << "Violation\n";
    OutputValuesStats(infeas, 1.0, /*cumulative=*/true);
    std::cout << "\n";
  }

  std::cout << "Final solution: Z=" << obj_value
            << " infeas=" << 1.0 - least_coverage << "\n";

  std::cout << "Solution\n";
  OutputValuesStats(solution);
  return 0;
}
