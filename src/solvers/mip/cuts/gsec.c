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

struct CutSeparationPrivCtx {
    CPXDIM *index;
    double *value;
    int32_t *cnnodes;
};

static inline get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return 1 + (n * n) / 4;
}

static void destroy_ctx(CutSeparationPrivCtx *ctx) {
    free(ctx->index);
    free(ctx->value);
    free(ctx->cnnodes);
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
        destroy_ctx(ctx);
        return NULL;
    }

    return ctx;
}

static void deactivate(CutSeparationFunctor *self) {
    destroy_ctx(self->ctx);
    free(self->ctx);
    memset(self, 0, sizeof(*self));
}

static bool fractional_sep(CutSeparationFunctor *self, const double obj_p,
                           const double *vstar) {
    return false;
}

static bool integral_sep(CutSeparationFunctor *self, const double obj_p,
                         const double *vstar, const Tour *tour) {
    // No separation is needed
    if (tour->num_comps == 1) {
        return true;
    }

    CutSeparationPrivCtx *ctx = self->ctx;

    for (int32_t c = 0; c < tour->num_comps; c++) {
        ctx->cnnodes[c] = 0;
    }

    return false;
}

const CutSeparationIface CUT_GSEC_IFACE = {
    .activate = activate,
    .deactivate = deactivate,
    .fractional_sep = fractional_sep,
    .integral_sep = integral_sep,
};
