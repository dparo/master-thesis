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

#define INT32_DEAD_VAL (INT32_MIN >> 1)

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

typedef struct Tour {
    int32_t num_customers;
    int32_t num_vehicles;
    int32_t *num_comps;
    int32_t *succ;
    int32_t *comp;
} Tour;

typedef struct Solution {
    double upper_bound;
    double lower_bound;
    Tour tour;
} Solution;

typedef struct SolverData SolverData;

#define MAX_NUM_SOLVER_PARAMS (256)

typedef struct SolverParams {
    int32_t num_params;
    struct {
        char *name;
        char *value;
    } params[MAX_NUM_SOLVER_PARAMS];
} SolverParams;

typedef struct SolverDescriptor {
    char *name;
    struct {
        bool required;
        char *name;
        char *type;
        char *default_value;
    } params[];
} SolverDescriptor;

typedef enum SolveStatus {
    SOLVE_STATUS_ERR = -127,
    SOLVE_STATUS_ABORTED_ERR = -63,
    // Greater or equal than 0
    SOLVE_STATUS_INVALID = 0,
    SOLVE_STATUS_ABORTED_INVALID,
    SOLVE_STATUS_INFEASIBLE,
    SOLVE_STATUS_FEASIBLE,
    SOLVE_STATUS_ABORTED_FEASIBLE,
    SOLVE_STATUS_OPTIMAL,
} SolveStatus;

typedef struct Solver {
    SolverData *data;
    bool should_terminate;

    // TODO: set_params
    bool (*set_params)(struct Solver *self, const SolverParams *params);
    SolveStatus (*solve)(struct Solver *self, const Instance *instance,
                         Solution *solution);
    void (*destroy)(struct Solver *self);
} Solver;

void instance_set_name(Instance *instance, const char *name);
void instance_destroy(Instance *instance);

Tour tour_create(const Instance *instance);
void tour_destroy(Tour *tour);
void tour_invalidate(Tour *tour);
Tour tour_copy(Tour const *other);
Tour tour_move(Tour *other);

Solution solution_create(const Instance *instance);
void solution_destroy(Solution *solution);
void solution_invalidate(Solution *solution);

SolveStatus cptp_solve(const Instance *instance, const char *solver_name,
                       const SolverParams *params, Solution *solution);


static inline cptp_solve_found_tour_solution(SolveStatus status)
{
    return status == SOLVE_STATUS_FEASIBLE || status == SOLVE_STATUS_ABORTED_FEASIBLE || status == SOLVE_STATUS_OPTIMAL;
}

#if __cplusplus
}
#endif
