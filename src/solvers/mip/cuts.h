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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include "misc.h"
#include "types.h"
#include "mip.h"

typedef struct CutDescriptor {
    const char *name;
    const CutSeparationIface *iface;
    struct {
        const char *name;
        const ParamType type;
        const char *default_value;
        const char *glossary;
    } const params[];
} CutDescriptor;

extern const CutSeparationIface CUT_GSEC_IFACE;
extern const CutSeparationIface CUT_GLM_IFACE;

static const CutDescriptor CUT_GSEC_DESCRIPTOR = {
    "GSEC",
    &CUT_GSEC_IFACE,
    {{0}},
};

static const CutDescriptor CUT_GLM_DESCRIPTOR = {
    "GLM",
    &CUT_GLM_IFACE,
    {{0}},
};

#if __cplusplus
}
#endif
