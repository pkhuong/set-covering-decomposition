Vectorisation and parallelisation
=================================

Grep for `XXX` or `vectori[a-z]+`.  There's a couple low-hanging
fruits.  I don't know if we want to link against a BLAS just for level
1 operations.  It's probably simpler to roll our own.

We may also find that there are too many short vectors, and we end up
spending most of our time in the fixup code for partial SIMD
registers.  In that case, we'll have to wait for the Array of Structs
of Arrays restructure.

Parallelisation isn't as dependent on the data layout.  We could start
with a simple parallel for, with threads and an atomic counter.  If we
restructure to AoSoA quickly enough, we may never need more.
Otherwise, I wonder how we'll manage load balancing while maximising
cache efficiency.
