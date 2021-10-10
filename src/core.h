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

#include "types.h"

typedef struct Instance {
    int32_t num_customers;
    int32_t num_vehicles;
    double vehicle_cap;

    struct {
        Vec2d *positions;
        double *demands;
        double *duals;
    };
} Instance;

typedef struct CostSolution {
    double upper_bound;
    double lower_bound;
} CostSolution;

typedef struct Tour {
    int32_t num_customers;
    int32_t num_vehicles;
    int32_t *num_connected_comps;
    int32_t *succ;
    int32_t *comp;
} Tour;

typedef struct Solution {
    CostSolution cost;
    Tour tour;
} Solution;

void instance_destroy(Instance *instance);

Tour tour_copy(Tour const *other);
Tour tour_move(Tour *other);

void tour_destroy(Tour *tour);

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

#if __cplusplus
}
#endif
