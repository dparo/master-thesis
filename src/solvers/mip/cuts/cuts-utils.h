#pragma once

#if __cplusplus
extern "C" {
#endif

#include "../mip.h"
#include "../cuts.h"

typedef struct {
    CPXDIM *index;
    double *value;
} CutSeparationPrivCtxCommon;

typedef struct {
    char sense;
    bool is_violated;
    double lhs;
    double rhs;
    CPXNNZ num_vars;
} SeparationInfo;

static inline bool is_violated_cut(CutSeparationPrivCtxCommon *ctx,
                                   SeparationInfo *info, double eps) {
    switch (info->sense) {
    case 'G':
        return !fgte(info->lhs, info->rhs, eps);
    case 'L':
        return !flte(info->lhs, info->rhs, eps);
    case 'E':
        return !feq(info->lhs, info->rhs, eps);
    default:
        assert(0);
    }
}

static inline void push_var_lhs(CutSeparationPrivCtxCommon *ctx,
                                SeparationInfo *info, const double *vstar,
                                double value, CPXDIM var_index) {
    ctx->index[info->num_vars] = var_index;
    ctx->value[info->num_vars] = value;
    info->lhs += value * vstar[var_index];
    ++info->num_vars;
}

static inline void push_var_rhs(CutSeparationPrivCtxCommon *ctx,
                                SeparationInfo *info, const double *vstar,
                                double value, CPXDIM var_index) {
    push_var_lhs(ctx, info, vstar, var_index, -value);
}

static inline void add_term_rhs(CutSeparationPrivCtx *ctx, SeparationInfo *info,
                                double value) {
    info->rhs += value;
}
static inline void add_term_lhs(CutSeparationPrivCtx *ctx, SeparationInfo *info,
                                double value) {
    add_term_rhs(ctx, info, -value);
}

#if __cplusplus
}
#endif
