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
    CPXDIM num_mip_vars;
    CPXDIM num_mip_constraints;
} SolverData;

struct CutSeparationIface;

typedef struct {
    CutSeparationPrivCtx *ctx;
    struct CutSeparationIface *iface;

    CPXCALLBACKCONTEXTptr cplex_cb_ctx;
    int32_t thread_id;
    int32_t num_threads;
    const Instance *instance;
    Solver *solver;
} CutSeparationFunctor;

typedef struct {
    CutSeparationPrivCtx *(*activate)(const Instance *instance, Solver *solver);
    void (*deactivate)(CutSeparationFunctor *self);

    bool (*fractional_sep)(CutSeparationFunctor *self);
    bool (*integral_sep)(CutSeparationFunctor *self);

} CutSeparationIface;

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

#if __cplusplus
}
#endif
