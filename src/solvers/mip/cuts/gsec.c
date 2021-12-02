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
#include "network.h"

const double EPS = 1e-6;

struct CutSeparationPrivCtx {
    CPXDIM *index;
    double *value;
    int32_t *cnnodes;

    PushRelabelCtx push_relabel_ctx;
    FlowNetwork network;
    MaxFlowResult max_flow_result;
};

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return 1 + (n * n) / 4;
}

static inline bool is_violated_cut(double flow, double y_i) {
    return flt(flow, 2.0 * y_i, EPS);
}

static void deactivate(CutSeparationPrivCtx *ctx) {
    flow_network_destroy(&ctx->network);
    max_flow_result_destroy(&ctx->max_flow_result);
    push_relabel_ctx_destroy(&ctx->push_relabel_ctx);
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
    ctx->network = flow_network_create(n);
    ctx->max_flow_result = max_flow_result_create(n);
    ctx->push_relabel_ctx = push_relabel_ctx_create(n);

    if (!ctx->index || !ctx->value || !ctx->cnnodes || !ctx->network.cap ||
        !ctx->network.flow || !ctx->max_flow_result.bipartition.data ||
        !push_relabel_ctx_is_valid(&ctx->push_relabel_ctx)) {
        deactivate(ctx);
        return NULL;
    }

    return ctx;
}

static bool fractional_sep(CutSeparationFunctor *self, const double obj_p,
                           const double *vstar) {

    CutSeparationPrivCtx *ctx = self->ctx;
    const Instance *instance = self->instance;
    const int32_t n = instance->num_customers + 1;

    ctx->network.source_vertex = 0;

    do {
        ctx->network.sink_vertex = rand() % (instance->num_customers + 1);
    } while (ctx->network.sink_vertex == 0 &&
             ctx->network.sink_vertex == ctx->network.source_vertex);

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
            *network_cap(&ctx->network, i, j) = cap;
        }
    }

    double max_flow = push_relabel_max_flow2(
        &ctx->network, &ctx->max_flow_result, &ctx->push_relabel_ctx);

    log_trace("%s :: max_flow = %g\n", __func__, max_flow);

    // Skip the depot node (start from h=1)
    for (int32_t h = 1; h < n; h++) {
        double y_h = vstar[get_y_mip_var_idx(instance, h)];
        int32_t bp_h = ctx->max_flow_result.bipartition.data[h];

        if (bp_h == 0 && is_violated_cut(max_flow, y_h)) {

            // Separate the cut
            double rhs = 0;
            char sense = 'G';
            CPXNNZ nnz = 0;
            int purgeable = CPX_USECUT_PURGE;
            int local_validity = 0; // (Globally valid)

            for (int32_t i = 1; i < n; i++) {
                for (int32_t j = 0; j < n; j++) {
                    int32_t bp_i = ctx->max_flow_result.bipartition.data[i];
                    int32_t bp_j = ctx->max_flow_result.bipartition.data[j];
                    if (bp_i == 0 && bp_j == 1) {
                        nnz++;
                    }
                }
            }

            nnz += 1;

            int32_t pos = 0;
            for (int32_t i = 1; i < n; i++) {
                for (int32_t j = 0; j < n; j++) {
                    int32_t bp_i = ctx->max_flow_result.bipartition.data[i];
                    int32_t bp_j = ctx->max_flow_result.bipartition.data[j];
                    if (bp_i == 0 && bp_j == 1) {
                        ctx->index[pos] = get_x_mip_var_idx(instance, i, j);
                        ctx->value[pos] = 1.0;
                        ++pos;
                    }
                }
            }

            ctx->index[pos] = get_y_mip_var_idx(instance, h);
            ctx->value[pos] = -2.0;

            if (!mip_cut_fractional_sol(self, nnz, rhs, sense, ctx->index,
                                        ctx->value, purgeable,
                                        local_validity)) {
                log_fatal(
                    "%s :: Failed to generate cut for fractional solution",
                    __func__);
                goto failure;
            }
        }
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

    // NOTE: In alternative, see function CCcut_connect_component of Concorde to
    // use a more efficient function
    // No separation is needed

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
        assert(*comp(tour, i) < tour->num_comps);
        if (*comp(tour, i) >= 0) {
            ++(ctx->cnnodes[*comp(tour, i)]);
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

    double rhs = 0.0;
    char sense = 'G';

    assert(*comp(tour, 0) == 0);

    CPXNNZ nnz_upper_bound = get_nnz_upper_bound(instance);

    // NOTE:
    // Start from c = 1. GSECS that include the depot node are NOT valid.
    for (int32_t c = 1; c < tour->num_comps; c++) {

        assert(ctx->cnnodes[c] >= 2);

        double flow = 0.0;
        CPXNNZ nnz = 1 + ctx->cnnodes[c] * (n - ctx->cnnodes[c]);
        CPXNNZ pos = 0;

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
                        ctx->index[pos] =
                            (CPXDIM)get_x_mip_var_idx(instance, i, j);
                        ctx->value[pos] = +1.0;
                        double x = vstar[get_x_mip_var_idx(instance, i, j)];
                        flow += x;
                        ++pos;
                    }
                }
            }
        }

        assert(pos < nnz_upper_bound);
        assert(pos == nnz - 1);

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

        int32_t added_cuts = 0;
        for (int32_t i = 0; i < n; i++) {
            if (*comp(tour, i) == c) {

                double y = vstar[get_y_mip_var_idx(instance, i)];
                assert(is_violated_cut(flow, y));
                assert(*comp(tour, i) >= 1);

                ctx->index[nnz - 1] = (CPXDIM)get_y_mip_var_idx(instance, i);
                ctx->value[nnz - 1] = -2.0;

                log_trace("%s :: Adding GSEC constraint (%g >= 2.0 * %g) for "
                          "component %d, vertex "
                          "%d "
                          "(num_of_nodes_in_each_comp[%d] = %d, nnz = %lld)",
                          __func__, flow, y, c, i, c, ctx->cnnodes[c], nnz);

                if (!mip_cut_integral_sol(self, nnz, rhs, sense, ctx->index,
                                          ctx->value)) {
                    log_fatal("%s :: Failed cut of integral solution",
                              __func__);
                    goto failure;
                }
                added_cuts += 1;
            }
        }
        assert(added_cuts == ctx->cnnodes[c]);
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
