Quickselect
===========

I see two kind-of-independent approaches to speed-up the linear
knapsack solver.

The first approach directly makes quickselect faster.  Can we pick our
pivots differently (sample more, or histogram the pivot values, or
even the inverse of the pivot values, since we can precompute the
inverse of the objective values, but not of the weights)?  Can we
partition faster, with SIMD instructions?  How about a 3-way
partition?  At what points does it make sense to implement a
distributed quickselect, with multiple threads each partitioning their
private vectors, based on commonly agreed pivot values?

The second approach tries to be more incremental.  We expect weights
to change slowly (and objective values not at all).  Can we use that
to re-use pivots across iterations, or to incremental sort ("crack")
the knapsack items?
