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

#define NUM_CUTS (ARRAY_LEN(CUT_DESCRS))
static const CutDescriptor *CUT_DESCRS[] = {
    &CUT_GSEC_DESCRIPTOR,
    // &CUT_GLM_DESCRIPTOR,
};

typedef struct {
    double *vstar;
    FlowNetwork network;
    Tour tour;
    CutSeparationFunctor functors[NUM_CUTS];
} CallbackThreadLocalData;

/// Struct that is used as a userhandle to be passed to the cplex generic
/// callback
typedef struct {
    Solver *solver;
    const Instance *instance;
    CallbackThreadLocalData thread_local_data[MAX_NUM_CORES];
} CplexCallbackCtx;

static bool
create_callback_thread_local_data(CallbackThreadLocalData *thread_local_data,
                                  CPXCALLBACKCONTEXTptr cplex_cb_ctx,
                                  const Instance *instance, Solver *solver) {
    memset(thread_local_data, 0, sizeof(*thread_local_data));

    thread_local_data->network =
        flow_network_create(instance->num_customers + 1);
    thread_local_data->tour = tour_create(instance);

    thread_local_data->vstar =
        malloc(sizeof(*thread_local_data->vstar) * solver->data->num_mip_vars);

    bool success = true;

    for (int32_t i = 0; i < (int32_t)NUM_CUTS; i++) {
        const CutSeparationIface *iface = CUT_DESCRS[i]->iface;
        CutSeparationFunctor *functor = &thread_local_data->functors[i];

        functor->ctx = iface->activate(instance, solver);
        functor->internal.cplex_cb_ctx = cplex_cb_ctx;
        functor->instance = instance;
        functor->solver = solver;

        success &= functor->ctx && thread_local_data->vstar &&
                   thread_local_data->network.flow &&
                   thread_local_data->network.cap &&
                   tour_is_valid(&thread_local_data->tour);
    }

    return success;
}

static void
destroy_callback_thread_local_data(CallbackThreadLocalData *thread_local_data) {

    for (int32_t i = 0; i < (int32_t)NUM_CUTS; i++) {
        if (thread_local_data->functors[i].ctx) {
            CUT_GSEC_IFACE.deactivate(thread_local_data->functors[i].ctx);
            memset(&thread_local_data->functors[i], 0,
                   sizeof(thread_local_data->functors[i]));
        }
    }
    free(thread_local_data->vstar);
    tour_destroy(&thread_local_data->tour);
    flow_network_destroy(&thread_local_data->network);
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

    tour_clear(t);
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

static int cplex_on_new_relaxation(CPXCALLBACKCONTEXTptr cplex_cb_ctx,
                                   CplexCallbackCtx *ctx, int32_t threadid,
                                   int32_t numthreads) {
    // NOTE:
    //      Called when cplex has a new feasible LP solution (not necessarily
    //      satisfying the integrality constraints)

    assert(threadid < numthreads);
    assert(threadid <= MAX_NUM_CORES);

    Solver *solver = ctx->solver;
    const Instance *instance = ctx->instance;
    CallbackThreadLocalData *tld = &ctx->thread_local_data[threadid];
    double *vstar = tld->vstar;

    if (!vstar) {
        log_fatal("%s :: Failed memory allocation", __func__);
        goto terminate;
    }

    double obj_p;
    if (CPXXcallbackgetrelaxationpoint(
            cplex_cb_ctx, vstar, 0, solver->data->num_mip_vars - 1, &obj_p)) {
        log_fatal("%s :: Failed `CPXXcallbackgetrelaxationpoint`", __func__);
        goto terminate;
    }

    FlowNetwork *net = &tld->network;
    const int32_t n = instance->num_customers + 1;

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
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
            *network_cap(net, i, j) = cap;
        }
    }

    for (int32_t i = 0; i < (int32_t)NUM_CUTS; i++) {
        CutSeparationFunctor *functor = &tld->functors[i];
#if 0
        printf("  threadid = %d, cplex_cb_ctx = %p\n", threadid, cplex_cb_ctx);
        printf("  threadid = %d, tld = %p\n", threadid, tld);
        printf("  threadid = %d, vstar = %p\n", threadid, vstar);
        printf("  threadid = %d, tour = %p\n", threadid, tour);
        printf("  threadid = %d, functor.ctx = %p\n", threadid, functor->ctx);
        printf("  threadid = %d, functor.internal.cplex_cb_ctx = %p\n",
               threadid, functor->internal.cplex_cb_ctx);
        printf("\n");
#endif

        const CutSeparationIface *iface = &CUT_GSEC_IFACE;
        if (iface->fractional_sep) {
            const int64_t begin_time = os_get_usecs();
            // NOTE: We need to reset the cplex_cb_ctx since it might change
            // during the execution. The same threadid id, is not guaranteed to
            // have the same cplex_cb_ctx for the entire duration of the thread
            functor->internal.cplex_cb_ctx = cplex_cb_ctx;
            bool separation_success =
                iface->fractional_sep(functor, obj_p, vstar, net);
            functor->internal.fractional_stats.accum_usecs +=
                os_get_usecs() - begin_time;

            if (!separation_success) {
                log_fatal("Separation of integral `%s` cuts failed", "GSEC");
                goto terminate;
            }
        }
    }

    return 0;
terminate:
    log_fatal("%s :: Fatal termination error", __func__);
    return 1;
}

static int cplex_on_new_candidate_point(CPXCALLBACKCONTEXTptr cplex_cb_ctx,
                                        CplexCallbackCtx *ctx, int32_t threadid,
                                        int32_t numthreads) {
    // NOTE:
    //      Called when cplex has a new feasible integral solution
    //      satisfying all constraints

    assert(threadid < numthreads);
    assert(threadid <= MAX_NUM_CORES);

    Solver *solver = ctx->solver;
    const Instance *instance = ctx->instance;
    CallbackThreadLocalData *tld = &ctx->thread_local_data[threadid];
    double *vstar = tld->vstar;
    Tour *tour = &tld->tour;

    if (!vstar) {
        log_fatal("%s :: Failed memory allocation", __func__);
        goto terminate;
    }

    if (!tour_is_valid(tour)) {
        goto terminate;
    }

    double obj_p;
    if (CPXXcallbackgetcandidatepoint(cplex_cb_ctx, vstar, 0,
                                      solver->data->num_mip_vars - 1, &obj_p)) {
        log_fatal("%s :: Failed `CPXcallbackgetcandidatepoint`", __func__);
        goto terminate;
    }

    unpack_mip_solution(instance, tour, vstar);

    if (tour->num_comps >= 2) {
        log_trace("%s :: num_comps of unpacked tour is %d -- rejecting "
                  "candidate point...",
                  __func__, tour->num_comps);

        for (int32_t i = 0; i < (int32_t)NUM_CUTS; i++) {
            CutSeparationFunctor *functor = &tld->functors[i];

            const CutSeparationIface *iface = &CUT_GSEC_IFACE;
            if (iface->fractional_sep) {
                const int64_t begin_time = os_get_usecs();
                // NOTE: We need to reset the cplex_cb_ctx since it might change
                // during the execution. The same threadid id, is not guaranteed
                // to have the same cplex_cb_ctx for the entire duration of the
                // thread
                functor->internal.cplex_cb_ctx = cplex_cb_ctx;
                bool separation_success =
                    iface->integral_sep(functor, obj_p, vstar, tour);
                functor->internal.integral_stats.accum_usecs +=
                    os_get_usecs() - begin_time;

                if (!separation_success) {
                    log_trace("Separation of integral `%s` cuts failed",
                              "GSEC");
                    goto terminate;
                }
            }
        }

    } else {
        log_trace("%s :: num_comps of unpacked tour is %d -- accepting "
                  "candidate point...",
                  __func__, tour->num_comps);
    }

    return 0;

terminate:
    log_fatal("%s :: Fatal termination error", __func__);
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
             "lower_bound = %.12f, upper_bound = %f, incumbent = %.12f\n",
             __func__, num_processed_nodes, simplex_iterations, lower_bound,
             incumbent, upper_bound);
    return 0;
}

static int cplex_on_thread_activation(int activation,
                                      CPXCALLBACKCONTEXTptr cplex_cb_ctx,
                                      CplexCallbackCtx *ctx, CPXLONG threadid,
                                      CPXLONG numthreads) {
    assert(activation == -1 || activation == 1);
    assert(numthreads <= MAX_NUM_CORES);

    CallbackThreadLocalData *thread_local_data =
        &ctx->thread_local_data[threadid];

    if (activation > 0) {
        log_trace("cplex_callback activated a thread :: threadid = "
                  "%lld, numthreads = %lld",
                  threadid, numthreads);

        if (!create_callback_thread_local_data(thread_local_data, cplex_cb_ctx,
                                               ctx->instance, ctx->solver)) {
            destroy_callback_thread_local_data(thread_local_data);
            log_fatal("%s :: Failed create_callback_thread_local_data()",
                      __func__);
            return 1;
        }
    } else if (activation < 0) {
        log_trace("cplex_callback deactivated an old thread :: threadid = "
                  "%lld, numthreads = %lld\n",
                  threadid, numthreads);
        destroy_callback_thread_local_data(thread_local_data);
    } else {
        assert(!"Invalid code path");
    }
    return 0;
}

CPXPUBLIC static int cplex_callback(CPXCALLBACKCONTEXTptr cplex_cb_ctx,
                                    CPXLONG contextid, void *userhandle) {
    CplexCallbackCtx *ctx = (CplexCallbackCtx *)userhandle;
    int result = 0;

    int32_t numthreads;
    int32_t threadid;
    CPXXcallbackgetinfoint(cplex_cb_ctx, CPXCALLBACKINFO_THREADS, &numthreads);
    CPXXcallbackgetinfoint(cplex_cb_ctx, CPXCALLBACKINFO_THREADID, &threadid);
    assert(threadid >= 0 && threadid < numthreads);

    log_debug("%s :: numthreads = %d, numcores = %d", __func__, numthreads,
              ctx->solver->data->numcores);
    assert(numthreads <= ctx->solver->data->numcores);

    // NOTE:
    //      Look at
    //      https://www.ibm.com/docs/en/cofz/12.10.0?topic=callbacks-multithreading-generic
    //      https://www.ibm.com/docs/en/icos/12.8.0.0?topic=c-cpxxcallbacksetfunc-cpxcallbacksetfunc
    switch (contextid) {
    case CPX_CALLBACKCONTEXT_CANDIDATE: {
        int is_point = false;
        if (CPXXcallbackcandidateispoint(cplex_cb_ctx, &is_point) != 0) {
            result = 1;
        }

        if (is_point && result == 0) {
            result = cplex_on_new_candidate_point(cplex_cb_ctx, ctx, threadid,
                                                  numthreads);
        }
        break;
    }
    case CPX_CALLBACKCONTEXT_RELAXATION:
        result =
            cplex_on_new_relaxation(cplex_cb_ctx, ctx, threadid, numthreads);
        break;
    case CPX_CALLBACKCONTEXT_GLOBAL_PROGRESS:
        // NOTE: Global progress is inherently thread safe
        //            See:
        //            https://www.ibm.com/docs/en/cofz/12.10.0?topic=callbacks-multithreading-generic
        result =
            cplex_on_global_progress(cplex_cb_ctx, ctx->solver, ctx->instance);
        break;
    case CPX_CALLBACKCONTEXT_THREAD_UP:
    case CPX_CALLBACKCONTEXT_THREAD_DOWN: {
        int activation = contextid == CPX_CALLBACKCONTEXT_THREAD_UP ? 1 : -1;
        result = cplex_on_thread_activation(activation, cplex_cb_ctx, ctx,
                                            threadid, numthreads);
        break;
    }

    default:
        assert(!"Invalid case");
        break;
    }

    if (ctx->solver->should_terminate) {
        CPXXcallbackabort(cplex_cb_ctx);
    }

    // NOTE:
    // From
    //  https://www.ibm.com/docs/en/cofz/12.10.0?topic=manual-cpxcallbackfunc
    //
    // The routine returns 0 (zero) if successful and nonzero if an error
    // occurs. Any value different from zero will result in an ungraceful
    // exit of CPLEX (usually with CPXERR_CALLBACK). Note that the actual
    // value returned is not propagated up the call stack. The only thing
    // that CPLEX checks is whether the returned value is zero or not. Do
    // not use a non-zero return value to stop optimization in case there is
    // no error. Use CPXXcallbackabort and CPXcallbackabort for that
    // purpose.

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

#ifndef NDEBUG
    CPXXsetintparam(solver->data->env, CPX_PARAM_SCRIND, 1);
    CPXXsetintparam(solver->data->env, CPX_PARAM_MIPDISPLAY, 3);
#endif

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

static void mip_solver_destroy(Solver *self) {

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
