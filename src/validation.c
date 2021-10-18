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

#include "validation.h"
#include "types.h"
#include <assert.h>

void validate_solution(const Instance *instance, Solution *solution) {
#ifndef NDEBUG

    // Upper bound should be bigger than lower bound
    double gap = solution_relgap(solution);
    assert(fgte(gap, 0.0, 1e-6));
    validate_tour(instance, &solution->tour);

    // The recomputed objective value should be the same of what is stored
    // inside the solution
    double obj = tour_eval(instance, &solution->tour);
    assert(fgapcmp(obj, solution->upper_bound, 1e-3));

#else
    UNUSED_PARAM(solution);
#endif
}

void validate_tour(const Instance *instance, Tour *tour) {
#ifndef NDEBUG

    // TODO:
    //     For now we are going to handle the single vehicle case
    //

    int32_t num_vehicles = 1;
    int32_t n = tour->num_customers + 1;

    // There should be only one subtour
    for (int32_t veh_idx = 0; veh_idx < num_vehicles; veh_idx++) {
        assert(*tour_num_comps(tour, veh_idx) == 1);
    }

    // The same subtour should share the same component idx
    for (int32_t veh_idx = 0; veh_idx < num_vehicles; veh_idx++) {
        // Depot is always part of the tour
        assert(*tour_comp(tour, veh_idx, 0) == 0);

        // Check that each vertex is either part of the first and only tour
        // or otherwise it is not visited
        for (int32_t i = 0; i < n; i++) {
            int32_t c = *tour_comp(tour, veh_idx, i);
            assert(c == 0 || c < 0);
        }
    }

    //
    // Validate capacity is not exceeded
    //
    for (int32_t veh_idx = 0; veh_idx < tour->num_vehicles; veh_idx++) {
        double demand = 0.0;
        if (*tour_comp(tour, veh_idx, 0) >= 0) {
            int32_t curr_vertex = 0;
            int32_t next_vertex;
            demand += instance->demands[0];

            while ((next_vertex = *tour_succ(tour, veh_idx, curr_vertex)) !=
                   0) {
                demand += instance->demands[next_vertex];
            }
        }

        assert(fgapcmp(demand, instance->vehicle_cap, 1e-3));
    }

#else
    UNUSED_PARAM(tour);
#endif
}
