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

#include "solvers.h"

static SolveStatus solve(Solver *self, const Instance *instance,
                         Solution *solution, int64_t begin_time) {
    UNUSED_PARAM(self);
    UNUSED_PARAM(instance);
    UNUSED_PARAM(solution);
    UNUSED_PARAM(begin_time);
    return SOLVE_STATUS_NULL;
}

static void destroy(ATTRIB_MAYBE_UNUSED Solver *self) {}

Solver stub_solver_create(const Instance *instance, SolverTypedParams *tparams,
                          double timelimit, int32_t randomseed) {
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tparams);
    UNUSED_PARAM(timelimit);
    UNUSED_PARAM(randomseed);
    Solver solver = {0};
    solver.solve = solve;
    solver.destroy = destroy;
    return solver;
}
