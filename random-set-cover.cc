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

  DriverState state(instance.obj_values);
  absl::Span<const double> solution;
  double solution_scale = 1.0;
  for (size_t i = 0; i < kMaxIter; ++i) {
    DriveOneIteration(absl::MakeSpan(instance.constraints), &state);

    const bool done = (-state.prev_min_loss / state.num_iterations) < kFeasEps;
    const bool infeasible = !state.feasible;
    const bool relaxation_optimal =
        kCheckFeasible && state.max_last_solution_infeasibility < kFeasEps &&
        state.last_solution_value <= state.best_bound + kFeasEps;

    if (i < 10 || ((i + 1) % 100) == 0 || done || infeasible ||
        relaxation_optimal) {
      const size_t num_it = i + 1;
      std::cout << "It " << num_it << ":"
                << " mix gap=" << state.sum_mix_gap << " max avg viol="
                << -state.prev_min_loss / state.num_iterations
                << " max avg feas="
                << state.prev_max_loss / state.num_iterations
                << " best bound=" << state.best_bound << " avg sol value="
                << state.sum_solution_value / state.num_iterations
                << " avg sol feasibility="
                << state.sum_solution_feasibility / state.num_iterations
                << " max last vio=" << state.max_last_solution_infeasibility
                << "\n";
      std::cout
          << "\t iter time=" << state.total_time / num_it << " prep time="
          << 100 * absl::FDivDuration(state.prepare_time, state.total_time)
          << "% ks time="
          << 100 * absl::FDivDuration(state.knapsack_time, state.total_time)
          << "% obs time="
          << 100 * absl::FDivDuration(state.observe_time, state.total_time)
          << "% upd time="
          << 100 * absl::FDivDuration(state.update_time, state.total_time)
          << "%.\n";
    }

    if (infeasible) {
      std::cout << "Infeasible!?!\n";
      return 0;
    }

    if (relaxation_optimal) {
      std::cout << "Feasible!\n";
      solution = absl::MakeSpan(state.last_solution);
      break;
    }

    if (done) {
      break;
    }
  }

  if (solution.empty()) {
    solution = absl::MakeSpan(state.sum_solutions);
    solution_scale = 1.0 / state.num_iterations;
  }

  double obj_value = 0.0;
  for (size_t i = 0; i < solution.size(); ++i) {
    obj_value += solution_scale * solution[i] * instance.obj_values[i];
  }

  double least_coverage = std::numeric_limits<double>::infinity();
  {
    std::vector<double> infeas;
    infeas.reserve(instance.sets_per_value.size());
    for (const std::vector<uint32_t>& tours : instance.sets_per_value) {
      double coverage = 0.0;
      for (uint32_t tour : tours) {
        coverage += solution_scale * solution[tour];
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
  OutputValuesStats(solution, solution_scale);
  return 0;
}
