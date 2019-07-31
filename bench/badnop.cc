#include <tuple>

#include "bench/timing-function.h"

static const auto FastNop = [] {};
// This nop is bad because the return value depends on the C++ "ABI".
static const auto BadNop = [] {
  (void)FastNop;
  return std::tuple<int, int>(0, 0);
};
DEFINE_MAKE_TIMING_FUNCTION(MakeNop, decltype(FastNop), BadNop);
