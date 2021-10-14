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
#include "core-utils.h"

#ifndef COMPILED_WITH_CPLEX
Solver mip_solver_create(Instance *instance) {
    UNUSED_PARAM(instance);
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

static void show_lp_file(Solver *self) {
    CPXXwriteprob(self->data->env, self->data->lp, "TEST.lp", NULL);
    system("kitty -e nvim TEST.lp");
}

/// Struct that is used as a userhandle to be passed to the cplex generic
/// callback
typedef struct {
    Solver *solver;
    const Instance *instance;
} CplexCallbackData;

static int32_t *num_connected_comps(Tour *tour) {
    return &tour->num_connected_comps[0];
}

static inline int32_t *succ(Tour *tour, int32_t i) {
    return tour_succ(tour, 0, i);
}

static inline int32_t *comp(Tour *tour, int32_t i) {
    return tour_comp(tour, 0, i);
}

static inline double cost(const Instance *instance, int32_t i, int32_t j) {
    assert(i >= 0 && i < instance->num_customers + 1);
    assert(j >= 0 && j < instance->num_customers + 1);
    return vec2d_dist(&instance->positions[i], &instance->positions[j]);
}

static inline double profit(const Instance *instance, int32_t i) {
    assert(i >= 0 && i < instance->num_customers + 1);
    return instance->duals[i];
}

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

static inline size_t get_x_mip_var_idx_impl(const Instance *instance, int32_t i,
                                            int32_t j) {
    assert(i >= 0 && i < instance->num_customers + 1);
    assert(j >= 0 && j < instance->num_customers + 1);

    assert(i < j);

    size_t N = (size_t)instance->num_customers + 1;
    size_t d = ((size_t)(i + 1) * (size_t)(i + 2)) / 2;
    size_t result = i * N + j - d;
    return result;
}

static inline size_t get_x_mip_var_idx(const Instance *instance, int32_t i,
                                       int32_t j) {
    assert(i != j);
    return get_x_mip_var_idx_impl(instance, MIN(i, j), MAX(i, j));
}

static inline size_t get_y_mip_var_idx_offset(const Instance *instance) {
    return get_x_mip_var_idx(instance, instance->num_customers - 1,
                             instance->num_customers);
}

static inline size_t get_y_mip_var_idx(const Instance *instance, int32_t i) {

    assert(i >= 0 && i < instance->num_customers + 1);
    return (size_t)i + 1 + get_y_mip_var_idx_offset(instance);
}

static bool validate_mip_vars_packing(const Instance *instance) {
#ifndef NDEBUG
    size_t cnt = 0;
    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        for (int32_t j = i + 1; j < instance->num_customers + 1; j++) {
            assert(cnt == get_x_mip_var_idx(instance, i, j));
            cnt++;
        }
    }

    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        assert(cnt == get_y_mip_var_idx(instance, i));
        cnt++;
    }

#endif
    (void)instance;
    return true;
}

static bool add_degree_constraints(Solver *self, const Instance *instance) {
    bool result = true;
    CPXNNZ nnz = instance->num_customers + 1;

    CPXNNZ rmatbeg[] = {0};
    CPXDIM *index = NULL;
    double *value = NULL;
    char cname[128];
    const char *pcname[] = {(const char *)cname};

    double rhs[] = {0.0};
    char sense[] = {'E'};

    index = malloc(nnz * sizeof(*index));
    value = malloc(nnz * sizeof(*value));

    if (!index || !value) {
        log_fatal("%s :: Failed memory allocation", __func__);
        result = false;
        goto terminate;
    }

    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        snprintf(cname, ARRAY_LEN(cname), "deg(%d)", i);
        int32_t cnt = 0;

        for (int32_t j = 0; j < instance->num_customers + 1; j++) {
            if (i == j) {
                continue;
            }

            int32_t x_idx = get_x_mip_var_idx(instance, i, j);
            index[cnt] = x_idx;
            value[cnt] = +1.0;
            cnt++;
            // log_trace("%s :: x_idx = %d", __func__, x_idx);
        }

        assert(cnt == instance->num_customers);
        int32_t y_idx = get_y_mip_var_idx(instance, i);
        index[cnt] = y_idx;
        value[cnt] = -2.0;
        cnt++;

        assert(cnt == nnz);

        if (CPXXaddrows(self->data->env, self->data->lp, 0, 1, nnz, rhs, sense,
                        rmatbeg, index, value, NULL, pcname)) {
            log_fatal("%s :: CPXXaddrows failure", __func__);
            result = false;
            goto terminate;
        }
    }

terminate:
    free(index);
    free(value);

#if 0
    if (result) {
        show_lp_file(self);
    }
#endif

    return result;
}

static bool add_depot_is_part_of_tour_constraint(Solver *self,
                                                 const Instance *instance) {
    bool result = true;

    CPXDIM indices[] = {get_y_mip_var_idx(instance, 0)};
    char lu[] = {'L'};
    double bd[] = {1.0};
    if (CPXXchgbds(self->data->env, self->data->lp, 1, indices, lu, bd)) {
        log_fatal("%s :: Cannot change bounds for the depot", __func__);
        result = false;
    }

#if 0
    if (result) {
        show_lp_file(self);
    }
#endif

    return result;
}

static bool add_capacity_constraint(Solver *self, const Instance *instance) {
    bool result = true;
#if 1
    if (result) {
        show_lp_file(self);
    }
#endif
    return result;
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
        for (int32_t j = i + 1; j < instance->num_customers + 1; j++) {
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
        obj[0] = -1.0 * profit(instance, i);

        // NOTE: __EMAIL__ the professor
        //           Should we add this line of code which ensures and enforces
        //           that the profit acheived at the depot is 0.0

#if 0
        if (i == 0) {
            // We are the depot, make sure that the obj factor is 0.0
            obj[0] = 0.0;
        }
#endif

        if (CPXXnewcols(self->data->env, self->data->lp, 1, obj, lb, ub, xctype,
                        pcname)) {
            log_fatal("%s :: CPXXnewcols returned an error", __func__);
            return false;
        }
    }

    //
    // Now create the constraints (eg add the rows)
    //
    //

    validate_mip_vars_packing(instance);

    if (!add_degree_constraints(self, instance)) {
        log_fatal("%s :: add_degree_constraints failed", __func__);
        return false;
    }

    if (!add_depot_is_part_of_tour_constraint(self, instance)) {
        log_fatal("%s :: add_depot_is_part_of_tour_constraint failed",
                  __func__);
        return false;
    }
    if (!add_capacity_constraint(self, instance)) {
        log_fatal("%s :: add_capacity_constraint failed", __func__);
        return false;
    }

    return result;
}

static void add_gsec(Solver *self, const Instance *instance) {}

static inline int cplex_on_new_candidate(CPXCALLBACKCONTEXTptr context,
                                         Solver *solver,
                                         const Instance *intsance) {
    // NOTE:
    //      Called when cplex has a new feasible integral solution satisfying
    //      all constraints
    return 0;
}

static inline int cplex_on_new_relaxation(CPXCALLBACKCONTEXTptr context,
                                          Solver *solver,
                                          const Instance *intsance) {
    // NOTE:
    //      Called when cplex has a new feasible LP solution (not necessarily
    //      satisfying the integrality constraints)
    return 0;
}

static inline int cplex_on_global_progress(CPXCALLBACKCONTEXTptr context,
                                           Solver *solver,
                                           const Instance *intsance) {
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
    log_trace("Called %s", __func__);
    CplexCallbackData *data = (CplexCallbackData *)userhandle;

    int result = 0;

    switch (contextid) {
    case CPX_CALLBACKCONTEXT_CANDIDATE:
        result = cplex_on_new_candidate(context, data->solver, data->instance);
        break;
    case CPX_CALLBACKCONTEXT_RELAXATION:
        result = cplex_on_new_relaxation(context, data->solver, data->instance);
        break;
    case CPX_CALLBACKCONTEXT_GLOBAL_PROGRESS:
        result =
            cplex_on_global_progress(context, data->solver, data->instance);
        break;
    case CPX_CALLBACKCONTEXT_THREAD_UP:
        break;
    case CPX_CALLBACKCONTEXT_THREAD_DOWN:
        break;
    default:
        assert(!"Invalid case");
        break;
    }

    if (data->solver->should_terminate) {
        result = -1;
    }

    return result;
}

static bool on_solve_start(Solver *self, const Instance *instance) {
    CplexCallbackData data = {0};
    data.solver = self;
    data.instance = instance;

    CPXLONG contextmask =
        CPX_CALLBACKCONTEXT_CANDIDATE | CPX_CALLBACKCONTEXT_RELAXATION |
        CPX_CALLBACKCONTEXT_GLOBAL_PROGRESS | CPX_CALLBACKCONTEXT_THREAD_UP |
        CPX_CALLBACKCONTEXT_THREAD_DOWN;

    if (!CPXXcallbacksetfunc(self->data->env, self->data->lp, contextmask,
                             cplex_callback, (void *)&data)) {
        log_fatal(
            "%s :: Failed to setup generic callback (CPXXcallbacksetfunc)",
            __func__);
        goto fail;
    }

    return true;
fail:
    return false;
}

SolveStatus solve(Solver *self, const Instance *instance, Solution *solution) {
    if (!on_solve_start(self, instance)) {
        return SOLVE_STATUS_ERR;
    }
    // TODO: CPlex solve here

    // TODO: CPlex verify gap

    // TODO: CPlex ask lower bound and upper bound

    // TODO: CPlex convert mip variables into usable solution

    // TODO: Return solution

    todo();
    // TODO: Change the return value here
    return SOLVE_STATUS_ERR;
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

Solver mip_solver_create(const Instance *instance) {
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
