#include "driver.h"

#include <cstddef>
#include <iostream>
#include <tuple>

#include "absl/types/span.h"
#include "random-set-cover-instance.h"
#include "set-cover-solver.h"
#include "solution-stats.h"

namespace {
constexpr double kFeasEps = 5e-3;
constexpr size_t kNumTours = 1000 * 1000;
constexpr size_t kNumLocs = 10000;
constexpr size_t kMaxTourPerLoc = 2100;
constexpr size_t kMaxIter = 100000;
constexpr bool kCheckFeasible = false;
}  // namespace

int main(int, char**) {
  RandomSetCoverInstance instance =
      GenerateRandomInstance(kNumTours, kNumLocs, kMaxTourPerLoc);

  SetCoverSolver solver(instance.obj_values,
                        absl::MakeSpan(instance.constraints));

  solver.Drive(kMaxIter, kFeasEps, kCheckFeasible);

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
