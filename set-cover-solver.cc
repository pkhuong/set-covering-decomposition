#include "set-cover-solver.h"

#include <iostream>

#include "absl/time/time.h"

SetCoverSolver::SetCoverSolver(absl::Span<const double> obj_values,
                               absl::Span<CoverConstraint> constraints)
    : driver_(obj_values), obj_values_(obj_values), constraints_(constraints) {}

void SetCoverSolver::Drive(size_t max_iter, double eps, bool check_feasible,
                           bool populate_solution_concurrently) {
  for (size_t i = 0; i < max_iter; ++i) {
    DriveOneIteration(constraints_, &driver_);

    const bool done = (-driver_.prev_min_loss / driver_.num_iterations) < eps;
    const bool infeasible = !driver_.feasible;
    const bool relaxation_optimal =
        check_feasible && driver_.max_last_solution_infeasibility < eps &&
        driver_.last_solution_value <= driver_.best_bound + eps;

    const bool last_iteration =
        done || infeasible || relaxation_optimal || (i + 1) >= max_iter;

    std::vector<double> current_solution;
    if (last_iteration || populate_solution_concurrently) {
      // XXX: vectorize.
      current_solution.reserve(driver_.sum_solutions.size());
      double scale = 1.0 / driver_.num_iterations;
      for (const double unscaled : driver_.sum_solutions) {
        current_solution.push_back(scale * unscaled);
      }
    }

    if (state_.mu.TryLock()) {
      state_.current_solution.swap(current_solution);

      state_.scalar.num_iterations = driver_.num_iterations;
      state_.scalar.done = done;
      state_.scalar.infeasible = infeasible;
      state_.scalar.relaxation_optimal = relaxation_optimal;

      state_.scalar.sum_mix_gap = driver_.sum_mix_gap;
      state_.scalar.min_loss = driver_.prev_min_loss;
      state_.scalar.max_loss = driver_.prev_max_loss;
      state_.scalar.best_bound = driver_.best_bound;

      state_.scalar.sum_solution_value = driver_.sum_solution_value;
      state_.scalar.sum_solution_feasibility = driver_.sum_solution_feasibility;

      state_.scalar.last_solution_value = driver_.last_solution_value;

      state_.scalar.total_time = driver_.total_time;
      state_.scalar.prepare_time = driver_.prepare_time;
      state_.scalar.knapsack_time = driver_.knapsack_time;
      state_.scalar.observe_time = driver_.observe_time;
      state_.scalar.update_time = driver_.update_time;

      state_.scalar.last_iteration_time = driver_.last_iteration_time;
      state_.scalar.last_prepare_time = driver_.last_prepare_time;
      state_.scalar.last_knapsack_time = driver_.last_knapsack_time;
      state_.scalar.last_observe_time = driver_.last_observe_time;
      state_.scalar.last_update_time = driver_.last_update_time;

      state_.mu.Unlock();
    }

    if (i < 10 || ((i + 1) % 100) == 0 || last_iteration) {
      const size_t num_it = i + 1;
      std::cout << "It " << num_it << ":"
                << " mix gap=" << driver_.sum_mix_gap << " max avg viol="
                << -driver_.prev_min_loss / driver_.num_iterations
                << " max avg feas="
                << driver_.prev_max_loss / driver_.num_iterations
                << " best bound=" << driver_.best_bound << " avg sol value="
                << driver_.sum_solution_value / driver_.num_iterations
                << " avg sol feasibility="
                << driver_.sum_solution_feasibility / driver_.num_iterations
                << " max last vio=" << driver_.max_last_solution_infeasibility
                << "\n";
      std::cout
          << "\t iter time=" << driver_.total_time / num_it << " prep time="
          << 100 * absl::FDivDuration(driver_.prepare_time, driver_.total_time)
          << "% ks time="
          << 100 * absl::FDivDuration(driver_.knapsack_time, driver_.total_time)
          << "% obs time="
          << 100 * absl::FDivDuration(driver_.observe_time, driver_.total_time)
          << "% upd time="
          << 100 * absl::FDivDuration(driver_.update_time, driver_.total_time)
          << "%.\n";
    }

    if (infeasible) {
      std::cout << "Infeasible!?!\n";
      break;
    }

    if (relaxation_optimal) {
      std::cout << "Feasible!\n";
      std::vector<double> last_solution(driver_.last_solution.begin(),
                                        driver_.last_solution.end());
      absl::MutexLock ml(&state_.mu);
      state_.current_solution.swap(last_solution);
      break;
    }

    if (done) {
      break;
    }
  }

  done_.Notify();
}
