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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "core.h"
#include "core-utils.h"
#include "maxflow.h"

#ifdef COMPILED_WITH_CPLEX

// NOTE:
//       cplexx is the 64 bit version of the API, while cplex (one x) is the 32
//       bit version of the API.
#include <ilcplex/cplexx.h>
#include <ilcplex/cpxconst.h>

struct CutSeparationPrivCtx;
typedef struct CutSeparationPrivCtx CutSeparationPrivCtx;

typedef struct SolverData {
    int64_t begin_time;
    CPXENVptr env;
    CPXLPptr lp;
    int numcores;
    bool heur_pricer_mode;
    CPXDIM num_mip_vars;
    CPXDIM num_mip_constraints;
} SolverData;

struct CutSeparationIface;

typedef struct {
    int64_t num_cuts;
    int64_t accum_usecs;
} CutSeparationStatistics;

typedef struct {
    CutSeparationPrivCtx *ctx;
    const Instance *instance;
    Solver *solver;

    /// These fields are internally used/updated from the MIP solver.
    /// User cuts should not bother modifying and/or reading these fields.
    struct {
        CPXCALLBACKCONTEXTptr cplex_cb_ctx;
        CutSeparationStatistics fractional_stats;
        CutSeparationStatistics integral_stats;
    } internal;
} CutSeparationFunctor;

typedef struct {
    CutSeparationPrivCtx *(*activate)(const Instance *instance, Solver *solver);
    void (*deactivate)(CutSeparationPrivCtx *ctx);

    bool (*fractional_sep)(CutSeparationFunctor *self, const double obj_p,
                           const double *vstar, MaxFlowResult *mf,
                           double max_flow);
    bool (*integral_sep)(CutSeparationFunctor *self, const double obj_p,
                         const double *vstar, Tour *tour);
} CutSeparationIface;

static inline int32_t *tour_succ(Tour *tour, int32_t i) {
    return tsucc(tour, i);
}

static inline int32_t *tour_comp(Tour *tour, int32_t i) {
    return tcomp(tour, i);
}

static inline double cost(const Instance *instance, int32_t i, int32_t j) {
    return cptp_dist(instance, i, j);
}

static inline double profit(const Instance *instance, int32_t i) {
    assert(i >= 0 && i < instance->num_customers + 1);
    return instance->profits[i];
}

static inline double demand(const Instance *instance, int32_t i) {
    assert(i >= 0 && i < instance->num_customers + 1);
    return instance->demands[i];
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

static inline bool mip_cut_integral_sol(CutSeparationFunctor *ctx, CPXNNZ nnz,
                                        double rhs, char sense, CPXDIM *index,
                                        double *value) {
    CPXNNZ rmatbeg[] = {0};
    ctx->internal.integral_stats.num_cuts += 1;

    // NOTE::
    //      https://www.ibm.com/docs/en/icos/12.10.0?topic=c-cpxxcallbackrejectcandidate-cpxcallbackrejectcandidate
    //  You can call this routine more than once in the same
    //  callback invocation. CPLEX will accumulate the constraints
    //  from all such calls.
    if (0 != CPXXcallbackrejectcandidate(ctx->internal.cplex_cb_ctx, 1, nnz,
                                         &rhs, &sense, rmatbeg, index, value)) {
        log_fatal("%s :: Failed CPXXcallbackrejectcandidate", __func__);
        return false;
    }

    return true;
}

static inline bool mip_cut_fractional_sol(CutSeparationFunctor *ctx, CPXNNZ nnz,
                                          double rhs, char sense, CPXDIM *index,
                                          double *value, int purgeable,
                                          int local_validity) {
    CPXNNZ rmatbeg[] = {0};
    ctx->internal.fractional_stats.num_cuts += 1;

    // NOTE::
    //      https://www.ibm.com/docs/en/icos/12.9.0?topic=c-cpxxcallbackaddusercuts-cpxcallbackaddusercuts
    //  You can call this routine more than once in the same
    //  callback invocation. CPLEX will accumulate the cuts from all
    //  such calls.

    // NOTE: local_validity = 1: Means the cut is only locally valid. 0 instead,
    // means Globally valid
    if (0 != CPXXcallbackaddusercuts(ctx->internal.cplex_cb_ctx, 1, nnz, &rhs,
                                     &sense, rmatbeg, index, value, &purgeable,
                                     &local_validity)) {
        log_fatal("%s :: Failed CPXXcallbackaddusercuts", __func__);
        return false;
    }
    return true;
}

void unpack_mip_solution(const Instance *instance, Tour *t, double *vstar);

#endif

#if __cplusplus
}
#endif
