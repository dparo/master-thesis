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
#include <math.h>

static inline int64_t hm_nentries(int32_t n) { return ((n * n) - n) / 2; }

// Number of entries in a full matrix of size `N x N`
// Eg N**2 - num_entries(diagonal)
static inline int64_t fm_nentries(int32_t n) { return (n * n) - n; }

static inline int32_t *tour_succ(Tour *tour, int32_t vehicle_idx,
                                 int32_t customer_idx) {
    return mati32_access(tour->succ, vehicle_idx, customer_idx,
                         tour->num_customers + 1, tour->num_vehicles);
}

static inline int32_t *tour_comp(Tour *tour, int32_t vehicle_idx,
                                 int32_t customer_idx) {

    return mati32_access(tour->comp, vehicle_idx, customer_idx,
                         tour->num_customers + 1, tour->num_vehicles);
}

static inline int32_t *tour_num_comps(Tour *tour, int32_t vehicle_idx) {
    return &tour->num_comps[vehicle_idx];
}

static inline bool tour_is_customer_served_from_vehicle(Tour *tour,
                                                        int32_t vehicle_idx,
                                                        int32_t customer_idx) {
    return *tour_comp(tour, vehicle_idx, customer_idx) != -1;
}

static inline bool
tour_is_customer_served_from_any_vehicle(Tour *tour, int32_t customer_idx) {
    for (int32_t i = 0; i < tour->num_vehicles; i++) {
        if (tour_is_customer_served_from_vehicle(tour, i, customer_idx)) {
            return true;
        }
    }
    return false;
}

static inline bool tour_are_all_customers_served(Tour *tour) {
    for (int32_t i = 1; i < tour->num_customers + 1; i++) {
        if (tour_is_customer_served_from_any_vehicle(tour, i)) {
            return true;
        }
    }
    return false;
}

static inline double solution_relgap(Solution *solution) {
    // Taken from:
    // https://www.ibm.com/docs/en/icos/12.10.0?topic=g-cpxxgetmiprelgap-cpxgetmiprelgap

    double ub = solution->upper_bound;
    double lb = solution->lower_bound;

    assert(lb <= ub);
    return (ub - lb) / (1e-10 + fabs(ub));
}

static inline void solver_params_push(SolverParams *params, char *name,
                                      char *value) {
    assert(params->num_params < MAX_NUM_SOLVER_PARAMS);
    params->params[params->num_params].name = name;
    params->params[params->num_params].value = value;
    params->num_params++;
}

#if __cplusplus
}
#endif
