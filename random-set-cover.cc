#include "driver.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>

static constexpr double kFeasEps = 5e-3;
static constexpr size_t kNumTours = 1000 * 1000;
static constexpr size_t kNumLocs = 10000;
static constexpr size_t kMaxTourPerLoc = 2100;
static constexpr size_t kMaxIter = 100000;
static constexpr bool kCheckFeasible = false;

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
  return 0;
}
