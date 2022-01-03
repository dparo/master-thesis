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
};

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return (n * n + 2 * n + 1) / 4;
}

static void deactivate(CutSeparationPrivCtx *ctx) {
    free(ctx->index);
    free(ctx->value);
    free(ctx);
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
    const char sense = 'G';

    int32_t added_cuts = 0;

    for (int32_t c = 0; c < tour->num_comps; c++) {
        CPXNNZ pos = 0;
        double demand_sum = 0.0;

        for (int32_t i = 0; i < n; i++) {
            bool i_is_customer = i > 0;
            bool i_in_s = (tour->comp[i] == c) && i_is_customer;

            if (i == 0)
                assert(demand(instance, i) == 0.0);

            if (!i_in_s)
                continue;

            double d = demand(instance, i);
            demand_sum += d;
        }

        double r = fmod(demand_sum, Q);
        double rhs = 2.0 * ceil(demand_sum / Q);

        for (int32_t i = 0; i < n; i++) {
            bool i_is_customer = i > 0;
            bool i_in_s = (tour->comp[i] == c) && i_is_customer;

            if (!i_in_s)
                continue;

            double d = demand(instance, i);
            rhs += -2.0 * d / r;
        }

        for (int32_t i = 0; i < n; i++) {
            bool i_is_customer = i > 0;
            bool i_in_s = (tour->comp[i] == c) && i_is_customer;

            if (i == 0)
                assert(demand(instance, i) == 0.0);

            if (!i_in_s)
                continue;

            double d = demand(instance, i);

            ctx->index[pos] = get_y_mip_var_idx(instance, i);
            ctx->value[pos] = -2.0 * d / r;
            ++pos;

            for (int32_t j = 0; j < n; j++) {
                if (i == j)
                    continue;

                bool j_is_customer = j > 0;
                bool j_in_s = (tour->comp[j] == c) && j_is_customer;

                if (j_in_s)
                    continue;

                ctx->index[pos] = get_x_mip_var_idx(instance, i, j);
                ctx->value[pos] = 1.0;
                ++pos;
            }
        }
    }
}

static CutSeparationPrivCtx *activate(const Instance *instance,
                                      Solver *solver) {

    CutSeparationPrivCtx *ctx = malloc(sizeof(*ctx));
    int32_t nnz_ub = get_nnz_upper_bound(instance);

    int32_t n = instance->num_customers + 1;
    ctx->index = malloc(nnz_ub * sizeof(*ctx->index));
    ctx->value = malloc(nnz_ub * sizeof(*ctx->value));

    if (!ctx->index || !ctx->value) {
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
