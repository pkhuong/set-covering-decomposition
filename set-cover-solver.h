#ifndef SET_COVER_SOLVER_H
#define SET_COVER_SOLVER_H
#include <cstddef>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "cover-constraint.h"
#include "driver.h"

// This class is thread-compatible.
class SetCoverSolver {
 public:
  struct SolverState {
    mutable absl::Mutex mu;
    size_t num_iterations GUARDED_BY(mu){0};
    bool done GUARDED_BY(mu);
    bool infeasible GUARDED_BY(mu);
    bool relaxation_optimal GUARDED_BY(mu);

    std::vector<double> current_solution GUARDED_BY(MU);
  };

  // Both spans must outlive this instance.
  SetCoverSolver(absl::Span<const double> obj_values,
                 absl::Span<CoverConstraint> constraints);

  SetCoverSolver(const SetCoverSolver&) = delete;
  SetCoverSolver(SetCoverSolver&&) = delete;
  SetCoverSolver& operator=(const SetCoverSolver&) = delete;
  SetCoverSolver& operator=(SetCoverSolver&&) = delete;

  const SolverState& state() const { return state_; }
  void WaitUntilDone() const { done_.WaitForNotification(); }

  // Solves until the average solution is `eps`-feasible, or
  // `max_iter` iterations have elapsed, whichever happens first.
  // Signals `done_` before returning.
  //
  // Updates the `state()` after each iteration.
  //
  // If `check_feasible` is true, also returns early whenever a
  // subproblem yields a solution that's both feasible and optimal.
  void Drive(size_t max_iter, double eps, bool check_feasible = false);

 private:
  SolverState state_;

  DriverState driver_;
  absl::Span<const double> obj_values_;
  absl::Span<CoverConstraint> constraints_;
  absl::Notification done_;
};
#endif /*!SET_COVER_SOLVER_H */