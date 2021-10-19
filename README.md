![License](https://img.shields.io/github/license/dparo/master-thesis)
![CI](https://img.shields.io/github/workflow/status/dparo/master-thesis/CI)
![Release](https://img.shields.io/github/v/release/dparo/master-thesis)
![Downloads](https://img.shields.io/github/downloads/dparo/master-thesis/total)


# CPTP: Capacitated Profitable Tour Problem

A Mixed Integer Programming (MIP) based solution for the CPTP.

This project studies and compares various MIP formulations and cuts separation techniques for the CPTP.


## Core Technical Concepts/Inspiration

The CPTP is a subproblem that appears in the column generation procedure of the commonly known [Vehicle Routing Problem](https://en.wikipedia.org/wiki/Vehicle_routing_problem) (**VRP**).
Current state-of-the art solutions for CPTP are based on dynamic programming paradigms.
Substantial literature was devoted at studying the use of a MIP solver for solving the pricing problem in VRP [[1]](#Jepsen2014) [[2]](#baldacci2008exact) [[3]](#baldacci2011new).

In particular, in 2014, Jepsen [[1]](#Jepsen2014) studied extensively this problem by employing a MIP solver.
Almost ten years have passed, and commercial MIP solvers have made extensive progress.

This work tries to revisit, and if possible improve, the original work of Jepsen in 2014 [[1]](#Jepsen2014).


## Getting Started
- Requirements:
    - A working `C` compiler
    - `cmake`
    - `git`
    - `CPLEX v>=12.10`: A free academic license can be downloaded at this [URL](https://www.ibm.com/academic/topic/data-science)
- Getting the source code:
    ```bash
    git clone --recursive github.com/dparo/master-thesis
    ```
- Building it:
    ```
    cd master-thesis
    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ../
    cmake --build ./ --config Release
    ```
- Running the tests:
    ```
    ctest --progress --force-new-ctest-process --output-on-failure -C Release
    ```
- Running the main solver:
    ```
    ./src/cptp --help
    ```

## TODO
The project is currently under heavy-development.

## Contact
- Insitutional EMAIL address: `davide.paro@studenti.unipd.it`
- Personal EMAIL address: `dparo@outlook.it`

## References
- <a id="Jepsen2014">[1]</a>
Jepsen, M. K., Petersen, B., Spoorendonk, S., & Pisinger, D. (2014). A branch-and-cut algorithm for the capacitated profitable tour problem. Discrete Optimization, 14, 78–96. https://doi.org/10.1016/j.disopt.2014.08.001
- <a id="baldacci2008exact">[2]</a>
Baldacci, R., Christofides, N., & Mingozzi, A. (2008). An exact algorithm for the vehicle routing problem based on the set partitioning formulation with additional cuts. Mathematical Programming, 115(2), 351–385.
- <a id="baldacci2011new">[3]</a>
Baldacci, R., Mingozzi, A., & Roberti, R. (2011). New route relaxation and pricing strategies for the vehicle routing problem. Operations Research, 59(5), 1269–1283.

## License
This project is licensed under the **MIT** license. Check the [LICENSE file](LICENSE).

This project makes use of thirdy party projects (located under `deps`) licensed respectively in their corresponding licenses:
1. [rxi/log.c](github.com/rxi/log.c) [**MIT license**](https://github.com/rxi/log.c/blob/master/LICENSE)
2. [argtable3/argtable3](github.com/argtable3/argtable3) [**BSD license**](https://github.com/argtable/argtable3/blob/master/LICENSE)
3. [ThrowTheSwitch/Unity](github.com/ThrowTheSwitch/Unity) [**MIT license**](https://github.com/ThrowTheSwitch/Unity/blob/master/LICENSE.txt)
