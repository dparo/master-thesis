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

static const double EPS = 1e-6;

struct CutSeparationPrivCtx {
    CPXDIM *index;
    double *value;
    int32_t *cnnodes;

    PushRelabelCtx push_relabel_ctx;
    MaxFlowResult max_flow_result;
};

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return 1 + (n * n) / 4;
}

static void deactivate(CutSeparationPrivCtx *ctx) {
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
    ctx->max_flow_result = max_flow_result_create(n);
    ctx->push_relabel_ctx = push_relabel_ctx_create(n);

    if (!ctx->index || !ctx->value || !ctx->cnnodes ||
        !ctx->max_flow_result.bipartition.data ||
        !push_relabel_ctx_is_valid(&ctx->push_relabel_ctx)) {
        deactivate(ctx);
        return NULL;
    }

    return ctx;
}

static bool fractional_sep(CutSeparationFunctor *self, const double obj_p,
                           const double *vstar, FlowNetwork *network) {
    assert(!"TODO");
    return false;
}

static bool integral_sep(CutSeparationFunctor *self, const double obj_p,
                         const double *vstar, Tour *tour) {
    if (tour->num_comps == 1) {
        return true;
    }

    CutSeparationPrivCtx *ctx = self->ctx;
    const Instance *instance = self->instance;
    const int32_t n = instance->num_customers + 1;

    const double Q = instance->vehicle_cap;
    const double rhs = 0.0;
    const char sense = 'G';

    CPXNNZ pos = 0;
    int32_t added_cuts = 0;

    // NOTE:
    // Start from c = 1. GLM cuts that include the depot node are NOT valid.
    for (int32_t c = 1; c < tour->num_comps; c++) {
        for (int32_t i = 0; i < n; i++) {
            if (tour->comp[i] == c)
                continue;
            for (int32_t j = 0; j < n; j++) {
                if (i == j)
                    continue;
                if (tour->comp[j] != c)
                    continue;

                if (i == 0)
                    assert(demand(instance, i) == 0.0);

                assert(tour->comp[i] != c && tour->comp[j] == c);

                double d = demand(instance, i);
                ctx->index[pos] = get_x_mip_var_idx(instance, i, j);
                ctx->value[pos] = 1 - 2.0 / Q * d;
                ++pos;
            }
        }

        for (int32_t j = 0; j < n; j++) {
            if (tour->comp[j] != c)
                continue;
            double d = demand(instance, j);
            ctx->index[pos] = get_y_mip_var_idx(instance, j);
            ctx->value[pos] = -2.0 / Q * d;
            ++pos;
        }

        CPXNNZ nnz = pos;
        log_trace("%s :: Adding GLM constraint", __func__);

        if (!mip_cut_integral_sol(self, nnz, rhs, sense, ctx->index,
                                  ctx->value)) {
            log_fatal("%s :: Failed cut of integral solution", __func__);
            goto failure;
        }
        added_cuts += 1;
    }

    return true;
failure:
    return false;
}

const CutSeparationIface CUT_GLM_IFACE = {
    .activate = activate,
    .deactivate = deactivate,
    .fractional_sep = NULL,
    .integral_sep = integral_sep,
};
