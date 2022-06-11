/*
 * Copyright (c) 2021 Davide Paro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <greatest.h>

#if COMPILED_WITH_CPLEX

#include "parser.h"
#include "solvers.h"
#include "core.h"
#include "core-utils.h"
#include "instances.h"

#define TIMELIMIT ((double)(600.0))
#define RANDOMSEED ((int32_t)0)

TEST creation(void) {
    const char *filepath = G_TEST_INSTANCES[0].filepath;
    Instance instance = parse(filepath);
    ASSERT(is_valid_instance(&instance));
    SolverParams params = {0};
    SolverTypedParams tparams = {0};
    bool resolved = resolve_params(&params, &MIP_SOLVER_DESCRIPTOR, &tparams);
    ASSERT(resolved == true);
    Solver solver =
        mip_solver_create(&instance, &tparams, TIMELIMIT, RANDOMSEED);
    ASSERT(solver.solve);
    ASSERT(solver.destroy);
    ASSERT(solver.data);
    solver.destroy(&solver);
    instance_destroy(&instance);
    solver_typed_params_destroy(&tparams);
    PASS();
}

TEST solve_test_instances(void) {
    for (int32_t i = 0; i < ARRAY_LEN_i32(G_TEST_INSTANCES); i++) {

        Instance instance = parse(G_TEST_INSTANCES[i].filepath);
        ASSERT(is_valid_instance(&instance));
        SolverParams params = {0};
        solver_params_append(&params, "AMORTIZED_FRACTIONAL_LABELING", "1");
        solver_params_append(&params, "NUM_THREADS", "1");
        Solution solution = solution_create(&instance);
        SolveStatus status = cptp_solve(&instance, "mip", &params, &solution,
                                        TIMELIMIT, RANDOMSEED);
        const bool success = status != 0 &&
                             BOOL(status & SOLVE_STATUS_CLOSED_PROBLEM) &&
                             !BOOL(status & SOLVE_STATUS_ERR);

        ASSERT(success);
        ASSERT(solution.dual_bound != -INFINITY);
        ASSERT(solution.primal_bound != +INFINITY);
        ASSERT(solution.tour.num_comps == 1);

        printf("%s :: Found primal_bound = %.17g,     Expected "
               "primal_bound = %.17g\n",
               G_TEST_INSTANCES[i].filepath, solution.primal_bound,
               G_TEST_INSTANCES[i].best_primal);
        ASSERT(
            feq(solution.primal_bound, G_TEST_INSTANCES[i].best_primal, 1e-3));
        instance_destroy(&instance);
        solution_destroy(&solution);
    }
    PASS();
}

#endif

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    log_set_level(LOG_WARN);

    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */
#if COMPILED_WITH_CPLEX
    RUN_TEST(creation);
    RUN_TEST(solve_test_instances);
#endif
    GREATEST_MAIN_END(); /* display results */
}
