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

/// Struct that is used as a userhandle to be passed to the cplex generic
/// callback
typedef struct {
    Solver *solver;
    Instance *instance;
} CplexCallbackData;

void mip_solver_destroy(Solver *self) {

    if (self->data) {
        if (self->data->lp) {
            CPXXfreeprob(self->data->env, &self->data->lp);
        }

        if (self->data->env) {
            CPXXcloseCPLEX(&self->data->env);
        }

        free(self->data);
    }

    memset(self, 0, sizeof(*self));
    self->destroy = mip_solver_destroy;
}

static inline double cost(const Instance *instance, int32_t idx1,
                          int32_t idx2) {
    return vec2d_dist(&instance->positions[idx1], &instance->positions[idx2]);
}

static inline size_t get_x_mip_var_idx(const Instance *instance, int32_t i,
                                       int32_t j) {
    assert(i >= 0 && i < instance->num_customers + 1);
    assert(i >= 0 && j < instance->num_customers + 1);

    size_t N = (size_t)instance->num_customers + 1;
    size_t d = ((size_t)(i + 1) * (size_t)(i + 2)) / 2;
    size_t result = i * N + j - d;
    return result;
}

static inline size_t get_y_mip_var_idx(const Instance *instance, int32_t i) {

    assert(i >= 0 && i < instance->num_customers + 1);
    return (size_t)i + get_x_mip_var_idx(instance, instance->num_customers,
                                         instance->num_customers);
}

bool build_mip_formulation(Solver *self, const Instance *instance) {
    bool result = true;

    //
    // Create all the MIP variables that we need first (eg add the columns)
    //

    char cname[128];
    const char *pcname[] = {(const char *)cname};

    double obj[1];
    double lb[1] = {0.0};
    double ub[1] = {1.0};
    char xctype[] = {'B'};

    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        for (int32_t j = 0; j < instance->num_customers + 1; j++) {
            if (i == j)
                continue;

            snprintf(cname, sizeof(cname), "x(%d,%d)", i, j);
            obj[0] = cost(instance, i, j);

            if (CPXXnewcols(self->data->env, self->data->lp, 1, obj, lb, ub,
                            xctype, pcname)) {
                log_fatal("%s :: CPXXnewcols returned an error", __func__);
                return false;
            }
        }
    }

    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        snprintf(cname, sizeof(cname), "y(%d)", i);
        obj[0] = instance->duals[i];

        if (CPXXnewcols(self->data->env, self->data->lp, 1, obj, lb, ub, xctype,
                        pcname)) {
            log_fatal("%s :: CPXXnewcols returned an error", __func__);
            return false;
        }
    }

    //
    // Now create the constraints (eg add the rows)
    //

    return result;
}

static void add_sec(ATTRIB_MAYBE_UNUSED Solver *self,
                    ATTRIB_MAYBE_UNUSED const Instance *instance) {}

static inline int
cplex_on_new_candidate(CPXCALLBACKCONTEXTptr context,
                       ATTRIB_MAYBE_UNUSED Solver *solver,
                       ATTRIB_MAYBE_UNUSED const Instance *intsance) {
    // NOTE:
    //      Called when cplex has a new feasible integral solution satisfying
    //      all constraints
    return 0;
}

static inline int
cplex_on_new_relaxation(CPXCALLBACKCONTEXTptr context,
                        ATTRIB_MAYBE_UNUSED Solver *solver,
                        ATTRIB_MAYBE_UNUSED const Instance *intsance) {

    // NOTE:
    //      Called when cplex has a new feasible LP solution (not necessarily
    //      satisfying the integrality constraints)
    return 0;
}

static inline int
cplex_on_global_progress(CPXCALLBACKCONTEXTptr context,
                         ATTRIB_MAYBE_UNUSED Solver *solver,
                         ATTRIB_MAYBE_UNUSED const Instance *intsance) {
    double obj, bound;
    CPXLONG num_processed_nodes, simplex_iterations;
    CPXXcallbackgetinfodbl(context, CPXCALLBACKINFO_BEST_SOL, &obj);
    CPXXcallbackgetinfodbl(context, CPXCALLBACKINFO_BEST_BND, &bound);
    CPXXcallbackgetinfolong(context, CPXCALLBACKINFO_NODECOUNT,
                            &num_processed_nodes);
    CPXXcallbackgetinfolong(context, CPXCALLBACKINFO_ITCOUNT,
                            &simplex_iterations);
    log_info("%s :: num_processed_nodes = %lld, simplex_iterations = %lld, "
             "best_sol = %f, best_bound = %f\n",
             __func__, num_processed_nodes, simplex_iterations, obj, bound);
    return 0;
}

CPXPUBLIC static int cplex_callback(CPXCALLBACKCONTEXTptr context,
                                    CPXLONG contextid, void *userhandle) {
    CplexCallbackData *data = (CplexCallbackData *)userhandle;

    switch (contextid) {
    case CPX_CALLBACKCONTEXT_CANDIDATE:
        return cplex_on_new_candidate(context, data->solver, data->instance);
        break;
    case CPX_CALLBACKCONTEXT_RELAXATION:
        return cplex_on_new_relaxation(context, data->solver, data->instance);
        break;
    case CPX_CALLBACKCONTEXT_GLOBAL_PROGRESS:
        return cplex_on_global_progress(context, data->solver, data->instance);
        break;
    case CPX_CALLBACKCONTEXT_THREAD_UP:
        break;
    case CPX_CALLBACKCONTEXT_THREAD_DOWN:
        break;
    default:
        assert(!"Invalid case");
        break;
    }

    return 0;
}

Solution solve(ATTRIB_MAYBE_UNUSED struct Solver *self,
               ATTRIB_MAYBE_UNUSED const Instance *instance) {

    // TODO: CPlex solve here

    // TODO: CPlex verify gap

    // TODO: CPlex ask lower bound and upper bound

    // TODO: CPlex convert mip variables into usable solution

    // TODO: Return solution

    return (Solution){};
}

bool cplex_setup(Solver *solver, const Instance *instance) {
    int status_p = 0;

    solver->data->env = CPXXopenCPLEX(&status_p);
    log_trace("%s :: CPXopenCPLEX returned status_p = %d, env = %p\n", __func__,
              status_p, solver->data->env);
    if (status_p != 0 || !solver->data->env) {
        goto fail;
    }

    log_info("%s :: CPLEX version is %s", __func__,
             CPXXversion(solver->data->env));

    solver->data->lp =
        CPXXcreateprob(solver->data->env, &status_p,
                       instance->name ? instance->name : "UNNAMED");
    if (status_p != 0 || !solver->data->lp) {
        log_fatal("CPXcreateprob FAILURE :: returned status_p: %d", status_p);
        goto fail;
    }

    return true;

fail:
    solver->destroy(solver);
    return false;
}

Solver mip_solver_create(ATTRIB_MAYBE_UNUSED Instance *instance) {
    log_trace("%s", __func__);

    Solver solver = {0};
    solver.solve = solve;
    solver.destroy = mip_solver_destroy;
    solver.data = calloc(1, sizeof(*solver.data));

    if (!cplex_setup(&solver, instance)) {
        log_fatal("%s : Failed to initialize cplex", __func__);
        goto fail;
    }

    if (!build_mip_formulation(&solver, instance)) {
        log_fatal("%s : Failed to build mip formulation", __func__);
        goto fail;
    }

    return solver;
fail:
    if (solver.destroy)
        solver.destroy(&solver);
    return (Solver){0};
}

#endif
