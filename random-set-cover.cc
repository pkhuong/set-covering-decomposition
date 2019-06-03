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

namespace {
constexpr double kFeasEps = 5e-3;
constexpr size_t kNumTours = 1000 * 1000;
constexpr size_t kNumLocs = 10000;
constexpr size_t kMaxTourPerLoc = 2100;
constexpr size_t kMaxIter = 100000;
constexpr bool kCheckFeasible = false;

void OutputHistogram(absl::Span<const std::pair<std::string, double>> rows,
                     const double step = 2.5e-2) {
  for (const auto& row : rows) {
    std::cout << absl::StrFormat("%20s: %5.2f%% ", row.first, 100 * row.second);
    for (double x = 0.0; x + step / 2 < row.second; x += step) {
      std::cout << "*";
    }

    std::cout << "\n";
  }
}

void OutputSolutionStats(absl::Span<const double> solution,
                         double solution_scale) {
  size_t num_zero = 0;
  size_t num_one = 0;

  std::array<size_t, 25> buckets;
  buckets.fill(0);

  for (const double unscaled_value : solution) {
    const double value = solution_scale * unscaled_value;

    if (value <= 0) {
      ++num_zero;
      continue;
    }

    if (value >= 1.0) {
      ++num_one;
      continue;
    }

    ++buckets[static_cast<size_t>(buckets.size() * value)];
  }

  const double to_frac = 1.0 / solution.size();
  std::vector<std::pair<std::string, double>> rows = {
      {"0", to_frac * num_zero},
  };

  const double bucket_size = 1.0 / buckets.size();
  for (size_t i = 0; i < buckets.size(); ++i) {
    rows.emplace_back(
        absl::StrFormat("%4.2f-%4.2f", bucket_size * i, bucket_size * (i + 1)),
        to_frac * buckets[i]);
  }

  rows.emplace_back("1", to_frac * num_one);
  OutputHistogram(rows);
}
}  // namespace

int main(int, char**) {
  std::vector<double> cost;
  std::vector<std::vector<uint32_t>> coefs;

  {
    std::random_device dev;
    std::mt19937 rng(dev());

    std::uniform_int_distribution<size_t> num_tour_distribution(1,
                                                                kMaxTourPerLoc);
    std::uniform_real_distribution<> cost_distribution(0.0, 10.0);
    std::vector<uint32_t> tour_ids;
    for (size_t i = 0; i < kNumTours; ++i) {
      tour_ids.push_back(i);
      cost.push_back(cost_distribution(rng));
    }

    for (size_t i = 0; i < kNumLocs; ++i) {
      coefs.emplace_back();
      std::vector<uint32_t>& dst = coefs.back();

      // Perform a partial Fisher-Yates shuffle and consume
      // newly-generated elements as they come.
      std::uniform_real_distribution<> u01(0.0, 1.0);
      for (size_t j = 0, n = num_tour_distribution(rng); j < n; ++j) {
        size_t src = j + (tour_ids.size() - j) * u01(rng);
        std::swap(tour_ids[j], tour_ids[src]);
        dst.push_back(tour_ids[j]);
      }
    }
  }

  std::vector<CoverConstraint> constraints;
  for (const auto& tours : coefs) {
    constraints.emplace_back(tours);
  }

  DriverState state(cost);
  absl::Span<const double> solution;
  double solution_scale = 1.0;
  for (size_t i = 0; i < kMaxIter; ++i) {
    DriveOneIteration(absl::MakeSpan(constraints), &state);

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
    obj_value += solution_scale * solution[i] * cost[i];
  }

  double least_coverage = std::numeric_limits<double>::infinity();
  for (const auto& tours : coefs) {
    double coverage = 0.0;
    for (uint32_t tour : tours) {
      coverage += solution_scale * solution[tour];
    }

    least_coverage = std::min(least_coverage, coverage);
  }

  std::cout << "Final solution: Z=" << obj_value
            << " infeas=" << 1.0 - least_coverage << "\n";

  OutputSolutionStats(solution, solution_scale);
  return 0;
}
