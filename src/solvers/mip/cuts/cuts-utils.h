/*
 * Copyright (c) 2022 Davide Paro
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
                                   SeparationInfo *info, double tolerance) {
    assert(tolerance >= 0.0);

    bool valid = true;

    bool greater_eq = info->lhs >= (info->rhs - tolerance);
    bool lower_eq = info->lhs <= (info->rhs + tolerance);

    switch (info->sense) {
    case 'G':
        valid = greater_eq;
        break;
    case 'L':
        valid = lower_eq;
        break;
    case 'E':
        valid = greater_eq && lower_eq;
        break;
    default:
        assert(0);
        break;
    }

    return !valid;
}

static inline void push_var_lhs(CutSeparationPrivCtxCommon *ctx,
                                SeparationInfo *info, const double *vstar,
                                double value, CPXDIM var_index) {
    assert(isnan(value) == false && 0 == isinf(value));

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
    assert(isnan(value) == false && 0 == isinf(value));

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

static inline void validate_cut_info(CutSeparationFunctor *functor,
                                     CutSeparationPrivCtxCommon *ctx,
                                     SeparationInfo *info,
                                     const double *vstar) {
    UNUSED_PARAM(functor);
    UNUSED_PARAM(ctx);
    UNUSED_PARAM(info);
#ifndef NDEBUG

    double lhs = 0.0;
    for (CPXDIM i = 0; i < info->num_vars; i++) {
        lhs += vstar[ctx->index[i]] * ctx->value[i];
    }

    assert(feq(lhs, info->lhs, 1e-5));
#endif
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

        log_trace("Pushing integral_cut    ---  %s  %s  %f %s %f    nnz = %lld",
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
