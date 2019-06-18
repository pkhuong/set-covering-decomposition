Weight update
=============

The conversion of cumulative loss to constraint weight is bottlenecked
on `exp(-step * (loss - min_loss))`, where `step` grows very slowly (<
1% per iteration), and `loss` and `min_loss` vary by at most 1 between
each iteration.  We also only care about the *normalised* weights, in
the end, and we know that at least one weight will be 1 (the
`min_loss` element), so we don't need that much precision.

How can we trade that precision for more speed?

The baseline vectorised `exp` doesn't seem great. Maybe we can steal
something from one of the machine learning libraries?

Simply knowing that the values change slowly means we can approximate
`exp(x0 + dx)` as `exp(x0) * fast_exp(dx)`, where `fast_exp` could be
as simple as a low order Maclaurin expansion.

However, I think there's even more to be gained by looking at the
change in arguments within each iteration.  I believe the first set of
calls to `exp` only affects `loss` and `min_loss`, not `step`; we can
use the same idea to compute the resulting change in weight with a
couple multiplications. The change to `min_loss` is global, and only
needs to be shifted to the exponential domain once.  Most (all but
one, for our set cover problem) changes to `loss` will be in `{-1, 0,
1}`, so that's easy.  When the change is fractional, we can enter a
slow path that recomputes the new weight from scratch.

The change to `step` is more annoying, since it's non-linear. I think
we'll have to resort to the generic `exp(x0) * fast_exp(dx)` trick
here, knowing that `dx` is tiny.
