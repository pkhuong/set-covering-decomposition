High-level structure
====================

I think most of the computations here are well suited to GPUs, but I
also think that more general subproblems (constraints) may work better
on CPUs so I'd like to come up with a structure that can cope with
slow CPU <-> GPU transfers.  In general, it seems like GPU
compatibility is also a good forcing function to prevent us from
getting stuck in callback hell or fine-grained (read
strongly instance-dependent) parallelism.

The initial loop does:

1. Convert loss vectors to weights
2. Solve subproblems
3. Solve knapsack
4. Update loss
5. Re-compute weights to update the mix loss.

I think we want to rotate this a bit, and instead do

0. Compute initial weights (all 1s)

1. Solve subproblems \*
2. Solve knapsack \*
3. Update loss
4. Re-compute weights \*
5. Convert loss vectors to weights.

I marked the steps for which we need the full result before starting
the next phase with an astedisk.  It's interesting that 5. is not a
barrier: we can feed weight vectors to step 1 incrementally.  As long
as we do so at a coarse enough granularity, the synchronisation
overhead won't be an issue (and somewhat coarser will let us do the
same thing on a GPU, I hope).

This observation should also inform how we lay our data out in memory.
It seems like we want an Array of Struct of Arrays, where each inner
SoA doesn't cross constraint boundaries.  These inner SoAs can also
form natural units of parallelisation, with SIMD applied to each SoA.
Since we control their size, we can make sure that the SoAs are
properly aligned and padded.

This SoA chunking doesn't have to conflict with the structure of the
knapsack solve.  A naive implementation just goes for quickselect on a
single linear array.  However, nothing prevents us from partition each
chunk independently, as long as we always settle on the same pivot
element at each recursion step.
