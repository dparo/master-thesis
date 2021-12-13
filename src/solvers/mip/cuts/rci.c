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

ATTRIB_MAYBE_UNUSED static const double EPS = 1e-6;

struct CutSeparationPrivCtx {
    CPXDIM *index;
    double *value;

    PushRelabelCtx push_relabel_ctx;
    MaxFlowResult max_flow_result;
};

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return 4 * (n * n * n) / 27;
}

static void deactivate(CutSeparationPrivCtx *ctx) {
    max_flow_result_destroy(&ctx->max_flow_result);
    push_relabel_ctx_destroy(&ctx->push_relabel_ctx);
    free(ctx->index);
    free(ctx->value);
    free(ctx);
}

static bool integral_sep(CutSeparationFunctor *self, const double obj_p,
                         const double *vstar, Tour *tour) {}

static CutSeparationPrivCtx *activate(const Instance *instance,
                                      Solver *solver) {

    CutSeparationPrivCtx *ctx = malloc(sizeof(*ctx));
    int32_t nnz_ub = get_nnz_upper_bound(instance);

    int32_t n = instance->num_customers + 1;
    ctx->index = malloc(nnz_ub * sizeof(*ctx->index));
    ctx->value = malloc(nnz_ub * sizeof(*ctx->value));
    ctx->max_flow_result = max_flow_result_create(n);
    ctx->push_relabel_ctx = push_relabel_ctx_create(n);

    if (!ctx->index || !ctx->value || !ctx->max_flow_result.bipartition.data ||
        !push_relabel_ctx_is_valid(&ctx->push_relabel_ctx)) {
        deactivate(ctx);
        return NULL;
    }

    return ctx;
}

const CutSeparationIface CUT_RCI_IFACE = {
    .activate = activate,
    .deactivate = deactivate,
    .fractional_sep = NULL,
    .integral_sep = integral_sep,
};
