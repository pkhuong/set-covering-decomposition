How do we evaluate proposals, and on what?
==========================================

Hopefully, most of our work will compute the same bits, just
faster. The question is then whether we improved the time per
iteration... and on what set of instances?

We definitely need (at least) a way to seed the instance generator's
PRNG and deterministically get the same instance.

Once we have that, I think we want to compare on a dense (5-10%
nonzeros) and sparse (0.1-1% nonzeros) instances.  The former should
be bottlenecked on the `exp` in weight updates, the latter on
quickselect.  Then there's the aspect ration (# sets VS # values).  We
should at least do 1:1, 2:1 and 1:2, and probably the 100:1 set:values
we're looking at for our endgoal of solving the CVRP set cover as fast
as an IPM.

I wrote [CSM](https://github.com/pkhuong/csm) for this kind of use
case.  I think I'll add a Kolmogorov-Smirnov test based on the same
law of iterated logarithms, but that's not essential here.

Given that we know our set of test instances and the metric we care
about, how we do make sure our performance improves or at least
remains neutral?  When we expect a performance improvement, that's
easy, the CSM will tell us.  We should also try to gauge how much
complexity (maybe additional LOC?) we're willing to incur for what
speed-up?  However, when we add performance neutral commits, we run
the risk of either failing flaky performance tests, or slowly
introducing regressions.

We should also compare performance against a couple fixed older
commits, and decide what slowdown we want to allow as "neutral
enough."

Time and iteration count
------------------------

More interesting changes will also affect what we do at each
iteration.  We should then check for regressions in either time or
iteration count.  If we can't always guarantee improvements in both
dimensions, here's a trade-off here: how many more iterations are we
willing to incur, in exchange for faster iterations?  If we only cared
about this solver, there would be no question.  However, I would like
to re-use what we learned here in a more generic solver, where the
subproblems (constraints) are general and usually more demanding than
finding the index of the min element in a vector.
