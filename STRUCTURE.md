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
the next phase with an asterisk.  It's interesting that 5. is not a
barrier: we can feed weight vectors to step 1 incrementally.  As long
as we do so at a coarse enough granularity, the synchronisation
overhead won't be an issue (and somewhat coarser will let us do the
same thing on a GPU, I hope).

Another lucky accident is that steps 3-5 are the most tightly coupled
ones (they really depend on the underlying online learning algorithm),
while 1 and 2 can both be modularised... without really losing
opportunities for optimisation since we need a barrier at the end of
both steps 1 and 2.  If we want to abstract out the weight update
algorithm, the steps are really

0. Initialize weights (all 1s)

A. Solve subproblems while applying some update function to each subsolution.
B. Solve knapsack, and generate a full solution
C. Update weights.

It'd be nice to avoid the "update function" in step A, and we could
just buffer all updates.  However, I think we want the generality,
which lets us tile and pipeline some work.  Plus, a blind tail call
doesn't break modularity that badly.

I'm really not sure whether we actually want to pipeline the
subproblem updates with the generation of knapsack items: in general,
a gather is preferable to a scatter.  If we update knapsack items
after each subproblem, the update increments values at a bunch of
locations scattered in the item vector.  If we instead wait until we
have solutions to all the subproblems, each item can be update by
reading from a bunch of scattered locations.  When everything else is
equal, random reads tend to have higher throughput than random writes.

This observation should also inform how we lay our data out in memory.
It seems like we want an Array of Struct of Arrays, where each inner
SoA doesn't cross constraint boundaries.  These inner SoAs can also
form natural units of parallelisation and modularity, with opaque,
probably SIMD-ised, functions applied to each SoA.  Since we control
their size, we can make sure that the SoAs are properly aligned and
padded.

This SoA chunking doesn't have to conflict with the structure of the
knapsack solve.  A naive implementation just goes for quickselect on a
single linear array.  However, nothing prevents us from partitioning
each chunk independently, as long as we always agree on the pivot
element at each recursion step.
