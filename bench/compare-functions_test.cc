#include "compare-functions.h"

#include <tuple>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "bench/test-params.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace {
using ::testing::AllOf;
using ::testing::Gt;
using ::testing::Le;

class DummyAnalysis {
 public:
  explicit DummyAnalysis(const TestParams&) {}

  bool Done() const { return false; }

  void Observe(absl::Span<double> deltas) {
    for (double delta : deltas) {
      if (++count_ < 10) {
        std::cout << "Delta " << delta << "\n";
      }
    }
  }

  void Summary(std::ostream*) {}

 private:
  size_t count_{0};
};

// None of the tests have assertions. We're mostly checking that they
// build and don't crash / hang.  We get correctness out of the
// statistical tests.
TEST(CompareFunctions, SmokeTest) {
  CompareFunctions<DummyAnalysis>(
      TestParams().SetMaxComparisons(10).SetNumThreads(1),
      [] {
        return std::vector<int>{1, 2, 3};
      },
      [](const std::vector<int>& x) -> size_t { return 0; },
      [](const std::vector<int>& x) {
        int min_value = x[0];
        size_t min_index = 0;
        for (size_t i = 1; i < x.size(); ++i) {
          if (x[i] < min_value) {
            min_value = x[i];
            min_index = i;
          }
        }

        return min_index;
      },
      [](std::tuple<uint64_t, size_t> x, std::tuple<uint64_t, size_t> y,
         const std::vector<int>&) {
        return 1.0 * std::get<0>(x) - std::get<0>(y);
      });
}

TEST(CompareFunctions, BadUsageTest) {
  struct NonConstFunctor {
    size_t operator()(const std::vector<int>& x) { return 0; }
  };

  struct LargeConstFunctor {
    size_t operator()(const std::vector<int>& x) const { return 0; }

    char buf[65];
  };

  CompareFunctions<DummyAnalysis>(
      TestParams().SetMaxComparisons(10).SetNumThreads(1),
      [] {
        return std::vector<int>{1, 2, 3};
      },
      NonConstFunctor(), LargeConstFunctor(),
      [](std::tuple<uint64_t, size_t> x, std::tuple<uint64_t, size_t> y,
         const std::vector<int>&) {
        return 1.0 * std::get<0>(x) - std::get<0>(y);
      });
}

TEST(CompareFunctions, WithThreads) {
  class Analysis {
   public:
    explicit Analysis(const TestParams&) {}

    bool Done() const { return false; }

    void Observe(absl::Span<double> deltas) {
      for (auto delta : deltas) {
        if (count_ == 0) {
          absl::SleepFor(absl::Milliseconds(100));
        }
        if (++count_ < 10) {
          std::cout << "Delta " << delta << "\n";
        }
      }
    }

    void Summary(std::ostream*) {}

   private:
    size_t count_ = 0;
  };

  CompareFunctions<Analysis>(
      TestParams().SetMaxComparisons(200).SetNumThreads(4),
      [] {
        return std::vector<int>{1, 2, 3};
      },
      [](const std::vector<int>& x) -> size_t { return 0; },
      [](const std::vector<int>& x) {
        int min_value = x[0];
        size_t min_index = 0;
        for (size_t i = 1; i < x.size(); ++i) {
          if (x[i] < min_value) {
            min_value = x[i];
            min_index = i;
          }
        }

        return min_index;
      },
      [](std::tuple<uint64_t, size_t> x, std::tuple<uint64_t, size_t> y,
         const std::vector<int>&) {
        return 1.0 * std::get<0>(x) - std::get<0>(y);
      });
}

TEST(CompareFunctions, WithYield) {
  CompareFunctions<DummyAnalysis>(
      TestParams().SetMaxComparisons(1000).SetNumThreads(4),
      [] {
        return std::vector<int>{1, 2, 3};
      },
      [](const std::vector<int>& x) -> size_t {
        absl::SleepFor(absl::Milliseconds(10));
        return 0;
      },
      [](const std::vector<int>& x) {
        int min_value = x[0];
        size_t min_index = 0;
        for (size_t i = 1; i < x.size(); ++i) {
          if (x[i] < min_value) {
            min_value = x[i];
            min_index = i;
          }
        }

        return min_index;
      },
      [](std::tuple<uint64_t, size_t> x, std::tuple<uint64_t, size_t> y,
         const std::vector<int>&) {
        return 1.0 * std::get<0>(x) - std::get<0>(y);
      });
}
}  // namespace
}  // namespace bench
