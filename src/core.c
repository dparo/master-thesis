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

#include "core.h"
#include <stdlib.h>
#include <string.h>

#include "solvers/stub.h"
#include "solvers/mip.h"
#include "core-utils.h"

void instance_set_name(Instance *instance, const char *name) {
    if (instance->name) {
        free(instance->name);
    }
    instance->name = strdup(name);
}

void instance_destroy(Instance *instance) {
    if (instance->name) {
        free(instance->name);
    }

    free(instance->positions);
    free(instance->demands);
    free(instance->duals);

    memset(instance, 0, sizeof(*instance));
}

void tour_invalidate(Tour *tour) {

    if (tour->num_comps) {
        veci32_set(tour->num_comps, tour->num_vehicles, 0);
    }

    const int32_t DEAD_VAL = INT32_MIN >> 1;

    if (tour->succ) {
        mati32_set(tour->succ, tour->num_customers + 1, tour->num_vehicles,
                   DEAD_VAL);
    }

    if (tour->comp) {
        mati32_set(tour->comp, tour->num_customers + 1, tour->num_vehicles,
                   DEAD_VAL);
    }
}

Tour tour_create(const Instance *instance) {
    Tour result = {0};
    result.num_customers = instance->num_customers;
    result.num_vehicles = instance->num_vehicles;

    result.num_comps = veci32_create(instance->num_vehicles);
    result.succ =
        mati32_create(instance->num_customers + 1, instance->num_vehicles);
    result.comp =
        mati32_create(instance->num_customers + 1, instance->num_vehicles);

    tour_invalidate(&result);
    return result;
}

void tour_destroy(Tour *tour) {
    free(tour->num_comps);
    free(tour->succ);
    free(tour->comp);
    memset(tour, 0, sizeof(*tour));
}

Solution solution_create(const Instance *instance) {
    Solution solution = {0};
    solution.upper_bound = INFINITY;
    solution.lower_bound = -INFINITY;
    solution.tour = tour_create(instance);
    return solution;
}

void solution_invalidate(Solution *solution) {
    solution->lower_bound = INFINITY;
    solution->upper_bound = -INFINITY;
    tour_invalidate(&solution->tour);
}

void solution_destroy(Solution *solution) {
    tour_destroy(&solution->tour);
    memset(solution, 0, sizeof(*solution));
}

Tour tour_copy(Tour const *other) {
    Tour result = {0};
    result.num_customers = other->num_customers;
    result.num_vehicles = other->num_vehicles;

    result.num_comps =
        veci32_copy(other->num_comps, result.num_vehicles);
    result.succ =
        mati32_copy(other->succ, result.num_customers + 1, result.num_vehicles);
    result.comp =
        mati32_copy(other->comp, result.num_customers + 1, result.num_vehicles);
    return result;
}

Tour tour_move(Tour *other) {
    Tour result = {0};
    memcpy(&result, other, sizeof(result));
    memset(other, 0, sizeof(*other));
    return result;
}

typedef Solver (*SolverCreateFn)(const Instance *instance);

static const struct SolverLookup {
    const SolverDescriptor *descriptor;
    SolverCreateFn create_fn;
} SOLVERS_LOOKUP_TABLE[] = {
    {&STUB_SOLVER_DESCRIPTOR, &stub_solver_create},
    {&MIP_SOLVER_DESCRIPTOR, &mip_solver_create},
};

typedef struct SolverLookup SolverLookup;

static const SolverLookup *lookup_solver(char *solver_name) {

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(SOLVERS_LOOKUP_TABLE); i++) {
        if (0 ==
            strcmp(solver_name, SOLVERS_LOOKUP_TABLE[i].descriptor->name)) {
            return &SOLVERS_LOOKUP_TABLE[i];
        }
    }

    return NULL;
}

static bool verify_solver_params(const SolverDescriptor *descriptor,
                                 const SolverParams *params) {
    log_warn("TODO!");
    return true;
}

static void log_solve_status(SolveStatus status, char *solver_name) {
    static ENUM_TO_STR_TABLE_DECL(SolveStatus) = {
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_ERR),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_UNFEASIBLE),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_INVALID),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_FEASIBLE),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_OPTIMAL),
    };

    log_info("Solver `%s` returned with solve status: %s", solver_name,
             ENUM_TO_STR(SolveStatus, status));
}

static void postprocess_solver_solution(SolveStatus status, Solution *solution,
                                        char *solver_name) {
    switch (status) {
    case SOLVE_STATUS_ERR:
    case SOLVE_STATUS_UNFEASIBLE:
    case SOLVE_STATUS_INVALID:
        solution_invalidate(solution);
        if (status == SOLVE_STATUS_UNFEASIBLE) {
            solution->upper_bound = INFINITY;
            solution->lower_bound = INFINITY;
        }
        break;
    case SOLVE_STATUS_FEASIBLE:
    case SOLVE_STATUS_OPTIMAL:
        todo();
        // TODO:
        //       1. Validate the tour edges and connectivity
        //       2. Validate that the upper bound populated from the solver is
        //          equal to the `tour_eval()` function that recomputes the
        //          upperbound from the tour
        //       3. In case the solver returns an optimal solution, verify
        //          that the upper_bound and lower_bound stays within a
        //          reasonable gap

        if (status == SOLVE_STATUS_OPTIMAL) {
            todo_msg("upper_bound and lower_bound gap check");
        }

        break;
    }

    // TODO: Check solution, print some stuff, validate the solution...
}

Solution cptp_solve(Instance *instance, char *solver_name,
                    const SolverParams *params) {
    Solution solution = solution_create(instance);
    const SolverLookup *lookup = lookup_solver(solver_name);

    if (lookup == NULL) {
        log_fatal("%s :: `%s` is not a know solver", __func__, solver_name);
        goto fail;
    } else {
        log_info("%s :: Found descriptor for solver `%s`", __func__,
                 solver_name);
    }

    if (!verify_solver_params(lookup->descriptor, params)) {
        log_fatal("%s :: Failed to veriy params", __func__);
        goto fail;
    }

    Solver solver = lookup->create_fn(instance);
    SolveStatus solve_status = solver.solve(&solver, instance, &solution);
    solver.destroy(&solver);
    log_solve_status(solve_status, solver_name);
    postprocess_solver_solution(solve_status, &solution, solver_name);
    return solution;

fail:
    solution_invalidate(&solution);
    return solution;
}
