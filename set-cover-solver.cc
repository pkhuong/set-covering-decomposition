#include "set-cover-solver.h"

#include <iostream>

#include "absl/time/time.h"

SetCoverSolver::SetCoverSolver(absl::Span<const double> obj_values,
                               absl::Span<CoverConstraint> constraints)
    : driver_(obj_values), obj_values_(obj_values), constraints_(constraints) {}

void SetCoverSolver::Drive(size_t max_iter, double eps, bool check_feasible) {
  for (size_t i = 0; i < max_iter; ++i) {
    DriveOneIteration(constraints_, &driver_);

    const bool done = (-driver_.prev_min_loss / driver_.num_iterations) < eps;
    const bool infeasible = !driver_.feasible;
    const bool relaxation_optimal =
        check_feasible && driver_.max_last_solution_infeasibility < eps &&
        driver_.last_solution_value <= driver_.best_bound + eps;

    std::vector<double> current_solution;
    {
      current_solution.reserve(driver_.sum_solutions.size());
      double scale = 1.0 / driver_.num_iterations;
      for (const double unscaled : driver_.sum_solutions) {
        current_solution.push_back(scale * unscaled);
      }
    }

    {
      absl::MutexLock ml(&state_.mu);
      state_.scalar.num_iterations = driver_.num_iterations;
      state_.scalar.done = done;
      state_.scalar.infeasible = infeasible;
      state_.scalar.relaxation_optimal = relaxation_optimal;
      state_.current_solution.swap(current_solution);
    }

    if (i < 10 || ((i + 1) % 100) == 0 || done || infeasible ||
        relaxation_optimal) {
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
