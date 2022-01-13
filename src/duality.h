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

#include "core.h"
#include "core-utils.h"

typedef struct {
    double cap_lb;
    double cap_ub;
} CptpLagrangianMultipliers;

static inline double cptp_duality_dist(const Instance *instance,
                                       CptpLagrangianMultipliers lm, int32_t i,
                                       int32_t j) {
    double di = instance->demands[i];
    double dj = instance->demands[j];
    double rc = cptp_reduced_cost(instance, i, j);
    double result = rc + (lm.cap_ub - lm.cap_lb) * 0.5 * (di + dj);
    return result;
}

void generate_dual_instance(const Instance *instance, Instance *out,
                            CptpLagrangianMultipliers lm);

#if __cplusplus
}
#endif
