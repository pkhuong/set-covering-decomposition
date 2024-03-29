#ifndef SET_COVER_SOLVER_H
#define SET_COVER_SOLVER_H
#include <cstddef>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "cover-constraint.h"
#include "driver.h"

// This class is thread-compatible.
class SetCoverSolver {
 public:
  struct ScalarState {
    size_t num_iterations{0};
    bool done{false};
    bool infeasible{false};
    bool relaxation_optimal{false};

    double sum_mix_gap{0.0};
    double min_loss{0.0};
    double max_loss{0.0};
    double best_bound{0.0};

    double sum_solution_value{0.0};
    double sum_solution_feasibility{0.0};

    double last_solution_value{0.0};

    absl::Duration total_time;
    absl::Duration prepare_time;
    absl::Duration knapsack_time;
    absl::Duration observe_time;
    absl::Duration update_time;

    absl::Duration last_iteration_time;
    absl::Duration last_prepare_time;
    absl::Duration last_knapsack_time;
    absl::Duration last_observe_time;
    absl::Duration last_update_time;
  };

  struct SolverState {
    mutable absl::Mutex mu;
    std::vector<double> current_solution GUARDED_BY(mu);

    ScalarState scalar GUARDED_BY(mu);
  };

  // Both spans must outlive this instance.
  SetCoverSolver(absl::Span<const double> obj_values,
                 absl::Span<CoverConstraint> constraints);

  SetCoverSolver(const SetCoverSolver&) = delete;
  SetCoverSolver(SetCoverSolver&&) = delete;
  SetCoverSolver& operator=(const SetCoverSolver&) = delete;
  SetCoverSolver& operator=(SetCoverSolver&&) = delete;

  const SolverState& state() const { return state_; }
  bool IsDone() const { return done_.HasBeenNotified(); }
  void WaitUntilDone() const { done_.WaitForNotification(); }

  // Solves until the average solution is `eps`-feasible, or
  // `max_iter` iterations have elapsed, whichever happens first.
  // Signals `done_` before returning.
  //
  // Updates the `state()` after each iteration. The `state()`'s
  // `current_solution` vector is only populated at the last
  // iteration, or `populate_solution_concurrently` is true.
  //
  // If `check_feasible` is true, also returns early whenever a
  // subproblem yields a solution that's both feasible and optimal.
  void Drive(size_t max_iter, double eps, bool check_feasible,
             bool populate_solution_concurrently = true);

 private:
  SolverState state_;

  DriverState driver_;
  absl::Span<const double> obj_values_;
  absl::Span<CoverConstraint> constraints_;
  absl::Notification done_;
};
#endif /*!SET_COVER_SOLVER_H */
