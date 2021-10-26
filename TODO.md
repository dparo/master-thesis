# VRPSolver:
- Disable the dynamic cuts generation (either command line, or by altering the control flow of the source code)
- Everytime the solver calls the dynamic programming algorithm (labeling) dump the time it takes and the dual formulation.
    - Use the dual formulation as input to our CPTP MIP based solver
    - Compare the dumped time of the labeling algorithm of the VRPSolver and compare it against our CPTP MIP based solver
