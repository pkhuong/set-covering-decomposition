#include "bench/timing-function.h"
#include "bench/wrap-function.h"

// This slower Nop compiles to a direct call to `NopCallee`.  That
// introduces a bit more noise, but we should still get a useful
// result.
static __attribute__((__noinline__)) void NopCallee() { asm volatile(""); }
static const WRAP_FUNCTION(NopCallee) Nop;

DEFINE_MAKE_TIMING_FUNCTION(MakeNop, decltype(NopCallee), Nop);
