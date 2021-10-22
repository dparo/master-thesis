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

#include "validation.h"
#include <signal.h>

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

    if (tour->succ) {
        mati32_set(tour->succ, tour->num_customers + 1, tour->num_vehicles,
                   INT32_DEAD_VAL);
    }

    if (tour->comp) {
        mati32_set(tour->comp, tour->num_customers + 1, tour->num_vehicles,
                   INT32_DEAD_VAL);
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

    result.num_comps = veci32_copy(other->num_comps, result.num_vehicles);
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

typedef Solver (*SolverCreateFn)(const Instance *instance, double timelimit,
                                 int32_t randomseed);

static const struct SolverLookup {
    const SolverDescriptor *descriptor;
    SolverCreateFn create_fn;
} SOLVERS_LOOKUP_TABLE[] = {
    {&STUB_SOLVER_DESCRIPTOR, &stub_solver_create},
    {&MIP_SOLVER_DESCRIPTOR, &mip_solver_create},
};

const char *param_type_as_str(SolverParamType type) {
    switch (type) {
    case SOLVER_TYPED_PARAM_DOUBLE:
        return "DOUBLE";
    case SOLVER_TYPED_PARAM_FLOAT:
        return "FLOAT";
    case SOLVER_TYPED_PARAM_BOOL:
        return "BOOL";
    case SOLVER_TYPED_PARAM_INT32:
        return "INT32";
    case SOLVER_TYPED_PARAM_USIZE:
        return "USIZE";
    case SOLVER_TYPED_PARAM_STR:
        return "STR";
    }

    assert(!"Invalid code path");
    return "<UNKNOWN>";
}

void cptp_print_list_of_solvers_and_params(void) {
    for (int32_t solver_idx = 0;
         solver_idx < (int32_t)ARRAY_LEN(SOLVERS_LOOKUP_TABLE); solver_idx++) {
        const SolverDescriptor *d = SOLVERS_LOOKUP_TABLE[solver_idx].descriptor;
        const char *solver_name = d->name;
        if (solver_name != NULL && *solver_name != '\0') {
            printf("%s\n", solver_name);
            for (int32_t j = 0; d->params[j].name != NULL; j++) {
                if (d->params[j].default_value != NULL &&
                    *d->params[j].default_value != '\0') {
                    printf("   %-20s  (%s, default: %s)\n", d->params[j].name,
                           param_type_as_str(d->params[j].type),
                           d->params->default_value);
                } else {
                    printf("   %-20s  (%s)\n", d->params[j].name,
                           param_type_as_str(d->params[j].type));
                }
                printf("%32s\n", d->params->glossary);
            }
        }
    }
}

typedef struct SolverLookup SolverLookup;

static const SolverLookup *lookup_solver(const char *solver_name) {

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
    bool result = true;
    assert(params->num_params <= MAX_NUM_SOLVER_PARAMS);

#ifndef NDEBUG
    // Validate that the solver descriptor lists param names which are unique
    for (int32_t i = 0; descriptor->params[i].name != NULL; i++) {
        const char *name1 = descriptor->params[i].name;

        for (int32_t j = 0; descriptor->params[j].name != NULL; j++) {
            if (i == j) {
                continue;
            }
            const char *name2 = descriptor->params[j].name;
            log_fatal("%s :: Solver desciptor lists duplicate param `%s`",
                      __func__, name1);
            assert(strcmp(name1, name2) != 0);
        }
    }
#endif

    // Check that user supplied params are all listed in the descriptor
    for (int32_t i = 0; i < MIN(MAX_NUM_SOLVER_PARAMS, params->num_params);
         i++) {
        const char *user_param_name = params->params[i].name;
        bool found_match = false;
        for (int32_t j = 0; descriptor->params[j].name != NULL; j++) {
            const char *descr_param_name = descriptor->params[i].name;
            if (strcmp(user_param_name, descr_param_name) == 0) {
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            log_fatal("%s :: Solver `%s` does not accept param `%s`", __func__,
                      descriptor->name, user_param_name);
            result = false;
        }
    }

    return result;
}

static void log_solve_status(SolveStatus status, const char *solver_name) {
    static ENUM_TO_STR_TABLE_DECL(SolveStatus) = {
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_ERR),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_ABORTED_ERR),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_INVALID),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_ABORTED_INVALID),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_INFEASIBLE),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_FEASIBLE),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_ABORTED_FEASIBLE),
        ENUM_TO_STR_TABLE_FIELD(SOLVE_STATUS_OPTIMAL),
    };

    log_info("Solver `%s` returned with solve status: %s", solver_name,
             ENUM_TO_STR(SolveStatus, status));
}

static void postprocess_solver_solution(const Instance *instance,
                                        SolveStatus status, Solution *solution,
                                        const char *solver_name) {
    switch (status) {
    case SOLVE_STATUS_ERR:
    case SOLVE_STATUS_ABORTED_ERR:
    case SOLVE_STATUS_INVALID:
    case SOLVE_STATUS_ABORTED_INVALID:
        solution_invalidate(solution);
        if (status == SOLVE_STATUS_INFEASIBLE) {
            solution->upper_bound = INFINITY;
            solution->lower_bound = INFINITY;
        }
        break;

    case SOLVE_STATUS_INFEASIBLE:
        solution_invalidate(solution);
        solution->upper_bound = INFINITY;
        solution->lower_bound = INFINITY;
        break;

    case SOLVE_STATUS_ABORTED_FEASIBLE:
    case SOLVE_STATUS_FEASIBLE:
    case SOLVE_STATUS_OPTIMAL:
        validate_solution(instance, solution);

        if (status == SOLVE_STATUS_OPTIMAL) {
#ifndef NDEBUG
            // If solution is optimal it should remain within a 6% optimal
            // gap
            double gap = solution_relgap(solution);
            assert(fcmp(gap, 0.0, 6.0 / 100));
#endif
        }

        break;
    }
}

static Solver *sighandler_ctx_solver_ptr;

void sighandler(int signum) {
    switch (signum) {
    case SIGTERM:
        log_warn("Received SIGINT");
        break;
    case SIGINT:
        log_warn("Received SIGTERM");
        break;
    default:
        break;
    }
    if (signum == SIGTERM || signum == SIGINT) {
        sighandler_ctx_solver_ptr->should_terminate = true;
    }
}

SolveStatus cptp_solve(const Instance *instance, const char *solver_name,
                       const SolverParams *params, Solution *solution,
                       double timelimit, int32_t randomseed) {
    SolveStatus status = SOLVE_STATUS_INVALID;
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

    if (randomseed == 0) {
        srand(time(NULL));
    } else {
        srand(randomseed);
    }

    Solver solver = lookup->create_fn(instance, timelimit, randomseed);
    sighandler_ctx_solver_ptr = &solver;

    {
        // Setup signals
        signal(SIGTERM, sighandler);
        signal(SIGINT, sighandler);
        usecs_t begin_time = os_get_usecs();
        status = solver.solve(&solver, instance, solution, begin_time);
        // Resets the signals
        signal(SIGTERM, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        sighandler_ctx_solver_ptr = NULL;
    }

    if (solver.should_terminate) {
        switch (status) {
        case SOLVE_STATUS_ERR:
            status = SOLVE_STATUS_ABORTED_ERR;
            break;
        case SOLVE_STATUS_INVALID:
            status = SOLVE_STATUS_ABORTED_INVALID;
            break;
        case SOLVE_STATUS_FEASIBLE:
            status = SOLVE_STATUS_ABORTED_FEASIBLE;
        default:
            break;
        }
    }

    solver.destroy(&solver);
    log_solve_status(status, solver_name);
    postprocess_solver_solution(instance, status, solution, solver_name);
    return status;

fail:
    solution_invalidate(solution);
    return status;
}
