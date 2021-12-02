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
#include "os.h"
#include <stb_ds.h>

#define INT32_DEAD_VAL (INT32_MIN >> 1)

typedef enum DistanceRounding {
    CPTP_DIST_ROUND = 0, /// default
    CPTP_DIST_NO_ROUND = 1,
    CPTP_DIST_CEIL = 2,
    CPTP_DIST_FLOOR = 3,
} DistanceRounding;

#define DEPOT_NODE_ID 0

typedef struct Instance {
    char *name;
    char *comment;

    int32_t num_customers;
    int32_t num_vehicles;
    double vehicle_cap;
    double zero_reduced_cost_threshold;

    DistanceRounding rounding_strat;
    Vec2d *positions;
    double *demands;
    double *duals;
    double *edge_weight;
} Instance;

typedef struct Tour {
    int32_t num_customers;
    int32_t num_comps;
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

typedef struct {
    int32_t num_params;
    struct {
        const char *name;
        const char *value;
    } params[MAX_NUM_SOLVER_PARAMS];
} SolverParams;

typedef struct SolverTypedParams {
    // NOTE: Hashtable use <stb_ds.h> : shput(), shget() and alike
    struct {
        char *key;
        TypedParam value;
    } * __sm;
} SolverTypedParams;

typedef struct SolverDescriptor {
    const char *name;
    struct {
        const char *name;
        const ParamType type;
        const char *default_value;
        const char *glossary;
    } const params[];
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
                         Solution *solution, int64_t begin_time);
    void (*destroy)(struct Solver *self);
} Solver;

void instance_set_name(Instance *instance, const char *name);
void instance_destroy(Instance *instance);

Tour tour_create(const Instance *instance);
void tour_destroy(Tour *tour);
bool tour_is_valid(Tour *tour);
Tour tour_copy(Tour const *other);
Tour tour_move(Tour *other);

Solution solution_create(const Instance *instance);
void solution_destroy(Solution *solution);
void solution_invalidate(Solution *solution);

void solver_typed_params_destroy(SolverTypedParams *params);
bool resolve_params(const SolverParams *params, const SolverDescriptor *desc,
                    SolverTypedParams *out);

SolveStatus cptp_solve(const Instance *instance, const char *solver_name,
                       const SolverParams *params, Solution *solution,
                       double timelimit, int32_t randomseed);

void cptp_print_list_of_solvers_and_params(void);

static inline bool is_valid_instance(Instance *instance) {
    bool invalid = instance->num_customers <= 0 ||
                   instance->num_vehicles <= 0 || instance->vehicle_cap <= 0 ||
                   !instance->demands;
    return !invalid;
}

static inline bool is_valid_solve_status(SolveStatus status) {
    return status == SOLVE_STATUS_FEASIBLE ||
           status == SOLVE_STATUS_ABORTED_FEASIBLE ||
           status == SOLVE_STATUS_OPTIMAL;
}

#if __cplusplus
}
#endif
