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

#include <unity.h>

#if COMPILED_WITH_CPLEX

#include "parser.h"
#include "solvers/mip.h"
#include "core.h"
#include "core-utils.h"

static void test_mip_solver_create(void) {
    const char *filepath = "res/my.vrp";
    Instance instance = parse(filepath);
    instance_set_name(&instance, "test");
    Solver solver = mip_solver_create(&instance);
    TEST_ASSERT_NOT_NULL(solver.solve);
    TEST_ASSERT_NOT_NULL(solver.destroy);
    TEST_ASSERT_NOT_NULL(solver.data);
    solver.destroy(&solver);
    instance_destroy(&instance);
}

static void test_mip_solver_solve(void) {
    const char *filepath = "res/my.vrp";
    Instance instance = parse(filepath);
    SolverParams params = {0};
    Solution solution = cptp_solve(&instance, "mip", &params);
    TEST_ASSERT(solution.lower_bound != -INFINITY);
    TEST_ASSERT(solution.upper_bound != +INFINITY);
    TEST_ASSERT(*tour_num_comps(&solution.tour, 0) == 1);
    instance_destroy(&instance);
    solution_destroy(&solution);
}

#endif

int main(void) {
    UNITY_BEGIN();

#if COMPILED_WITH_CPLEX
    RUN_TEST(test_mip_solver_create);
    RUN_TEST(test_mip_solver_solve);
#endif

    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
