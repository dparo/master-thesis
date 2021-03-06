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

#include "../mip.h"
#include "../cuts.h"

// NOTE(dparo): 8 Jan 2022
//       There is a trade off between which cut purgeability to use for GSEC:
//       Mainly we have 2 choices which are:
//         - CPX_USECUT_PURGE
//         - CPX_USECUT_FILTER
//          The latter one (CPX_USECUT_FILTER), tells CPLEX to score the cut
//       and use it only if it deems it be strong enough. If it does not
//       believe any GSEC cut is strong enough, it either resolve back
//       to its own internal cuts, or it fall backs to branching.
//          The first one (CPX_USECUT_PURGE), tells CPLEX to always use
//       GSEC cuts even if they are deemed ineffective. Notice however, that
//       CPLEX is still allowed to purge old GSEC cuts, thus the separation
//       procedure should be able to regenerate old cuts at any point.
//
//       When to prefer one or another:
//          - When CPX_USECUT_PURGE is used:
//              - PROS:
//                - The MIP model can be solved at the root LP node without
//                  incuring in any branching.
//              - CONS:
//                - Less use of available cores. CPLEX can make high use
//                  of parallelization only when multiple B&B nodes are
//                  opened
//                - CPLEX is an high engineered piece of software.
//                  Maybe if it deems the cut
//                  to be ineffective is probably better to take in
//                  consideration its decision.
//          - When CPX_USECUT_FILTER is used:
//              - PROS:
//                - Anticipates branching, allowing more cores to be used.
//                - Potentially avoids spending too much time separating cuts
//                  which are ineffective, wasting tremendous amount of time
//                  at the root LP node
//              - CONS:
//                - Increases memory consumption due to B&B nodes
//                  being generated (especially for big instances)
//                - Tradeoff: Due to CPLEX scoring each GSEC cut,
//                  generating many GSEC cuts
//                  (eg FRACTIONAL_VIOLATION_TOLERANCE low), costs
//                  more time per fractional separation iteration.
//                  Instead, setting FRACTIONAL_VIOLATION_TOLERANCE to a high
//                  value results in more branching
#define FRACTIONAL_CUT_PURGEABILITY CPX_USECUT_FILTER

static const double FRACTIONAL_VIOLATION_TOLERANCE = 1e-2;
static const double EPS = 1e-5;

struct CutSeparationPrivCtx {
    CPXDIM *index;
    double *value;
    int32_t *cnnodes;
};

static inline void validate_index_array(CutSeparationPrivCtx *ctx, CPXNNZ nnz) {
#ifndef NDEBUG
    // Assert that index array does not contain duplicates
    for (int32_t i = 0; i < nnz - 1; i++) {
        assert(ctx->index[i] >= 0);
        for (int32_t j = 0; j < nnz - 1; j++) {
            assert(ctx->index[j] >= 0);
            if (i != j) {
                assert(ctx->index[i] != ctx->index[j]);
            }
        }
    }
#endif
}

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return 1 + (n * n) / 4;
}

static inline bool is_violated_fractional_cut(double flow, double y_i) {
    bool valid = flow >= (2.0 * y_i - FRACTIONAL_VIOLATION_TOLERANCE);
    return !valid;
}

static void deactivate(CutSeparationPrivCtx *ctx) {
    free(ctx->index);
    free(ctx->value);
    free(ctx->cnnodes);
    free(ctx);
}

static CutSeparationPrivCtx *activate(const Instance *instance,
                                      Solver *solver) {
    CutSeparationPrivCtx *ctx = malloc(sizeof(*ctx));
    int32_t nnz_ub = get_nnz_upper_bound(instance);

    int32_t n = instance->num_customers + 1;
    ctx->index = malloc(nnz_ub * sizeof(*ctx->index));
    ctx->value = malloc(nnz_ub * sizeof(*ctx->value));
    ctx->cnnodes = malloc(n * sizeof(*ctx->cnnodes));

    if (!ctx->index || !ctx->value || !ctx->cnnodes) {
        deactivate(ctx);
        return NULL;
    }

    return ctx;
}

static bool fractional_sep(CutSeparationFunctor *self, const double obj_p,
                           const double *vstar, MaxFlowResult *mf,
                           double max_flow) {
    CutSeparationPrivCtx *ctx = self->ctx;
    const Instance *instance = self->instance;
    const int32_t n = instance->num_customers + 1;

    int32_t added_cuts = 0;

    int32_t depot_color = mf->colors[0];

    const int32_t source_vertex = mf->s;
    const int32_t sink_vertex = mf->t;

    assert(source_vertex != sink_vertex);
    assert(mf->colors[source_vertex] == BLACK);
    assert(mf->colors[sink_vertex] == WHITE);

    int32_t set_s_size = 0;

    // No way anything will be violated, so don't pay the cost
    // of the function
    if (mf->maxflow >= 2.0 - FRACTIONAL_VIOLATION_TOLERANCE) {
        return true;
    }

    // Scan for the city that violates the cut the most.
    // If none, there's no need to fill
    // the index, value array with a cut that will never
    // be violated
    int32_t best_violated_idx = -1;
    double max_violation_amt = INFINITY;

    for (int32_t i = 0; i < n; i++) {
        int32_t i_color = mf->colors[i];
        bool i_in_s = i_color != depot_color;
        if (i_in_s) {
            assert(i != 0);
            ++set_s_size;
            double y_i = vstar[get_y_mip_var_idx(instance, i)];

            double violation_amt = mf->maxflow - 2 * y_i;
            if (is_violated_fractional_cut(mf->maxflow, y_i) &&
                violation_amt < max_violation_amt) {
                max_violation_amt = violation_amt;
                best_violated_idx = i;
            }
        }
    }

    assert(set_s_size >= 1);

    //
    // NOTE(dparo): 9 Jan 2022
    //      Report to CPLEX a single GSEC per set S. The one that is violated
    //      the most. The reason is to reduce the processing/scoring that CPLEX
    //      needs to do per fractional cut iteration
    //
    if (best_violated_idx >= 0 && set_s_size >= 2) {
        // Separate the cut
        CPXNNZ nnz = 0;
        const double rhs = 0;
        const char sense = 'G';
        const int purgeable = FRACTIONAL_CUT_PURGEABILITY;
        const int local_validity = 0; // (Globally valid)

        double flow = 0.0;
        for (int32_t i = 0; i < n; i++) {
            int32_t i_color = mf->colors[i];
            bool i_in_s = i_color != depot_color;

            if (!i_in_s) {
                continue;
            }

            for (int32_t j = 0; j < n; j++) {
                if (i == j) {
                    continue;
                }

                int32_t j_color = mf->colors[j];
                bool j_in_s = j_color != depot_color;

                if (j_in_s) {
                    continue;
                }

                assert(i != 0);
                assert(i_color != j_color);
                assert(i_color != depot_color);
                assert(j_color == depot_color);
                ctx->index[nnz] = (CPXDIM)get_x_mip_var_idx(instance, i, j);
                ctx->value[nnz] = +1.0;
                double x = vstar[get_x_mip_var_idx(instance, i, j)];
                flow += x;
                ++nnz;
            }
        }

        assert(feq(flow, mf->maxflow, EPS));
        validate_index_array(ctx, nnz - 1);

        double y_i = vstar[get_y_mip_var_idx(instance, best_violated_idx)];

        ctx->index[nnz] =
            (CPXDIM)get_y_mip_var_idx(instance, best_violated_idx);
        ctx->value[nnz] = -2.0;

        log_trace("%s :: Adding GSEC fractional constraint (%g >= "
                  "2.0 * %g)"
                  " (|S| = %d, nnz = %lld)",
                  __func__, max_flow, y_i, set_s_size, nnz);

        if (!mip_cut_fractional_sol(self, nnz, rhs, sense, ctx->index,
                                    ctx->value, purgeable, local_validity)) {
            log_fatal("%s :: Failed to generate cut for fractional "
                      "solution",
                      __func__);
            goto failure;
        }

        added_cuts += 1;
    }

    return true;
failure:
    return false;
}

static bool integral_sep(CutSeparationFunctor *self, const double obj_p,
                         const double *vstar, Tour *tour) {
    // NOTE:
    //   Generalized Subtour Elimination Constraints (GSECs) separation based on
    //   the connected components

    // NOTE: An alternative implementation, may employ the
    // `CCcut_connect_component` function of Concorde

    if (tour->num_comps == 1) {
        return true;
    }

    CutSeparationPrivCtx *ctx = self->ctx;
    const Instance *instance = self->instance;
    const int32_t n = instance->num_customers + 1;

    for (int32_t c = 0; c < tour->num_comps; c++) {
        ctx->cnnodes[c] = 0;
    }

    // Count the number of nodes in each component
    for (int32_t i = 0; i < n; i++) {
        int32_t c = tour->comp[i];
        assert(c < tour->num_comps);
        if (c >= 0) {
            ++(ctx->cnnodes[c]);
        }
    }

    // Small sanity check
#ifndef NDEBUG
    {
        for (int32_t i = 0; i < tour->num_comps; i++) {
            assert(ctx->cnnodes[i] >= 2);
        }
    }
#endif

    const double rhs = 0.0;
    const char sense = 'G';

    assert(tour->comp[0] == 0);

    int32_t depot_color = 0;
    CPXNNZ nnz_upper_bound = get_nnz_upper_bound(instance);
    int32_t total_num_added_cuts = 0;

    // NOTE: The set S cannot contain the depot. Component index 0 always
    // contains the depot, and always induces a non violated cut for integral
    // solutions
    for (int32_t c = 1; c < tour->num_comps; c++) {
        assert(ctx->cnnodes[c] >= 2);

        double flow = 0.0;
        const CPXNNZ nnz = 1 + ctx->cnnodes[c] * (n - ctx->cnnodes[c]);
        CPXNNZ pos = 0;

        assert(nnz <= nnz_upper_bound);

        for (int32_t i = 0; i < n; i++) {
            bool i_is_customer = i > 0;
            bool i_in_s = (tour->comp[i] == c) && i_is_customer;

            if (!i_in_s) {
                continue;
            }

            for (int32_t j = 0; j < n; j++) {
                if (i == j) {
                    continue;
                }

                bool j_is_customer = j > 0;
                bool j_in_s = (tour->comp[j] == c) && j_is_customer;

                if (j_in_s) {
                    continue;
                }

                assert(i != 0);
                assert(tour->comp[i] != tour->comp[j]);
                assert(tour->comp[i] != depot_color);
                assert(tour->comp[i] == c);
                assert(tour->comp[j] != c);
                ctx->index[pos] = (CPXDIM)get_x_mip_var_idx(instance, i, j);
                ctx->value[pos] = +1.0;
                double x = vstar[get_x_mip_var_idx(instance, i, j)];
                flow += x;
                ++pos;
            }
        }

        assert(feq(flow, 0.0, EPS));
        assert(pos < nnz_upper_bound);
        assert(pos == nnz - 1);

        validate_index_array(ctx, nnz - 1);

        int32_t added_cuts = 0;

        for (int32_t i = 0; i < n; i++) {
            bool i_is_customer = i > 0;
            bool i_in_s = (tour->comp[i] == c) && i_is_customer;

            if (!i_in_s) {
                continue;
            }

            double y_i = vstar[get_y_mip_var_idx(instance, i)];
            assert(tour->comp[i] >= 1);

            ctx->index[nnz - 1] = (CPXDIM)get_y_mip_var_idx(instance, i);
            ctx->value[nnz - 1] = -2.0;

            log_trace("%s :: Adding GSEC constraint (%g >= 2.0 * %g) for "
                      "component %d, vertex "
                      "%d "
                      "(|S| = %d, nnz = %lld)",
                      __func__, flow, y_i, c, i, ctx->cnnodes[c], nnz);

            if (!mip_cut_integral_sol(self, nnz, rhs, sense, ctx->index,
                                      ctx->value)) {
                log_fatal("%s :: Failed cut of integral solution", __func__);
                goto failure;
            }

            added_cuts += 1;
        }
        assert(added_cuts == ctx->cnnodes[c]);
        total_num_added_cuts += added_cuts;
    }

    return true;

failure:
    return false;
}

const CutSeparationIface CUT_GSEC_IFACE = {
    .activate = activate,
    .deactivate = deactivate,
    .fractional_sep = fractional_sep,
    .integral_sep = integral_sep,
};
