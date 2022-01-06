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

#include "analyze-and-optimize.h"
#include "validation.h"

NodeInfoAnalysis analyze_node(const Instance *instance, int32_t i) {
#ifndef NDEBUG
    validate_symmetric_distances(instance);
#endif

    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    assert(i >= 0 && i < n);

    NodeInfoAnalysis info = {0};
    info.servable = instance->demands[i] <= Q;

    if (!info.servable) {
        // There is no way this node will be servable since it requires more
        // demand that is available in the truck
        info.obj_ub = INFINITY;
        info.obj_lb = INFINITY;
    } else {
        info.obj_ub = instance->profits[i];
        info.obj_lb = instance->profits[i];

        // NOTE(dparo):
        //      Independently from the state of the current tour (eg which
        //      vertices are visited, and which are vertices are taken), try
        //      to get reasonable bounds for the best and worst insertion case.
        //
        //      Best insertion case: pick the 2 best cost edges for inserting
        //      the node, and hypotheze to remove the worst edge from the
        //      remaining tour
        //      The worst insertion case: pick the 2 worst case edges for
        //      inserting the node, and hypotheze to remove the best edge from
        //      the remaining_tour

        double worst1 = -INFINITY;
        double worst2 = -INFINITY;
        int32_t worst1_idx = -1;
        int32_t worst2_idx = -1;

        double best1 = INFINITY;
        double best2 = INFINITY;
        int32_t best1_idx = -1;
        int32_t best2_idx = -1;

        // Scan for the first best/worst edge
        for (int32_t j = 0; j < n; j++) {
            double d = cptp_dist(instance, i, j);
            if (d > worst1) {
                worst1 = d;
                worst1_idx = j;
            }
            if (d < best1) {
                best1 = d;
                best1_idx = j;
            }
        }

        // Scan for the second best/worst edge
        for (int32_t j = 0; j < n; j++) {
            double d = cptp_dist(instance, i, j);
            if (j != worst1_idx && d > worst2) {
                worst2 = d;
                worst2_idx = j;
            }
            if (j != worst1_idx && d < best2) {
                best2 = d;
                best2_idx = j;
            }
        }

        assert(worst1_idx >= 0 && worst1_idx < n);
        assert(worst2_idx >= 0 && worst2_idx < n);
        assert(worst1_idx != worst2_idx);

        assert(best1_idx >= 0 && best1_idx < n);
        assert(best2_idx >= 0 && best2_idx < n);
        assert(best1_idx != best2_idx);
    }

    return info;
}
