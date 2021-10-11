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
#include "utils.h"

typedef struct Instance {
    char *name;

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

typedef struct SolverData SolverData;

typedef struct SolverParams {
    // TODO :Implement me
    int32_t ___dummy___;
} SolverParams;

typedef struct Solver {
    SolverData *data;

    // TODO: set_params
    bool (*set_params)(struct Solver *self, const SolverParams *params);
    Solution (*solve)(struct Solver *self, const Instance *instance);
    void (*destroy)(struct Solver *self);
} Solver;

void instance_set_name(Instance *instance, const char *name);
void instance_destroy(Instance *instance);

Tour tour_copy(Tour const *other);
Tour tour_move(Tour *other);

void tour_destroy(Tour *tour);

#if __cplusplus
}
#endif
