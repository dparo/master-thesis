# CPTP

## The pricing problem
Find a violated column dynamically to feed a branch and price algorithm.
Columns are generated dynamically in the pricing procedure.
The problem in VRP is identical to find a tour that has an associated negative objective function.
If no such tour exists, no violated column exists for the branch-and-price.
Exact methods are needed to prove that no more violated tour exists (namely dynamic programming and MIP based solvers).
Heuristic based solutions for the pricing problem may still be used to quickly assess and find ""easy"" violated tours.
Heuristic solutions are especially usefull at the very first iterations of the branch and price, where finding any violated solution is relatively easy.
As the branch-and-price evolve, finding violated tours for the CVRP becomes harder and harder.
This is where a MIP based CPTP problem solver might provide a reasonable impact.

- NOTE: Unlike a branch and cut procedure, the branch and price cannot be early-terminated (eg CTRL-C) to get a still reasonable lower bound.
    In fact, the lower bounds produced from a branch and price procedure might oscillate up and down wrt to the optimal solution.
    The solution cannot be proven optimal, nor we can make any informative guess about the lower bound until we show that no more violated columns exist.

## VRPSolver
- Disable the dynamic cuts generation (either command line, or by altering the control flow of the source code)
- Everytime the solver calls the dynamic programming algorithm (labeling) dump the time it takes and the dual formulation.
    - Use the dual formulation as input to our CPTP MIP based solver
    - Compare the dumped time of the labeling algorithm of the VRPSolver and compare it against our CPTP MIP based solver
