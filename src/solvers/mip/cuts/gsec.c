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

static inline bool is_violated_cut(double flow, double y_i) {
    return !fgte(flow, 2.0 * y_i, EPS);
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
                           const double *vstar, MaxFlowResult *mf) {
    CutSeparationPrivCtx *ctx = self->ctx;
    const Instance *instance = self->instance;
    const int32_t n = instance->num_customers + 1;

    int32_t added_cuts = 0;

    int32_t depot_color = mf->colors[0];

    const int32_t source_vertex = mf->source;
    const int32_t sink_vertex = mf->sink;

    assert(mf->colors[source_vertex] == BLACK);
    assert(mf->colors[sink_vertex] == WHITE);

    int32_t set_s_size = 0;

    for (int32_t i = 0; i < n; i++) {
        int32_t i_color = mf->colors[i];
        bool i_in_s = i_color != depot_color;
        if (i_in_s)
            ++set_s_size;
    }

    if (set_s_size >= 2) {
        // Separate the cut
        CPXNNZ nnz = 0;
        const double rhs = 0;
        const char sense = 'G';
        // NOTE(dparo): 3 Jan 2022
        //       Use CPX_USECUT_PURGE instead of the more classical
        //       CPX_USECUT_FILTER. We do not care if CPLEX deems the GSEC cuts
        //       to be ineffective. They define the original problem and must be
        //       in the formulation regardless. We can allow to purge the cuts
        //       however, because we can regenerate them fresly from any point
        //       of the solution process.
        // NOTE(dparo): 3 Jan 2022
        //       Also specifying this purgeable to be CPX_USECUT_PURGE has the
        //       extra benefit of reducing branching and thus memory
        //       consumption. Making ESPPRC instances with high number of nodes
        //       extremely more manageable
        const int purgeable = CPX_USECUT_PURGE;
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

        for (int32_t i = 0; i < n; i++) {
            double y_i = vstar[get_y_mip_var_idx(instance, i)];
            int32_t bp_i = mf->colors[i];

            bool i_is_customer = i > 0;
            bool i_in_s = (bp_i == depot_color) && i_is_customer;

            if (!i_in_s) {
                continue;
            }

            if (is_violated_cut(mf->maxflow, y_i)) {
                ctx->index[nnz] = (CPXDIM)get_y_mip_var_idx(instance, i);
                ctx->value[nnz] = -2.0;

                log_trace("%s :: Adding GSEC fractional constraint (%g >= "
                          "2.0 * %g)"
                          " (nnz = %lld)",
                          __func__, mf->maxflow, y_i, nnz);

                if (!mip_cut_fractional_sol(self, nnz, rhs, sense, ctx->index,
                                            ctx->value, purgeable,
                                            local_validity)) {
                    log_fatal("%s :: Failed to generate cut for fractional "
                              "solution",
                              __func__);
                    goto failure;
                }

                added_cuts += 1;
            }
        }
    }

    if (added_cuts > 0) {
        log_info("%s :: Created %d GSEC cuts", __func__, added_cuts);
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

    const double rhs = 0.0;
    const char sense = 'G';

    assert(*comp(tour, 0) == 0);

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
                assert(tour->comp[j] != c);
                ctx->index[pos] = (CPXDIM)get_x_mip_var_idx(instance, i, j);
                ctx->value[pos] = +1.0;
                double x = vstar[get_x_mip_var_idx(instance, i, j)];
                flow += x;
                ++pos;
            }
        }

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
            assert(is_violated_cut(flow, y_i));
            assert(*comp(tour, i) >= 1);

            ctx->index[nnz - 1] = (CPXDIM)get_y_mip_var_idx(instance, i);
            ctx->value[nnz - 1] = -2.0;

            log_trace("%s :: Adding GSEC constraint (%g >= 2.0 * %g) for "
                      "component %d, vertex "
                      "%d "
                      "(num_of_nodes_in_each_comp[%d] = %d, nnz = %lld)",
                      __func__, flow, y_i, c, i, c, ctx->cnnodes[c], nnz);

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

    log_info("%s :: Created %d GSEC cuts", __func__, total_num_added_cuts);

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
