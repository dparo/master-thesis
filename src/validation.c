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
#include <stdlib.h>
#include "types.h"
#include <assert.h>

void validate_symmetric_distances(const Instance *instance) {
#ifndef NDEBUG
    const int32_t n = instance->num_customers + 1;

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            double d1 = cptp_dist(instance, i, j);
            double d2 = cptp_dist(instance, j, i);
            assert(feq(d1, d2, 1e-5));
        }
    }
#else
    UNUSED_PARAM(instance);
#endif
}

void validate_solution(const Instance *instance, Solution *solution,
                       int32_t min_num_customers_served) {
#ifndef NDEBUG

    validate_tour(instance, &solution->tour, min_num_customers_served);

    // Upper bound should be bigger than lower bound
    double gap = solution_relgap(solution);
    assert(fgte(gap, 0.0, 1e-6));

    // The recomputed objective value should be the same of what is stored
    // inside the solution
    double obj = tour_eval(instance, &solution->tour);
    assert(feq(obj, solution->primal_bound, 1e-5));

#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(solution);
    UNUSED_PARAM(min_num_customers_served);
#endif
}

void validate_tour(const Instance *instance, Tour *tour,
                   int32_t min_num_customers_served) {
#ifndef NDEBUG
    int32_t n = tour->num_customers + 1;

    //
    // There should be only one subtour
    //
    assert(tour->num_comps == 1);

    //
    // The same subtour should share the same component idx
    //
    {
        // Depot is always part of the tour
        assert(*tcomp(tour, 0) == 0);

        // Check that each vertex is either part of the first and only tour
        // or otherwise it is not visited
        for (int32_t i = 0; i < n; i++) {
            int32_t c = *tcomp(tour, i);
            assert(c == 0 || c < 0);
        }

        // Check that each vertex if is part of a component index it has its
        // succ populated
        for (int32_t i = 0; i < n; i++) {
            int32_t c = *tcomp(tour, i);
            if (c >= 0) {
                assert(*tsucc(tour, i) >= 0 && *tsucc(tour, i) < n);
            }
        }
    }

    //
    // Validate succ array consistency:
    //       - Assert succ is within range
    //       - Vertices cannot be visited twice
    //
    {
        int32_t num_visited = 0;
        bool *visited = calloc(n, sizeof(*visited));
        if (*tcomp(tour, 0) >= 0) {
            assert(*tsucc(tour, 0) >= 0 && *tcomp(tour, 0) >= 0);
            visited[0] = true;
            ++num_visited;

            int32_t curr_vertex = 0;
            int32_t next_vertex;

            while ((next_vertex = *tsucc(tour, curr_vertex)) != 0) {
                assert(next_vertex != curr_vertex);
                assert(next_vertex >= 0 && next_vertex < n);
                assert(visited[next_vertex] == false);
                visited[next_vertex] = true;
                ++num_visited;
                curr_vertex = next_vertex;
            }
        }
        free(visited);

        assert(num_visited - 1 >= (min_num_customers_served));
    }

    //
    // Verify that the number of components that the tour reports
    // is actually consistent with what is encoded in the succ array
    //
    {

        bool *visited = calloc(n, sizeof(*visited));
        int32_t num_comps = 0;
        for (int32_t i = 0; i < n; i++) {
            if (visited[i] == false) {
                int32_t first_vertex = i;
                int32_t curr_vertex = first_vertex;

                visited[first_vertex] = true;

                for (int32_t retries = 0; retries < n; retries++) {
                    int32_t next_vertex = *tsucc(tour, curr_vertex);
                    if (next_vertex < 0) {
                        break;
                    }

                    visited[next_vertex] = true;

                    if (next_vertex == first_vertex) {
                        num_comps++;
                        break;
                    }

                    curr_vertex = next_vertex;
                }
            }
        }

        assert(num_comps == tour->num_comps);
        free(visited);
    }

    //
    // Validate capacity is not exceeded
    //
    assert(flte(tour_demand(instance, tour), instance->vehicle_cap, 1e-5));

#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tour);
#endif
}
