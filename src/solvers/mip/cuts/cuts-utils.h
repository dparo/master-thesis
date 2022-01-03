#pragma once

#if __cplusplus
extern "C" {
#endif

#include "../mip.h"
#include "../cuts.h"

typedef struct {
    char sense;
    CPXDIM *index;
    double *value;
} CutSeparationPrivCtxCommon;

typedef struct {
    bool is_violated;
    CPXNNZ nnz;
    double lhs;
    double rhs;
    CPXNNZ pos;
} SeparationInfo;

static inline bool is_violated_cut(CutSeparationPrivCtxCommon *ctx,
                                   SeparationInfo *info, double eps) {
    switch (ctx->sense) {
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
                                SeparationInfo *info, double *vstar,
                                CPXDIM var_index, double value) {
    ctx->index[info->pos] = var_index;
    ctx->value[info->pos] = value;
    info->lhs += value * vstar[var_index];
    ++info->pos;
}

static inline void push_var_rhs(CutSeparationPrivCtx *ctx, SeparationInfo *info,
                                double *vstar, CPXDIM var_index, double value) {
    push_var_lhs(ctx, info, vstar, var_index, -value);
}

static inline void push_term_rhs(CutSeparationPrivCtx *ctx,
                                 SeparationInfo *info, double value) {
    info->rhs += value;
}
static inline void push_term_lhs(CutSeparationPrivCtx *ctx,
                                 SeparationInfo *info, double value) {
    push_term_rhs(ctx, info, -value);
}

#if __cplusplus
}
#endif
