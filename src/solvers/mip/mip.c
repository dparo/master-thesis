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
#include <stdio.h>
#include <stdlib.h>
#include "core-utils.h"
#include "network.h"
#include "validation.h"
#include "cuts.h"

#ifndef COMPILED_WITH_CPLEX

Solver mip_solver_create(const Instance *instance, SolverTypedParams *tparams,
                         double timelimit, int32_t randomseed) {
    UNUSED_PARAM(instance);
    UNUSED_PARAM(timelimit);
    UNUSED_PARAM(randomseed);
    fprintf(stderr,
            "%s: Cannot use mip solver as the program was not compiled with "
            "CPLEX\n",
            __FILE__);
    fflush(stderr);
    abort();
    return (Solver){0};
}
#else

// NOTE:
//       cplexx is the 64 bit version of the API, while cplex (one x) is the 32
//       bit version of the API.
#include <ilcplex/cplexx.h>
#include <ilcplex/cpxconst.h>

#include <string.h>
#include "log.h"

ATTRIB_MAYBE_UNUSED static void show_lp_file(Solver *self) {
    (void)self;
#ifndef CONTINOUS_INTEGRATION_ENABLED
    CPXXwriteprob(self->data->env, self->data->lp, "TEST.lp", NULL);
    system("kitty -e nvim TEST.lp");
#endif
}

#define MAX_NUM_CORES 256

/// Struct that is used as a userhandle to be passed to the cplex generic
/// callback
typedef struct {
    Solver *solver;
    const Instance *instance;

    struct {
        double *vstar;
        Tour *tour;
    } threadctx[MAX_NUM_CORES];
} CplexCallbackCtx;

static inline int32_t *succ(Tour *tour, int32_t i) { return tsucc(tour, i); }

static inline int32_t *comp(Tour *tour, int32_t i) { return tcomp(tour, i); }

static inline double cost(const Instance *instance, int32_t i, int32_t j) {
    return cptp_dist(instance, i, j);
}

static inline double profit(const Instance *instance, int32_t i) {
    assert(i >= 0 && i < instance->num_customers + 1);
    return instance->duals[i];
}

static inline double demand(const Instance *instance, int32_t i) {
    assert(i >= 0 && i < instance->num_customers + 1);
    return instance->demands[i];
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
    return hm_nentries(instance->num_customers + 1);
}

static inline size_t get_y_mip_var_idx(const Instance *instance, int32_t i) {

    assert(i >= 0 && i < instance->num_customers + 1);
    return (size_t)i + get_y_mip_var_idx_offset(instance);
}

static void validate_mip_vars_packing(const Instance *instance) {
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

#else
    UNUSED_PARAM(instance);
#endif
}

static void unpack_mip_solution(const Instance *instance, Tour *t,
                                double *vstar) {
    int32_t n = t->num_customers + 1;

    for (int32_t start = 0; start < n; start++) {
        if (*comp(t, start) >= 0)
            continue; // node "start" was already visited, just skip it

        // a new component is found
        t->num_comps += 1;
        int32_t i = start;
        bool done = false;

        int32_t tour_length = 0;
        while (!done) {
            *comp(t, i) = t->num_comps - 1;
            done = true;
            tour_length += 1;
            for (int32_t j = 0; j < n; j++) {
                if (i == j) {
                    continue;
                }
                double v = vstar[get_x_mip_var_idx(instance, i, j)];
                assert(feq(v, 0.0, 1e-3) || feq(v, 1.0, 1e-3));
                bool j_was_already_visited = *comp(t, j) >= 0;
                if (v > 0.5 && !j_was_already_visited) {
                    *succ(t, i) = j;
                    i = j;
                    done = false;
                    break;
                }
            }
        }

        // Last edge to close the cycle
        if (i != start) {
            assert(tour_length > 1);
            *succ(t, i) = start;
        } else {
            // Avoid closing cycle for single node tours. They should be left
            // alone
            assert(tour_length == 1);
            t->num_comps -= 1;
            *comp(t, i) = INT32_DEAD_VAL;
        }
    }

#ifndef NDEBUG
    // Validate that the Y mip variable is consistent with what we find in the X
    // MIP var

    for (int32_t i = 0; i < n; i++) {
        double v = vstar[get_y_mip_var_idx(instance, i)];
        assert(feq(v, 0.0, 1e-3) || feq(v, 1.0, 1e-3));
        if (i == 0 || v >= 0.5) {
            assert(*comp(t, i) >= 0);
            assert(*succ(t, i) >= 0);
        } else {
            assert(*comp(t, i) < 0);
            assert(*succ(t, i) < 0);
        }
    }
#endif
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
        snprintf_safe(cname, ARRAY_LEN(cname), "deg(%d)", i);
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

    CPXNNZ rmatbeg[] = {0};
    CPXDIM *index = NULL;
    double *value = NULL;
    char cname[128];
    const char *pcname[] = {(const char *)cname};

    double rhs[] = {instance->vehicle_cap};
    char sense[] = {'L'};

    int32_t nnz = instance->num_customers + 1;

    index = malloc(nnz * sizeof(*index));
    value = malloc(nnz * sizeof(*value));

    assert(demand(instance, 0) == 0.0);

    for (int32_t i = 0; i < nnz; i++) {
        index[i] = get_y_mip_var_idx(instance, i);
        value[i] = demand(instance, i);
    }

    snprintf_safe(cname, ARRAY_LEN(cname), "capacity");

    if (CPXXaddrows(self->data->env, self->data->lp, 0, 1, nnz, rhs, sense,
                    rmatbeg, index, value, NULL, pcname)) {
        log_fatal("%s :: CPXXaddrows failure", __func__);
        result = false;
        goto terminate;
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

bool build_mip_formulation(Solver *self, const Instance *instance) {
    bool result = true;

    //
    // Create all the MIP variables that we need first (eg add the columns)
    //

    // Create the X MIP variable
    {
        double obj[1];
        double lb[1] = {0.0};
        double ub[1] = {1.0};
        char xctype[] = {'B'};

        char cname[128];
        const char *pcname[] = {(const char *)cname};

        int32_t cnt = 0;
        for (int32_t i = 0; i < instance->num_customers + 1; i++) {
            for (int32_t j = i + 1; j < instance->num_customers + 1; j++) {
                if (i == j)
                    continue;

                snprintf_safe(cname, sizeof(cname), "x(%d,%d)", i, j);
                obj[0] = cost(instance, i, j);

                if (CPXXnewcols(self->data->env, self->data->lp, 1, obj, lb, ub,
                                xctype, pcname)) {
                    log_fatal("%s :: CPXXnewcols returned an error", __func__);
                    return false;
                }
                cnt++;
            }
        }
        assert(cnt == hm_nentries(instance->num_customers + 1));
    }

    assert(hm_nentries(instance->num_customers + 1) ==
           CPXXgetnumcols(self->data->env, self->data->lp));

    // Create the Y MIP variable
    {

        double obj[1];
        double lb[1] = {0.0};
        double ub[1] = {1.0};
        char xctype[] = {'B'};

        char cname[128];
        const char *pcname[] = {(const char *)cname};

        for (int32_t i = 0; i < instance->num_customers + 1; i++) {
            snprintf_safe(cname, sizeof(cname), "y(%d)", i);
            obj[0] = -1.0 * profit(instance, i);

            if (CPXXnewcols(self->data->env, self->data->lp, 1, obj, lb, ub,
                            xctype, pcname)) {
                log_fatal("%s :: CPXXnewcols returned an error", __func__);
                return false;
            }
        }
    }

    //
    // Now create the constraints (eg add the rows)
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

static int cplex_on_new_relaxation(CPXCALLBACKCONTEXTptr context,
                                   Solver *solver, const Instance *instance,
                                   int32_t threadid, int32_t numthreads) {
    // NOTE:
    //      Called when cplex has a new feasible LP solution (not necessarily
    //      satisfying the integrality constraints)

    UNUSED_PARAM(threadid);
    UNUSED_PARAM(numthreads);

#if 0
    // DELETE_ME: Short circuit for now
    return 0;
#else

    double *vstar = malloc(sizeof(*vstar) * solver->data->num_mip_vars);
    if (!vstar) {
        log_fatal("%s :: Failed memory allocation", __func__);
        goto terminate;
    }

    double obj_p;
    if (CPXXcallbackgetrelaxationpoint(
            context, vstar, 0, solver->data->num_mip_vars - 1, &obj_p)) {
        log_fatal("%s :: Failed `CPXXcallbackgetrelaxationpoint`", __func__);
        goto terminate;
    }

    FlowNetwork network = {0};
    network.nnodes = instance->num_customers + 1;

    network.source_vertex = 0;

    do {
        network.sink_vertex = rand() % (instance->num_customers + 1);
    } while (network.sink_vertex == 0 &&
             network.sink_vertex == network.source_vertex);

    network.flow =
        malloc((instance->num_customers + 1) * (instance->num_customers + 1) *
               sizeof(*network.flow));

    network.cap = malloc((instance->num_customers + 1) *
                         (instance->num_customers + 1) * sizeof(*network.cap));

    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        for (int32_t j = 0; j < instance->num_customers + 1; j++) {
            double cap =
                i == j ? 0.0 : vstar[get_x_mip_var_idx(instance, i, j)];
            assert(fgte(cap, 0.0, 1e-8));
            // NOTE: Fix floating point rounding errors. In fact cap may be
            // slightly negative...
            cap = MAX(0.0, cap);
            if (feq(cap, 0.0, 1e-6)) {
                cap = 0.0;
            }
            assert(cap >= 0.0);
            *network_cap(&network, i, j) = cap;
        }
    }

    MaxFlowResult max_flow_result =
        max_flow_result_create(instance->num_customers + 1);
    double max_flow = push_relabel_max_flow(&network, &max_flow_result);

    // printf("--- max_flow = %f\n", max_flow);
    const double EPS = 1e-4;
    for (int32_t h = 1; h < instance->num_customers + 1; h++) {
        double y = vstar[get_y_mip_var_idx(instance, h)];
        int32_t bp_h = max_flow_result.bipartition.data[h];
        if (bp_h == 0 && flt(max_flow, 2.0 * y, EPS)) {
            // Separate the cut
            double rhs = 0;
            char sense = 'G';
            CPXNNZ nnz = 0;
            CPXDIM *index = NULL;
            double *value = NULL;
            CPXNNZ rmatbeg = 0;
            int purgeable = CPX_USECUT_PURGE;
            int local_validity = 0;

            for (int32_t i = 1; i < instance->num_customers + 1; i++) {
                for (int32_t j = 0; j < instance->num_customers + 1; j++) {
                    int32_t bp_i = max_flow_result.bipartition.data[i];
                    int32_t bp_j = max_flow_result.bipartition.data[j];
                    if (bp_i == 0 && bp_j == 1) {
                        nnz++;
                    }
                }
            }

            nnz += 1;
            index = malloc(nnz * sizeof(*index));
            value = malloc(nnz * sizeof(*value));

            int32_t pos = 0;
            for (int32_t i = 1; i < instance->num_customers + 1; i++) {
                for (int32_t j = 0; j < instance->num_customers + 1; j++) {
                    int32_t bp_i = max_flow_result.bipartition.data[i];
                    int32_t bp_j = max_flow_result.bipartition.data[j];
                    if (bp_i == 0 && bp_j == 1) {
                        index[pos] = get_x_mip_var_idx(instance, i, j);
                        value[pos] = 1.0;
                        ++pos;
                    }
                }
            }

            index[pos] = get_y_mip_var_idx(instance, h);
            value[pos] = -2.0;

            // NOTE::
            //      https://www.ibm.com/docs/en/icos/12.9.0?topic=c-cpxxcallbackaddusercuts-cpxcallbackaddusercuts
            //  You can call this routine more than once in the same
            //  callback invocation. CPLEX will accumulate the cuts from all
            //  such calls.
            if (0 != CPXXcallbackaddusercuts(context, 1, nnz, &rhs, &sense,
                                             &rmatbeg, index, value, &purgeable,
                                             &local_validity)) {
                log_fatal(
                    "%s :: CPXXcallbackaddusercuts failed to add user cut",
                    __func__);
            }

            free(index);
            free(value);
        }
    }

    free(vstar);
    flow_network_destroy(&network);
    max_flow_result_destroy(&max_flow_result);
    return 0;

terminate:
    log_fatal("%s :: Fatal termination error", __func__);

    free(vstar);
    flow_network_destroy(&network);
    max_flow_result_destroy(&max_flow_result);
    return 1;
#endif
}

static bool reject_candidate_point(Tour *tour, CPXCALLBACKCONTEXTptr context,
                                   Solver *solver, const Instance *instance,
                                   int32_t threadid, int32_t numthreads) {
    UNUSED_PARAM(threadid);
    UNUSED_PARAM(numthreads);

    assert(tour->num_comps != 1);

    int32_t *cnnodes = calloc(tour->num_comps, sizeof(*cnnodes));

    CPXDIM *index = NULL;
    double *value = NULL;

    if (cnnodes == NULL) {
        log_fatal("%s :: Failed memory allocation", __func__);
        goto failure;
    }

    int32_t n = instance->num_customers + 1;

    // Count the number of nodes in each component
    for (int32_t i = 0; i < n; i++) {
        assert(*comp(tour, i) < tour->num_comps);
        if (*comp(tour, i) >= 0) {
            ++cnnodes[*comp(tour, i)];
        }
    }

    // Small sanity check
#ifndef NDEBUG
    {
        for (int32_t i = 0; i < tour->num_comps; i++) {
            assert(cnnodes[i] >= 2);
        }
    }
#endif

    double rhs = 0;
    char sense = 'G';
    CPXNNZ rmatbeg[] = {0};

    int32_t nnz_upper_bound = 1 + (n * n) / 4;
    index = malloc(sizeof(*index) * nnz_upper_bound);
    value = malloc(sizeof(*value) * nnz_upper_bound);

    if (!index || !value) {
        log_fatal("%s :: Failed memory allocation", __func__);
        goto failure;
    }

    assert(*comp(tour, 0) == 0);

    // Note start from c = 1. Subtour Elimination Constraints that include the
    // depot are NOT valid.
    for (int32_t c = 1; c < tour->num_comps; c++) {
        assert(cnnodes[c] >= 2);

        CPXNNZ nnz = 1 + cnnodes[c] * (n - cnnodes[c]);
        CPXNNZ cnt = 0;

        assert(nnz <= nnz_upper_bound);

        // Again, skip the depot (start from i = 1)
        for (int32_t i = 1; i < n; i++) {
            if (*comp(tour, i) == c) {
                for (int32_t j = 0; j < n; j++) {
                    if (i == j) {
                        continue;
                    }

                    // If node i belongs to S and node j does NOT belong to S
                    if (*comp(tour, j) != c) {
                        index[cnt] = (CPXDIM)get_x_mip_var_idx(instance, i, j);
                        value[cnt] = +1.0;
                        cnt++;
                    }
                }
            }
        }

        assert(cnt < nnz_upper_bound);
        assert(cnt == nnz - 1);

#ifndef NDEBUG
        // Assert that index array does not contain duplicates
        for (int32_t i = 0; i < nnz - 1; i++) {
            assert(index[i] >= 0);
            for (int32_t j = 0; j < nnz - 1; j++) {
                assert(index[j] >= 0);
                if (i != j) {
                    assert(index[i] != index[j]);
                }
            }
        }
#endif

        int32_t added_cuts = 0;
        for (int32_t i = 0; i < n; i++) {
            if (*comp(tour, i) == c) {
                assert(*comp(tour, i) >= 1);

                index[nnz - 1] = (CPXDIM)get_y_mip_var_idx(instance, i);
                value[nnz - 1] = -2.0;

                log_trace(
                    "%s :: Adding GSEC constraint for component %d vertex %d, "
                    "(num_of_nodes_in_each_comp[%d] = %d, nnz = %lld)",
                    __func__, c, i, c, cnnodes[c], nnz);

                // NOTE::
                //      https://www.ibm.com/docs/en/icos/12.10.0?topic=c-cpxxcallbackrejectcandidate-cpxcallbackrejectcandidate
                //  You can call this routine more than once in the same
                //  callback invocation. CPLEX will accumulate the constraints
                //  from all such calls.
                if (CPXXcallbackrejectcandidate(context, 1, nnz, &rhs, &sense,
                                                rmatbeg, index, value) != 0) {
                    log_fatal("%s :: Failed CPXXcallbackrejectcandidate",
                              __func__);
                    goto failure;
                }
                added_cuts += 1;
            }
        }
        assert(added_cuts == cnnodes[c]);
    }

    free(cnnodes);
    free(index);
    free(value);
    return true;

failure:
    free(cnnodes);
    free(index);
    free(value);
    return false;
}

static int cplex_on_new_candidate_point(CPXCALLBACKCONTEXTptr context,
                                        Solver *solver,
                                        const Instance *instance,
                                        int32_t threadid, int32_t numthreads) {
    // NOTE:
    //      Called when cplex has a new feasible integral solution
    //      satisfying all constraints

    // NOTE:
    //   Subtour Elimination Constraints (SECs) separation based on
    //   the connected components

    // NOTE: In alternative see function CCcut_connect_component of Concorde to
    // use a more efficient function

    double *vstar = malloc(sizeof(*vstar) * solver->data->num_mip_vars);
    if (!vstar) {
        log_fatal("%s :: Failed memory allocation", __func__);
        goto terminate;
    }

    Tour tour = tour_create(instance);
    if (!tour.comp || !tour.succ) {
        goto terminate;
    }

    double obj_p;
    if (CPXXcallbackgetcandidatepoint(context, vstar, 0,
                                      solver->data->num_mip_vars - 1, &obj_p)) {
        log_fatal("%s :: Failed `CPXcallbackgetcandidatepoint`", __func__);
        goto terminate;
    }

    unpack_mip_solution(instance, &tour, vstar);

    if (tour.num_comps >= 2) {
        log_info("%s :: num_comps of unpacked tour is %d -- rejecting "
                 "candidate point...",
                 __func__, tour.num_comps);
        reject_candidate_point(&tour, context, solver, instance, threadid,
                               numthreads);
    } else {
        log_info("%s :: num_comps of unpacked tour is %d -- accepting "
                 "candidate point...",
                 __func__, tour.num_comps);
    }

    free(vstar);
    tour_destroy(&tour);
    return 0;

terminate:
    log_fatal("%s :: Fatal termination error", __func__);
    free(vstar);
    tour_destroy(&tour);
    return 1;
}

static int cplex_on_global_progress(CPXCALLBACKCONTEXTptr context,
                                    Solver *solver, const Instance *instance) {
    UNUSED_PARAM(solver);
    UNUSED_PARAM(instance);

    // NOTE: Global progress is inherently thread safe
    //            See:
    //            https://www.ibm.com/docs/en/cofz/12.10.0?topic=callbacks-multithreading-generic
    double upper_bound = INFINITY, lower_bound = -INFINITY;

    CPXLONG num_processed_nodes = 0, simplex_iterations = 0;
    CPXXcallbackgetinfodbl(context, CPXCALLBACKINFO_BEST_BND, &lower_bound);
    CPXXcallbackgetinfodbl(context, CPXCALLBACKINFO_BEST_SOL, &upper_bound);
    CPXXcallbackgetinfolong(context, CPXCALLBACKINFO_NODECOUNT,
                            &num_processed_nodes);
    CPXXcallbackgetinfolong(context, CPXCALLBACKINFO_ITCOUNT,
                            &simplex_iterations);

    double incumbent = INFINITY;
    CPXXcallbackgetincumbent(context, NULL, 0, 0, &incumbent);

    if (lower_bound <= -CPX_INFBOUND) {
        lower_bound = -INFINITY;
    }
    if (upper_bound >= CPX_INFBOUND) {
        upper_bound = INFINITY;
    }
    if (incumbent >= CPX_INFBOUND) {
        incumbent = INFINITY;
    }

    log_info("%s :: num_processed_nodes = %lld, simplex_iterations = %lld, "
             "lower_bound = %f, upper_bound = %f, incumbent = %f\n",
             __func__, num_processed_nodes, simplex_iterations, lower_bound,
             incumbent, upper_bound);
    return 0;
}

static int cplex_on_thread_activation(int activation,
                                      CPXCALLBACKCONTEXTptr context,
                                      Solver *solver, const Instance *instance,
                                      CPXLONG threadid, CPXLONG numthreads) {
    UNUSED_PARAM(context);
    UNUSED_PARAM(solver);
    UNUSED_PARAM(instance);
    assert(activation == -1 || activation == 1);

    if (activation > 0) {
        log_info("cplex_callback activated a thread :: threadid = "
                 "%lld, numthreads = %lld",
                 threadid, numthreads);
    } else if (activation < 0) {
        log_info("cplex_callback deactivated an old thread :: threadid = "
                 "%lld, numthreads = %lld",
                 threadid, numthreads);
    } else {
        assert(!"Invalid code path");
    }
    return 0;
}

CPXPUBLIC static int cplex_callback(CPXCALLBACKCONTEXTptr context,
                                    CPXLONG contextid, void *userhandle) {
    CplexCallbackCtx *data = (CplexCallbackCtx *)userhandle;

    int result = 0;

    int32_t numthreads;
    int32_t threadid;
    CPXXcallbackgetinfoint(context, CPXCALLBACKINFO_THREADS, &numthreads);
    CPXXcallbackgetinfoint(context, CPXCALLBACKINFO_THREADID, &threadid);
    assert(threadid >= 0 && threadid < numthreads);

    log_debug("%s :: numthreads = %d, numcores = %d", __func__, numthreads,
              data->solver->data->numcores);
    assert(numthreads <= data->solver->data->numcores);

    // NOTE:
    //      Look at
    //      https://www.ibm.com/docs/en/cofz/12.10.0?topic=callbacks-multithreading-generic
    //      https://www.ibm.com/docs/en/icos/12.8.0.0?topic=c-cpxxcallbacksetfunc-cpxcallbacksetfunc
    switch (contextid) {
    case CPX_CALLBACKCONTEXT_CANDIDATE: {
        int is_point = false;
        if (CPXXcallbackcandidateispoint(context, &is_point) != 0) {
            result = 1;
        }

        if (is_point && result == 0) {
            result = cplex_on_new_candidate_point(
                context, data->solver, data->instance, threadid, numthreads);
        }
        break;
    }
    case CPX_CALLBACKCONTEXT_RELAXATION:
        result = cplex_on_new_relaxation(context, data->solver, data->instance,
                                         threadid, numthreads);
        break;
    case CPX_CALLBACKCONTEXT_GLOBAL_PROGRESS:
        // NOTE: Global progress is inherently thread safe
        //            See:
        //            https://www.ibm.com/docs/en/cofz/12.10.0?topic=callbacks-multithreading-generic
        result =
            cplex_on_global_progress(context, data->solver, data->instance);
        break;
    case CPX_CALLBACKCONTEXT_THREAD_UP:
    case CPX_CALLBACKCONTEXT_THREAD_DOWN: {
        int activation = contextid == CPX_CALLBACKCONTEXT_THREAD_UP ? 1 : -1;
        result =
            cplex_on_thread_activation(activation, context, data->solver,
                                       data->instance, threadid, numthreads);
        break;
    }

    default:
        assert(!"Invalid case");
        break;
    }

    if (data->solver->should_terminate) {
        CPXXcallbackabort(context);
    }

    // NOTE:
    // from
    // https://www.ibm.com/docs/en/cofz/12.10.0?topic=manual-cpxcallbackfunc
    //  The routine returns 0 (zero) if successful and nonzero if an error
    //  occurs. Any value different from zero will result in an ungraceful exit
    //  of CPLEX (usually with CPXERR_CALLBACK). Note that the actual value
    //  returned is not propagated up the call stack. The only thing that CPLEX
    //  checks is whether the returned value is zero or not.
    // Do not use a non-zero return value to stop optimization in case there is
    // no error. Use CPXXcallbackabort and CPXcallbackabort for that purpose.

    return result;
}

static bool on_solve_start(Solver *self, const Instance *instance,
                           CplexCallbackCtx *callback_ctx) {
    UNUSED_PARAM(instance);

    CPXLONG contextmask =
        CPX_CALLBACKCONTEXT_CANDIDATE | CPX_CALLBACKCONTEXT_RELAXATION |
        CPX_CALLBACKCONTEXT_GLOBAL_PROGRESS | CPX_CALLBACKCONTEXT_THREAD_UP |
        CPX_CALLBACKCONTEXT_THREAD_DOWN;

    if (CPXXcallbacksetfunc(self->data->env, self->data->lp, contextmask,
                            cplex_callback, (void *)callback_ctx) != 0) {
        log_fatal(
            "%s :: Failed to setup generic callback (CPXXcallbacksetfunc)",
            __func__);
        goto fail;
    }

    return true;
fail:
    return false;
}

static bool process_cplex_output(Solver *self, Solution *solution, int lpstat) {
#define CHECKED(FUNC, ...)                                                     \
    do {                                                                       \
        if (0 != __VA_ARGS__) {                                                \
            log_fatal("%s ::" #FUNC " failed", __func__);                      \
            return false;                                                      \
        }                                                                      \
    } while (0)

    double gap;
    CPXDIM num_user_cuts;

    CHECKED(CPXXgetbestobjval,
            CPXXgetbestobjval(self->data->env, self->data->lp,
                              &solution->lower_bound));

    CHECKED(CPXXgetobjval, CPXXgetobjval(self->data->env, self->data->lp,
                                         &solution->upper_bound));

    CHECKED(CPXXgetmiprelgap,
            CPXXgetmiprelgap(self->data->env, self->data->lp, &gap));

    CHECKED(CPXXgetnumcuts, CPXXgetnumcuts(self->data->env, self->data->lp,
                                           CPX_CUT_USER, &num_user_cuts));

    CPXCNT simplex_iterations =
        CPXXgetmipitcnt(self->data->env, self->data->lp);

    CPXCNT nodecnt = CPXXgetnodecnt(self->data->env, self->data->lp);

    assert(feq(gap, solution_relgap(solution), 1e-6));

    log_info("Cplex solution finished (lpstat = %d) with :: cost = [%f, %f], "
             "gap = %f, simplex_iterations = %lld, nodecnt = %lld, user_cuts = "
             "%d",
             lpstat, solution->lower_bound, solution->upper_bound, gap,
             simplex_iterations, nodecnt, num_user_cuts);

#undef CHECKED
    return true;
}

SolveStatus solve(Solver *self, const Instance *instance, Solution *solution,
                  int64_t begin_time) {
    self->data->begin_time = begin_time;

    SolveStatus result = SOLVE_STATUS_ERR;

    CplexCallbackCtx callback_ctx = {0};
    callback_ctx.solver = self;
    callback_ctx.instance = instance;

    if (!on_solve_start(self, instance, &callback_ctx)) {
        return SOLVE_STATUS_ERR;
    }

    if (CPXXmipopt(self->data->env, self->data->lp) != 0) {
        log_fatal("%s :: CPXmipopt() error", __func__);
        return SOLVE_STATUS_ERR;
    }

    assert(CPXXgetmethod(self->data->env, self->data->lp) == CPX_ALG_MIP);

    int lpstat = 0;
    double *vstar = malloc(sizeof(*vstar) * self->data->num_mip_vars);

    if (CPXXsolution(self->data->env, self->data->lp, &lpstat,
                     &solution->upper_bound, vstar, NULL, NULL, NULL) != 0) {
        log_fatal("%s :: CPXXsolution failed [lpstat = %d]", __func__, lpstat);
        goto terminate;
    }

    if (!process_cplex_output(self, solution, lpstat)) {
        log_fatal("%s :: process_cplex_output failed", __func__);
        goto terminate;
    }

    // https://www.ibm.com/docs/en/icos/12.10.0?topic=g-cpxxgetstat-cpxgetstat
    // https://www.ibm.com/docs/en/icos/12.10.0?topic=micclcarm-solution-status-symbols-in-cplex-callable-library-c-api
    // https://www.ibm.com/docs/en/icos/12.10.0?topic=micclcarm-solution-status-symbols-specific-mip-in-cplex-callable-library-c-api

    switch (lpstat) {
    case CPXMIP_OPTIMAL:
    case CPXMIP_OPTIMAL_TOL:
        result = SOLVE_STATUS_OPTIMAL;
        break;

    case CPX_STAT_FEASIBLE:
    case CPXMIP_TIME_LIM_FEAS:
    case CPXMIP_NODE_LIM_FEAS:
    case CPXMIP_ABORT_FEAS:
        result = SOLVE_STATUS_FEASIBLE;
        break;

    case CPX_STAT_INFEASIBLE:
    case CPXMIP_INFEASIBLE:
    case CPXMIP_TIME_LIM_INFEAS:
    case CPXMIP_NODE_LIM_INFEAS:
    case CPXMIP_ABORT_INFEAS:
        result = SOLVE_STATUS_INFEASIBLE;
        break;

    case CPXERR_CALLBACK:
        result = SOLVE_STATUS_ERR;
        break;

    case CPX_STAT_UNBOUNDED:
    case CPXMIP_UNBOUNDED:
        log_fatal("%s :: CPXXsolution lpstat -- Solution to problem has "
                  "an unbounded ray",
                  __func__);
        break;
    case CPX_STAT_NUM_BEST:
        // Could not converge due to number difficulties
        result = SOLVE_STATUS_ERR;
        break;

    default:
        assert(!"Invalid code path");
        result = SOLVE_STATUS_ERR;
        break;
    }

    bool cplex_found_a_solution =
        result == SOLVE_STATUS_FEASIBLE || result == SOLVE_STATUS_OPTIMAL;

    if (cplex_found_a_solution) {

        for (int32_t i = 0; i < instance->num_customers + 1; i++) {
            for (int32_t j = i + 1; j < instance->num_customers + 1; j++) {
                double v = vstar[get_x_mip_var_idx(instance, i, j)];
                if (feq(v, 1.0, 1e-3)) {
                    printf("x(%d, %d) = %g\n", i, j,
                           vstar[get_x_mip_var_idx(instance, i, j)]);
                }
            }
        }
        printf("\n");

        for (int32_t i = 0; i < instance->num_customers + 1; i++) {
            double v = vstar[get_y_mip_var_idx(instance, i)];
            if (feq(v, 1.0, 1e-3)) {
                printf("y(%d) = %g\n", i, v);
            }
        }

        unpack_mip_solution(instance, &solution->tour, vstar);
    }

terminate:
    free(vstar);
    return result;
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

    // Clamp the number of available cores to MAX_NUM_CORES
    {
        if (CPXXgetnumcores(solver->data->env, &solver->data->numcores) != 0) {
            log_fatal("CPXXgetnumcores failed");
            goto fail;
        }

        log_info("%s :: CPXXgetnumcores returned numcores = %d", __func__,
                 solver->data->numcores);

        int32_t num_threads = MIN(MAX_NUM_CORES, solver->data->numcores);
        log_info("%s :: Setting the maximum number of threads that CPLEX can "
                 "use to %d",
                 __func__, num_threads);
        if (CPXXsetintparam(solver->data->env, CPX_PARAM_THREADS,
                            num_threads) != 0) {
            log_fatal("%s :: CPXXsetintparam for CPX_PARAM_THREADS failed",
                      __func__);
            goto fail;
        }
    }

    return true;

fail:
    solver->destroy(solver);
    return false;
}

Solver mip_solver_create(const Instance *instance, SolverTypedParams *tparams,
                         double timelimit, int32_t randomseed) {
    UNUSED_PARAM(tparams);
    log_trace("%s", __func__);

    Solver solver = {0};
    solver.solve = solve;
    solver.destroy = mip_solver_destroy;
    solver.data = calloc(1, sizeof(*solver.data));
    if (!solver.data) {
        goto fail;
    }

    if (!cplex_setup(&solver, instance)) {
        log_fatal("%s : Failed to initialize cplex", __func__);
        goto fail;
    }

    if (!build_mip_formulation(&solver, instance)) {
        log_fatal("%s : Failed to build mip formulation", __func__);
        goto fail;
    }

    solver.data->num_mip_vars =
        CPXXgetnumcols(solver.data->env, solver.data->lp);
    solver.data->num_mip_constraints =
        CPXXgetnumrows(solver.data->env, solver.data->lp);

    log_info("%s :: CPXXsetdblparam -- Setting TIMELIMIT to %f", __func__,
             timelimit);
    if (CPXXsetdblparam(solver.data->env, CPX_PARAM_TILIM, timelimit) != 0) {
        log_fatal("%s :: CPXXsetdbparam -- Failed to setup CPX_PARAM_TILIM "
                  "(timelimit) to value %f",
                  __func__, timelimit);
        goto fail;
    }

    log_info("%s :: CPXXsetintparam -- Setting SEED to %d", __func__,
             randomseed);
    if (CPXXsetintparam(solver.data->env, CPX_PARAM_RANDOMSEED, randomseed) !=
        0) {
        log_fatal("%s :: CPXXsetintparam -- Faield to setup "
                  "CPX_PARAM_RANDOMSEED (randomseed) to value %d",
                  __func__, randomseed);
    }

    return solver;
fail:
    if (solver.destroy)
        solver.destroy(&solver);
    return (Solver){0};
}

#endif
