/*
 * Copyright (c) 2022 Davide Paro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "duality.h"

void generate_dual_instance(const Instance *instance, Instance *out,
                            CptpLagrangianMultipliers lm) {
    int32_t n = instance->num_customers + 1;

    if (!out->profits || !out->demands) {
        instance_destroy(out);
        *out = instance_copy(instance, true, false);
    } else {
        *out = instance_copy(instance, false, false);
    }

    out->edge_weight = malloc(sizeof(*out->edge_weight) * hm_nentries(n));

    for (int32_t i = 0; i < n; i++) {
        // NOTE(dparo):
        //     In the dual formulation all profits associated to each
        //     city are cleared, and are instead encoded in the reduced
        //     cost associated to each arc.
        out->profits[i] = 0.0;
        // demands are instead copied as is
        out->demands[i] = instance->demands[i];
    }

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            out->edge_weight[sxpos(n, i, j)] =
                cptp_duality_dist(instance, lm, i, j);
        }
    }
}
