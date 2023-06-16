![License](https://img.shields.io/github/license/dparo/master-thesis)
![CI](https://img.shields.io/github/actions/workflow/status/dparo/master-thesis/ci.yml?branch=master)
![Release](https://img.shields.io/github/v/release/dparo/master-thesis)
![Downloads](https://img.shields.io/github/downloads/dparo/master-thesis/total)


# A Branch-and-Cut based Pricer for the Capacitated Vehicle Routing Problem

A Mixed Integer Programming (MIP) model and branch-and-cut framework algorithm for solving the CPTP.
The CPTP appears as the pricing sub-problem in column generation schemes for the CVRP.

## :bulb: About

The Capacitated Vehicle Routing Problem, CVRP for short,
is a combinatorial optimization routing problem in which,
a geographically distributed set of customers with associated known demands,
must be served with a fleet of vehicles stationed at a central facility.
In the last two decades,
column generation techniques embedded inside branch-price-and-cut frameworks
have been the de facto state-of-the-art dominant approach
for building exact algorithms for the CVRP.
The pricer, an important component in column generation, needs to solve
the so-called Pricing Problem (PP) which asks for an
Elementary Shortest Path Problem with Resource Constraints (ESPPRC)
in a reduced cost network.
Little scientific efforts have been dedicated in studying
branch-and-cut based approaches for tackling the PP.
The ESPPRC has been traditionally relaxed and solved through dynamic programming
algorithms.
This approach, however, has two main downsides.
First, it leads to a worsening of the obtained dual bounds.
Second, the running time deteriorates as the length of the generated paths increases.

Little scientific efforts were devoted at studying branch-and-cut approaches for tackling
the PP.
Jepsen et al. (2014) [[1]](#Jepsen2014) studied the CPTP problem in the context of pricing.
Almost ten years have passed from their work, and both, dynamic programming algorithms
and MIP optimizers have improved a lot.

This work revisits the original efforts of Jepsen et al. (2014) [[1]](#Jepsen2014)
and verifies whether branch-and-cut algorithms may be effectively employed
for tackling the PP.


## :rocket: Getting Started

### :anchor: Requirements
- A working `C` compiler
- `cmake`
- `git`
- `CPLEX>=12.10`: A free academic license can be downloaded at this [URL](https://www.ibm.com/academic/topic/data-science)

### :inbox_tray: Getting the source code
```bash
git clone --recursive https://github.com/dparo/master-thesis/
```
### :hammer: Building it
```
cd master-thesis
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
cmake --build ./ --config Release
```
### :wrench: Running the tests
```
ctest --progress --force-new-ctest-process --output-on-failure -C Release
```
### :car: Running the main solver
```
./src/cptp --help
```

## :mailbox_with_mail: Contact
- Insitutional EMAIL address: `davide.paro@studenti.unipd.it`
- Personal EMAIL address: `dparo@outlook.it`

## :bookmark_tabs: References
- <a id="Jepsen2014">[1]</a>
Jepsen, M. K., Petersen, B., Spoorendonk, S., & Pisinger, D. (2014). A branch-and-cut algorithm for the capacitated profitable tour problem. Discrete Optimization, 14, 78–96. https://doi.org/10.1016/j.disopt.2014.08.001
- <a id="baldacci2008exact">[2]</a>
Baldacci, R., Christofides, N., & Mingozzi, A. (2008). An exact algorithm for the vehicle routing problem based on the set partitioning formulation with additional cuts. Mathematical Programming, 115(2), 351–385.
- <a id="baldacci2011new">[3]</a>
Baldacci, R., Mingozzi, A., & Roberti, R. (2011). New route relaxation and pricing strategies for the vehicle routing problem. Operations Research, 59(5), 1269–1283.

## :libra: License
This project is licensed under the **MIT** license. Check the [LICENSE file](LICENSE).

This project makes use of thirdy party projects, located under the `deps` folder, licensed respectively in their corresponding licenses:
1. [rxi/log.c](https://github.com/rxi/log.c) [**MIT license**](https://github.com/rxi/log.c/blob/master/LICENSE)
2. [argtable3/argtable3](https://github.com/argtable3/argtable3) [**BSD license**](https://github.com/argtable/argtable3/blob/master/LICENSE)
3. [silentbicycle/greatest/](https://github.com/silentbicycle/greatest/) [**MIT license**](https://github.com/silentbicycle/greatest/blob/master/LICENSE)
4. [DavGamble/cJSON](https://github.com/DaveGamble/cJSON) [**MIT license**](https://github.com/DaveGamble/cJSON/blob/master/LICENSE)
5. [scottt/debugbreak](https://github.com/scottt/debugbreak) [**BSD2 license**](https://github.com/scottt/debugbreak/blob/master/COPYING)
6. [B-Con/crypto-algorithms](https://github.com/B-Con/crypto-algorithms) **Public domain**
7. [johthepro/doxygen-awesome-css](https://github.com/jothepro/doxygen-awesome-css) [**MIT license**](https://github.com/jothepro/doxygen-awesome-css/blob/main/LICENSE)

## :sparkling_heart: Funding
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/J3J47WJB2)
