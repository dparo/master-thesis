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

Tour tour_create(const Instance *instance) {
    Tour result = {0};
    result.num_customers = instance->num_customers;
    result.num_vehicles = instance->num_vehicles;
    result.num_connected_comps = 0;
    result.succ =
        mati32_create(instance->num_customers + 1, instance->num_vehicles);
    result.comp =
        mati32_create(instance->num_customers + 1, instance->num_vehicles);

    if (result.succ) {
        mati32_set(result.succ, instance->num_customers + 1,
                   instance->num_vehicles, -1);
    }

    if (result.comp) {
        mati32_set(result.comp, instance->num_customers + 1,
                   instance->num_vehicles, -1);
    }

    return result;
}

void tour_destroy(Tour *tour) {
    free(tour->num_connected_comps);
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

void solution_destroy(Solution *solution) {
    tour_destroy(&solution->tour);
    memset(solution, 0, sizeof(*solution));
}

Tour tour_copy(Tour const *other) {
    Tour result = {0};
    result.num_customers = other->num_customers;
    result.num_vehicles = other->num_vehicles;

    result.num_connected_comps =
        veci32_copy(other->num_connected_comps, result.num_vehicles);
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

typedef Solver (*SolverCreateFn)(Instance *instance);

static const struct SolverLookup {
    const SolverDescriptor *descriptor;
    SolverCreateFn create_fn;
} SOLVERS_LOOKUP_TABLE[] = {
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
    assert(!"TODO");
    return true;
}

Solution cptp_solve(Instance *instance, char *solver_name,
                    const SolverParams *params) {
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

    Solution solution = solution_create(instance);

    Solver solver = lookup->create_fn(instance);
    SolveStatus solve_status = solver.solve(&solver, instance, &solution);
    solver.destroy(&solver);

    switch (solve_status) {
    case SOLVE_STATUS_ERR:
        todo();
        break;
    case SOLVE_STATUS_INVALID:
        todo();
        break;
    case SOLVE_STATUS_OK:
        todo();
        break;
    case SOLVE_STATUS_EXACT:
        todo();
        break;
    }
    // TODO: Check solution quality, print some stuff, validate the solution
    // etc...

    return solution;

fail:
    return (Solution){0};
}
