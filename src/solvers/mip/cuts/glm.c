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

static const char CONSTRAINT_SENSE = 'G';

struct CutSeparationPrivCtx {
    CPXDIM *index;
    double *value;
};

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return 4 * (n * n * n) / 27;
}

static void deactivate(CutSeparationPrivCtx *ctx) {
    free(ctx->index);
    free(ctx->value);
    free(ctx);
}

static inline bool is_violated_cut(double lhs, double rhs) {
    switch (CONSTRAINT_SENSE) {
    case 'G':
        return !fgte(lhs, rhs, EPS);
    case 'L':
        return !flte(lhs, rhs, EPS);
    case 'E':
        return !feq(lhs, rhs, EPS);
    default:
        assert(0);
    }
}

static CutSeparationPrivCtx *activate(const Instance *instance,
                                      Solver *solver) {

    CutSeparationPrivCtx *ctx = malloc(sizeof(*ctx));
    int32_t nnz_ub = get_nnz_upper_bound(instance);

    ctx->index = malloc(nnz_ub * sizeof(*ctx->index));
    ctx->value = malloc(nnz_ub * sizeof(*ctx->value));

    if (!ctx->index || !ctx->value) {
        deactivate(ctx);
        return NULL;
    }

    return ctx;
}

typedef struct {
    bool is_violated;
    CPXNNZ nnz;
    double lhs;
    double rhs;
} SeparationInfo;

static inline SeparationInfo separate(CutSeparationFunctor *self,
                                      const double obj_p, const double *vstar,
                                      int32_t *colors, int32_t curr_color,
                                      double max_flow) {

    SeparationInfo info = {0};
    CutSeparationPrivCtx *ctx = self->ctx;
    const Instance *instance = self->instance;
    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    // Curr color must be different from the color of the depot.
    assert(curr_color != colors[0]);
    assert(demand(instance, 0) == 0.0);

    int32_t set_s_size = 0;
    for (int32_t i = 0; i < n; i++) {
        bool i_in_s = (colors[i] == curr_color);
        if (i_in_s) {
            ++set_s_size;
        }
    }

    double lhs = 0.0;
    double rhs = 0.0;
    double flow = 0.0;
    CPXNNZ pos = 0;

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
                double d = demand(instance, j);

                assert(i != 0);
                assert(colors[i] != colors[j]);
                assert(colors[i] == curr_color);
                assert(colors[j] != curr_color);

                double x = 1.0 - 2.0 * d / Q;
                ctx->index[pos] = (CPXDIM)get_x_mip_var_idx(instance, i, j);
                ctx->value[pos] = x;
                lhs += x;
                flow += vstar[get_x_mip_var_idx(instance, i, j)];
                ++pos;
            }
        }

        assert(feq(flow, max_flow, EPS));

        for (int32_t i = 0; i < n; i++) {
            bool i_in_s = colors[i] == curr_color;

            if (!i_in_s)
                continue;

            double d = demand(instance, i);
            double x = -2.0 * d / Q;
            assert(i != 0);
            ctx->index[pos] = (CPXDIM)get_y_mip_var_idx(instance, i);
            ctx->value[pos] = x;
            lhs += x;
            ++pos;
        }
    } else {
        pos = 0;
    }

    info.nnz = pos;

    if (info.nnz) {
        info.is_violated = is_violated_cut(lhs, rhs);
        info.lhs = lhs;
        info.rhs = rhs;
    }

    return info;
}

static bool fractional_sep(CutSeparationFunctor *self, const double obj_p,
                           const double *vstar, MaxFlowResult *mf) {
    CutSeparationPrivCtx *ctx = self->ctx;
    const int purgeable = CPX_USECUT_FILTER;
    const int local_validity = 0; // (Globally valid)

    int32_t depot_color = mf->colors[0];
    SeparationInfo info =
        separate(self, obj_p, vstar, mf->colors,
                 depot_color == BLACK ? WHITE : BLACK, mf->maxflow);
    if (info.nnz && info.is_violated) {
        log_trace("%s :: Adding GLM fractional constraint", __func__);

        if (!mip_cut_fractional_sol(self, info.nnz, info.rhs, CONSTRAINT_SENSE,
                                    ctx->index, ctx->value, purgeable,
                                    local_validity)) {
            log_fatal("%s :: Failed cut of for fractional solution solution",
                      __func__);
            goto failure;
        }
    }

    return true;
failure:
    return false;
}

static bool integral_sep(CutSeparationFunctor *self, const double obj_p,
                         const double *vstar, Tour *tour) {
    if (tour->num_comps == 1) {
        return true;
    }

    CutSeparationPrivCtx *ctx = self->ctx;
    const char sense = 'G';
    int32_t added_cuts = 0;

    // NOTE:
    // Start from c = 1. GLM cuts that include the depot node are NOT valid.
    for (int32_t c = 1; c < tour->num_comps; c++) {
        SeparationInfo info = separate(self, obj_p, vstar, tour->comp, c, 0.0);
        if (info.nnz && info.is_violated) {
            log_trace("%s :: Adding GLM integral constraint", __func__);

            if (!mip_cut_integral_sol(self, info.nnz, info.rhs, sense,
                                      ctx->index, ctx->value)) {
                log_fatal("%s :: Failed cut of integral solution", __func__);
                goto failure;
            }
            added_cuts += 1;
        }
    }

    log_info("%s :: Created %d GLM cuts", __func__, added_cuts);

    return true;
failure:
    return false;
}

const CutSeparationIface CUT_GLM_IFACE = {
    .activate = activate,
    .deactivate = deactivate,
    .fractional_sep = fractional_sep,
    .integral_sep = integral_sep,
};
