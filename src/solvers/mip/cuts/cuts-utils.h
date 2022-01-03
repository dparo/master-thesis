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
    int purgeable;
    int local_validity;
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

static inline void add_term_rhs(CutSeparationPrivCtxCommon *ctx,
                                SeparationInfo *info, double value) {
    UNUSED_PARAM(ctx);
    info->rhs += value;
}
static inline void add_term_lhs(CutSeparationPrivCtxCommon *ctx,
                                SeparationInfo *info, double value) {
    add_term_rhs(ctx, info, -value);
}

typedef struct {
    char *purgeable_str;
    char *sense_str;
} SeparationInfoAsString;

static inline SeparationInfoAsString
separation_info_to_str(SeparationInfo *info) {
    SeparationInfoAsString result = {0};
    switch (info->purgeable) {
    case CPX_USECUT_FORCE:
        result.purgeable_str = "<FORCE>";
        break;
    case CPX_USECUT_PURGE:
        result.purgeable_str = "<PURGE>";
        break;
    case CPX_USECUT_FILTER:
        result.purgeable_str = "<FILTER>";
        break;
    }

    switch (info->sense) {
    case 'G':
        result.sense_str = ">=";
        break;
    case 'L':
        result.sense_str = "<=";
        break;
    case 'E':
        result.sense_str = "==";
        break;
    }

    return result;
}

static inline bool push_fractional_cut(char *cut_name,
                                       CutSeparationFunctor *functor,
                                       CutSeparationPrivCtxCommon *ctx,
                                       SeparationInfo *info) {
    if (info->is_violated) {
        SeparationInfoAsString info_str = separation_info_to_str(info);

        log_trace(
            "Pushing fractional_cut    ---  %s  %s  %f %s %f   %s   nnz = %lld",
            cut_name, info_str.purgeable_str, info->lhs, info_str.sense_str,
            info->rhs,
            info->local_validity ? "(locally valid only)" : "(globally valid)",
            info->num_vars);

        if (!mip_cut_fractional_sol(functor, info->num_vars, info->rhs,
                                    info->sense, ctx->index, ctx->value,
                                    info->purgeable, info->local_validity)) {
            log_fatal("%s :: %s Failed push of fractional cut", __func__,
                      cut_name);
            return false;
        }
    }

    return true;
}

static inline bool push_integral_cut(char *cut_name,
                                     CutSeparationFunctor *functor,
                                     CutSeparationPrivCtxCommon *ctx,
                                     SeparationInfo *info) {
    if (info->is_violated) {
        SeparationInfoAsString info_str = separation_info_to_str(info);

        log_trace("Pusing integral_cut    ---  %s  %s  %f %s %f    nnz = %lld",
                  cut_name, info_str.purgeable_str, info->lhs,
                  info_str.sense_str, info->rhs, info->num_vars);

        if (!mip_cut_integral_sol(functor, info->num_vars, info->rhs,
                                  info->sense, ctx->index, ctx->value)) {
            log_fatal("%s :: %s Failed push of integral cut", __func__,
                      cut_name);
            return false;
        }
    }

    return true;
}

#if __cplusplus
}
#endif
