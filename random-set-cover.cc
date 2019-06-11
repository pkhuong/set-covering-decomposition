#include "driver.h"

#include <cstddef>
#include <iostream>
#include <tuple>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/types/span.h"
#include "random-set-cover-flags.h"
#include "random-set-cover-instance.h"
#include "set-cover-solver.h"
#include "solution-stats.h"

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  const double kFeasEps = absl::GetFlag(FLAGS_feas_eps);

  RandomSetCoverInstance instance = GenerateRandomInstance(
      absl::GetFlag(FLAGS_num_sets), absl::GetFlag(FLAGS_num_values),
      absl::GetFlag(FLAGS_min_set_per_value),
      absl::GetFlag(FLAGS_max_set_per_value));

  SetCoverSolver solver(instance.obj_values,
                        absl::MakeSpan(instance.constraints));

  solver.Drive(absl::GetFlag(FLAGS_max_iter), kFeasEps,
               absl::GetFlag(FLAGS_check_feasible),
               /*populate_solution_concurrently=*/false);

  absl::MutexLock ml(&solver.state().mu);
  absl::Span<const double> solution(solver.state().current_solution);

  const double obj_value = ComputeObjectiveValue(solution, instance.obj_values);

  double max_infeas;
  {
    std::vector<double> infeas;
    std::tie(max_infeas, infeas) =
        ComputeCoverInfeasibility(solution, instance.sets_per_value);
    std::cout << "Violation\n";
    OutputHistogram(std::cout, BinValues(infeas, 25, kFeasEps),
                    /*step=*/2.5e-2, /*cumulative=*/true);
    std::cout << "\n";
  }

  {
    std::cout << "Solution\n";
    OutputHistogram(std::cout, BinValues(solution, 25, kFeasEps));
    std::cout << "\n";
  }

  std::cout << "Final solution: Z=" << obj_value << " infeas=" << max_infeas
            << "\n";
  return 0;
}
