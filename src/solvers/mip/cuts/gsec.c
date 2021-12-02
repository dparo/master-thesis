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

static inline CPXNNZ get_nnz_upper_bound(const Instance *instance) {
    int32_t n = instance->num_customers + 1;
    return 1 + (n * n) / 4;
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
                           const double *vstar) {
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
    Solver *solver = self->solver;
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

    double rhs = 0;
    char sense = 'G';

    assert(*comp(tour, 0) == 0);

    CPXNNZ nnz_upper_bound = get_nnz_upper_bound(instance);

    // Note start from c = 1. Subtour Elimination Constraints that include the
    // depot are NOT valid.
    for (int32_t c = 1; c < tour->num_comps; c++) {

        assert(ctx->cnnodes[c] >= 2);

        CPXNNZ nnz = 1 + ctx->cnnodes[c] * (n - ctx->cnnodes[c]);
        CPXNNZ cnt = 0;

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
                        ctx->index[cnt] =
                            (CPXDIM)get_x_mip_var_idx(instance, i, j);
                        ctx->value[cnt] = +1.0;
                        cnt++;
                    }
                }
            }
        }

        assert(cnt < nnz_upper_bound);
        assert(cnt == nnz - 1);

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
                assert(*comp(tour, i) >= 1);

                ctx->index[nnz - 1] = (CPXDIM)get_y_mip_var_idx(instance, i);
                ctx->value[nnz - 1] = -2.0;

                log_trace(
                    "%s :: Adding GSEC constraint for component %d vertex %d, "
                    "(num_of_nodes_in_each_comp[%d] = %d, nnz = %lld)",
                    __func__, c, i, c, ctx->cnnodes[c], nnz);

                // NOTE::
                //      https://www.ibm.com/docs/en/icos/12.10.0?topic=c-cpxxcallbackrejectcandidate-cpxcallbackrejectcandidate
                //  You can call this routine more than once in the same
                //  callback invocation. CPLEX will accumulate the constraints
                //  from all such calls.

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
