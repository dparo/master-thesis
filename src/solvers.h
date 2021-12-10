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

#include "core.h"

static const SolverDescriptor MIP_SOLVER_DESCRIPTOR = {
    "mip",
    {
        {"SCRIND", TYPED_PARAM_BOOL, "false",
         "Enable or disable (default) CPLEX SCRIND and MIPDISPLAY parameters"},
        {"GSEC_CUTS", TYPED_PARAM_BOOL, "true", "Enable GSEC cut separation"},
        {"GLM_CUTS", TYPED_PARAM_BOOL, "false", "Enable GLM cuts separation"},
        {0},
    }};

static const SolverDescriptor STUB_SOLVER_DESCRIPTOR = {"stub",
                                                        {
                                                            {0},
                                                        }};

Solver mip_solver_create(const Instance *instance, SolverTypedParams *tparams,
                         double timelimit, int32_t seed);
Solver stub_solver_create(const Instance *instance, SolverTypedParams *tparams,
                          double timelimit, int32_t randomseed);

#if __cplusplus
}
#endif
