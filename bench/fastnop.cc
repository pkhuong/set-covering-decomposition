#include "bench/timing-function.h"

static const auto FastNop = [] {};
DEFINE_MAKE_TIMING_FUNCTION(MakeNop, decltype(FastNop), FastNop);
