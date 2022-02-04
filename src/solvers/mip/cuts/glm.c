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
#include "./cuts-utils.h"

static const double FRACTIONAL_VIOLATION_TOLERANCE = 0.1;
static const double INTEGRAL_VIOLATION_TOLERANCE = 1e-2;
ATTRIB_MAYBE_UNUSED static const double EPS = 1e-6;

struct CutSeparationPrivCtx {
    CutSeparationPrivCtxCommon super;
};

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return ((n + 1) * (n + 1)) / 4;
}

static void deactivate(CutSeparationPrivCtx *ctx) {
    free(ctx->super.index);
    free(ctx->super.value);
    free(ctx);
}

static CutSeparationPrivCtx *activate(const Instance *instance,
                                      Solver *solver) {

    CutSeparationPrivCtx *ctx = malloc(sizeof(*ctx));
    size_t nnz_ub = get_nnz_upper_bound(instance);

    ctx->super.index = malloc(nnz_ub * sizeof(*ctx->super.index));
    ctx->super.value = malloc(nnz_ub * sizeof(*ctx->super.value));

    if (!ctx->super.index || !ctx->super.value) {
        deactivate(ctx);
        return NULL;
    }

    return ctx;
}

static inline SeparationInfo separate(CutSeparationFunctor *self,
                                      const double *vstar, int32_t *colors,
                                      int32_t curr_color, double max_flow,
                                      double tolerance) {
    SeparationInfo info = {0};
    CutSeparationPrivCtx *ctx = self->ctx;
    const Instance *instance = self->instance;
    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    info.sense = 'G';

    assert(curr_color != colors[0]);
    assert(demand(instance, 0) == 0.0);

    int32_t set_s_size = 0;
    for (int32_t i = 0; i < n; i++) {
        bool i_in_s = (colors[i] == curr_color);
        if (i_in_s) {
            ++set_s_size;
        }
    }

    assert(set_s_size >= 1);

    if (set_s_size >= 2) {
        for (int32_t i = 0; i < n; i++) {
            bool i_in_s = colors[i] == curr_color;

            if (!i_in_s) {
                continue;
            }

            for (int32_t j = 0; j < n; j++) {
                if (i == j) {
                    continue;
                }

                bool j_in_s = colors[j] == curr_color;

                if (j_in_s)
                    continue;

                assert(i_in_s && !j_in_s);
                assert(i != 0);
                assert(colors[i] != colors[j]);
                assert(colors[i] == curr_color);
                assert(colors[j] != curr_color);

                if (j == 0) {
                    assert(demand(instance, j) == 0.0);
                }

                double value = 1.0 - 2.0 * demand(instance, j) / Q;
                push_var_lhs(&ctx->super, &info, vstar, value,
                             (CPXDIM)get_x_mip_var_idx(instance, i, j));
            }
        }

        for (int32_t i = 0; i < n; i++) {
            bool i_in_s = colors[i] == curr_color;

            if (!i_in_s) {
                continue;
            }

            assert(i != 0);
            double value = -2.0 * demand(instance, i) / Q;
            push_var_lhs(&ctx->super, &info, vstar, value,
                         (CPXDIM)get_y_mip_var_idx(instance, i));
        }

        assert(info.num_vars);
        info.is_violated = is_violated_cut(&ctx->super, &info, tolerance);
        validate_cut_info(self, &ctx->super, &info, vstar);
    }

    info.purgeable = CPX_USECUT_FILTER;
    info.local_validity = 0; // (Globally valid)

    return info;
}

static bool fractional_sep(CutSeparationFunctor *self, const double obj_p,
                           const double *vstar, MaxFlowResult *mf) {
    UNUSED_PARAM(obj_p);

    CutSeparationPrivCtx *ctx = self->ctx;
    int32_t depot_color = mf->colors[0];
    SeparationInfo info =
        separate(self, vstar, mf->colors, depot_color == BLACK ? WHITE : BLACK,
                 mf->maxflow, FRACTIONAL_VIOLATION_TOLERANCE);
    if (!push_fractional_cut("GLM", self, &ctx->super, &info)) {
        return false;
    }

    return true;
}

static bool integral_sep(CutSeparationFunctor *self, const double obj_p,
                         const double *vstar, Tour *tour) {
    UNUSED_PARAM(obj_p);

    if (tour->num_comps == 1) {
        return true;
    }

    CutSeparationPrivCtx *ctx = self->ctx;
    int32_t added_cuts = 0;

    // NOTE:
    // Start from c = 1. GLM cuts that include the depot node are NOT valid.
    for (int32_t c = 1; c < tour->num_comps; c++) {
        SeparationInfo info = separate(self, vstar, tour->comp, c, 0.0,
                                       INTEGRAL_VIOLATION_TOLERANCE);
        if (!push_integral_cut("GLM", self, &ctx->super, &info)) {
            return false;
        }
        ++added_cuts;
    }

    log_info("%s :: Created %d GLM cuts", __func__, added_cuts);

    return true;
}

const CutSeparationIface CUT_GLM_IFACE = {
    .activate = activate,
    .deactivate = deactivate,
    .fractional_sep = fractional_sep,
    .integral_sep = integral_sep,
};
