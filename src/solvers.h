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
        {"NUM_THREADS", TYPED_PARAM_INT32, "0",
         "Set the number of threads to use. Default 0, means autodetect based "
         "on the number of cores available"},
        {"APPLY_UPPER_CUTOFF", TYPED_PARAM_BOOL, "false",
         "Apply upper cutoff value (CPX_PARAM_CUTUP) by using the "
         "zero_reduced_cost_threshold"},
        {"APPLY_LOWER_CUTOFF", TYPED_PARAM_BOOL, "true",
         "Apply lower cutoff value (CPX_PARAM_CUTLO) by using a "
         "trivially computed lower bound for the objective function"},
        {"PRICER_MODE", TYPED_PARAM_BOOL, "true",
         "Behave as pricer. Terminate solution process as soon as a negative "
         "value tour is found, without proving its optimality (there may exist "
         "more optimal tours) "},
        {"INS_HEUR_WARM_START", TYPED_PARAM_BOOL, "true",
         "Warm start the MIP solver by using an insertion heuristic for "
         "finding an initial solution"},
        {"APPLY_POLISHING_AFTER_WARM_START", TYPED_PARAM_BOOL, "false",
         "Polish the initial warm start solutions right away before beginning "
         "the Branch&Cut procedure"},
        {"GSEC_CUTS", TYPED_PARAM_BOOL, "true", "Enable GSEC cut separation"},
        {"GLM_CUTS", TYPED_PARAM_BOOL, "true", "Enable GLM cuts separation"},
        {"RCI_CUTS", TYPED_PARAM_BOOL, "true", "Enable RCI cuts separation"},

        {"GSEC_FRAC_CUTS", TYPED_PARAM_BOOL, "false",
         "Enable GSEC cut separation for fractional solutions. Param "
         "`GSEC_CUTS` must also be enabled for this to take effect."},
        {"GLM_FRAC_CUTS", TYPED_PARAM_BOOL, "true",
         "Enable GLM cut separation for fractional solutions. Param "
         "`GLM_CUTS` must also be enabled for this to take effect."},
        {"RCI_FRAC_CUTS", TYPED_PARAM_BOOL, "true",
         "Enable RCI cut separation for fractional solutions. Param "
         "`RCI_CUTS` must also be enabled for this to take effect."},
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
