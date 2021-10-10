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

#include "mip.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef COMPILED_WITH_CPLEX
Solver mip_solver_create(ATTRIB_MAYBE_UNUSED Instance *instance) {
    fprintf(stderr,
            "%s: Cannot use mip solver as the program was not compiled with "
            "CPLEX\n",
            __FILE__);
    fflush(stderr);
    abort();
    return (Solver){};
}
#else

// NOTE: cplexx is the 64 bit version of the API, while clex (one x) is the 32
//       bit version of the API.
#include <ilcplex/cplexx.h>
#include <ilcplex/cpxconst.h>

#include <string.h>
#include "log.h"

typedef struct SolverData {
    CPXENVptr env;
    CPXLPptr lp;
} SolverData;

void mip_solver_destroy(Solver *self) {

    if (self->data->lp) {
        CPXXfreeprob(self->data->env, &self->data->lp);
    }

    if (self->data->env) {
        CPXXcloseCPLEX(&self->data->env);
    }

    if (self->data) {
        free(self->data);
    }

    memset(self, 0, sizeof(*self));
}

Solution solve(ATTRIB_MAYBE_UNUSED struct Solver *self,
               ATTRIB_MAYBE_UNUSED const Instance *instance) {
    // TODO:: Implement me
    return (Solution){};
}

Solver mip_solver_create(ATTRIB_MAYBE_UNUSED Instance *instance) {

    Solver solver = {0};
    solver.solve = solve;
    solver.destroy = mip_solver_destroy;

    int status_p = 0;

    log_trace("%s", __func__);

    solver.data = calloc(1, sizeof(*solver.data));
    solver.data->env = CPXXopenCPLEX(&status_p);

    log_trace("%s :: CPXopenCPLEX returned status_p = %d, env = %p\n", __func__,
              status_p, solver.data->env);

    if (!status_p && solver.data->env) {
        log_info("%s :: CPLEX version is %s", __func__,
                 CPXXversion(solver.data->env));
        fflush(stdout);

        solver.data->lp =
            CPXXcreateprob(solver.data->env, &status_p,
                           instance->name ? instance->name : "UNNAMED");
        if (!status_p && solver.data->lp) {
        } else {
            solver.destroy(&solver);
            FATAL("CPXcreateprob FAILURE :: returned status_p: %d", status_p);
        }
    } else {
        FATAL("CPXopenCPLEX FAILURE");
    }

    return solver;
}

#endif
