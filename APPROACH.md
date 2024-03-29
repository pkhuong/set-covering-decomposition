The relaxation approach
=======================

I already covered the simpler [surrogate relaxation](https://pvk.ca/Blog/2019/04/23/fractional-set-covering-with-experts/)
approach on my blog.

This repository implements [surrogate *decomposition*](https://www.authorea.com/users/2541/articles/324048-no-regret-surrogate-decomposition?access_token=7PRDD1XaWLjc8wglr1_PLg).

The difference between relaxation in general and decomposition
specifically is that decomposition is the same relaxation, but applied
to a formulation with artificial decision variables and additional
constraints.

A surrogate relaxation approach takes a formulation of the form

        min c'x
    s.t.
        A x ≤ b
        D x ≤ e
     x ∈ X

and turns it into the "simpler"

        min c'x
    s.t.
        wA x ≤ wb
         D x ≤ e
     x ∈ X,

where `w` is a vector weights.

Arguably, this is only simpler in specialised cases.  In general,
we've destroyed so much structure that, even if we might have solved
the initial formulation with a specialised solver, we can't do
anything about the relaxation.  In the limit, we get a single
knapsack, and that's obviously amenable to knapsack solvers.

A decomposition approach preserves more of the structure, and [Monique
Guignard](https://link.springer.com/article/10.1007/BF02592954) showed
that, for Lagrangian relaxation, Lagrangian decomposition is never
weaker than Lagrangian relaxation (the formal definition is in the
paper, I can bore you about convex hulls offline).

I call the equivalent reformulation, in our surrogate relaxation
scheme, surrogate decomposition.  The idea is to take the same initial
formulation

        min c'x
    s.t.
        A x ≤ b
        D x ≤ e
     x ∈ X

then add artificial clones of the decision variables `x`,

        min c'x
    s.t.
        A x_a ≤ b
          x_a ≤ x  (*)
          x_a ≥ x  (*)
        D x_d ≤ e
          x_d ≤ x  (*)
          x_d ≥ x  (*)
       x ∈ X,
     x_a ∈ X_a ⊇ X,
     x_b ∈ X_b ⊇ X.

The pair of `≥` and `≤` constraints give us equality, so this is
clearly equivalent to the initial formulation, and no easier to solve.
The trick is to the relax the linking constraints marked with an
asterisk.  The formulation (after flipping the `≥` constraints)
becomes

        min c'x
    s.t.
          A x_a ≤ b
        x_a - x ≤ 0  (w_a-)
        x - x_a ≤ 0  (w_a+)
          D x_d ≤ e
        x_d - x ≤ 0  (w_d-)
        x - x_d ≤ 0  (w_d+)
       x ∈ X,
     x_a ∈ X_a ⊇ X,
     x_b ∈ X_b ⊇ X,

where the new linking constraints are now tagged with the
corresponding vector of weights.  We can smoosh the relaxed
constraints, and find

We can further smoosh the constraints in `w_a` and `w_d`:


        min c'x
    s.t.
        (w_a+ - w_a- + w_d+ - w_d-) x ≤ (w_a+ - w_a-)x_a  +(w_a+ - w_a-)x_d (*)
        A x_a ≤ b
        D x_d ≤ e
     x ∈ X, x_a ∈ X_a ⊇ X, x_b ∈ X_b ⊇ X,

and finally we have a way out.  For any fixed `x_a` and `x_d`, the
remaining subproblem in `x` is a pure knapsack problem, and is
minimised by maximising the right-hand side of the constraint `(*)`.
We can do so independently for `x_a` and `x_d`:

        max (w_a+ - w_a-)x_a
    s.t.
        A x_a ≤ b
     x_a ∈ X_a,

        max (w_d+ - w_d-)x_d
    s.t.
        D x_d ≤ e
     x_d ∈ X_d,

and solve the knapsack

        min c'x
    s.t.
        (w_a+ - w_a- + w_d+ - w_d-) x ≤ (w_a+ - w_a-)x_a  +(w_a+ - w_a-)x_d
     x ∈ X

once we have values for `x_a` and `x_d`.

I find this less direct reformulation interesting for multiple
reasons.  First, Guignard already proved that it's stronger than a
similar Lagrangian relaxation.  Second, we now have independent
subproblems that preserve the structure of the initial problem.
Third, the relaxed constraints have the same range of violation /
satisfaction (loss), `[-1, 1]`, regardless of the problem!

Philosophically, I really like how the decomposition is agnostic to
the specific linear constraints in A and D.  All we really care about
is that we have a minimisation oracle for each subdomain.  That oracle
might solve a linear program, an integer program, or solve a minimum
spanning tree, or a shortest path with regular code, or even punt to a
SAT solver.  This also means that we get cuts and strong formulations
for free in a lot of cases: instead of badly translating each
constraint to a MIP in order to solve for all constraints
simultaneously, we can solve each constraint naturally, and let the
surrogate decomposition algorithm assemble solutions to all the
subproblems and find a superoptimal point approximately in (the convex
hull of) all the constraints' feasible domains.

Rather than solving (a fractional relaxation of)

        min c'x
    s.t.
        A x ≤ b
        D x ≤ e
     x ∈ X,

we can solve the more general

        min c'x
    s.t.
        x ∈ F_A
        x ∈ F_B
     x ∈ X,

where `F_A` and `F_B` are arbitrary feasible sets for which we have
linear optimisation oracles that can solve

        min c'x
    s.t.
     x ∈ F_A ∩ X

The set covering solver in this repository implements an extreme form
of surrogate decomposition where each value to cover (each linear
covering constraint) gets its own copy of the decision variables.  It
also halves the number of relaxed constraints (and thus of experts) by
observing that it's always OK if some initial decision variable 
`x[i] > x_a[i]` for a constraint `a`.  Thus, we only need the
linking constraints `x ≥ x_a`, and not the symmetrical `x ≤ x_a`.
