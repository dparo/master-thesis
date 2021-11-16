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
#include <signal.h>

#include "solvers/stub.h"
#include "solvers/mip.h"
#include "core-utils.h"
#include "parsing-utils.h"
#include "validation.h"

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
    if (instance->comment) {
        free(instance->comment);
    }

    free(instance->positions);
    free(instance->demands);
    free(instance->duals);
    free(instance->edge_weight);

    memset(instance, 0, sizeof(*instance));
}

void tour_invalidate(Tour *tour) {
    tour->num_comps = 0;

    if (tour->succ) {
        veci32_set(tour->succ, tour->num_customers + 1, INT32_DEAD_VAL);
    }

    if (tour->comp) {
        veci32_set(tour->comp, tour->num_customers + 1, INT32_DEAD_VAL);
    }
}

Tour tour_create(const Instance *instance) {
    Tour result = {0};
    result.num_customers = instance->num_customers;

    result.num_comps = 0;
    result.succ = veci32_create(instance->num_customers + 1);
    result.comp = veci32_create(instance->num_customers + 1);

    tour_invalidate(&result);
    return result;
}

void tour_destroy(Tour *tour) {
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

    result.num_comps = other->num_comps;
    result.succ = veci32_copy(other->succ, result.num_customers + 1);
    result.comp = veci32_copy(other->comp, result.num_customers + 1);
    return result;
}

Tour tour_move(Tour *other) {
    Tour result = {0};
    memcpy(&result, other, sizeof(result));
    memset(other, 0, sizeof(*other));
    return result;
}

typedef Solver (*SolverCreateFn)(const Instance *instance,
                                 SolverTypedParams *tparams, double timelimit,
                                 int32_t randomseed);

static const struct SolverLookup {
    const SolverDescriptor *descriptor;
    SolverCreateFn create_fn;
} SOLVERS_REGISTRY[] = {
    {&STUB_SOLVER_DESCRIPTOR, &stub_solver_create},
#if COMPILED_WITH_CPLEX
    {&MIP_SOLVER_DESCRIPTOR, &mip_solver_create},
#endif
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

    printf("\n");
    printf("Available Solvers and Settable Params:\n");
    for (int32_t solver_idx = 0;
         solver_idx < (int32_t)ARRAY_LEN(SOLVERS_REGISTRY); solver_idx++) {
        const SolverDescriptor *d = SOLVERS_REGISTRY[solver_idx].descriptor;
        if (d->name != NULL && *d->name != '\0') {
            printf("  - %s:\n", d->name);
            int32_t j;
            for (j = 0; d->params[j].name != NULL; j++) {
                if (d->params[j].default_value != NULL &&
                    *d->params[j].default_value != '\0') {
                    printf("      - %-20s  (%s, default: %s)",
                           d->params[j].name,
                           param_type_as_str(d->params[j].type),
                           d->params->default_value);
                } else {
                    printf("      - %-20s  (%s)", d->params[j].name,
                           param_type_as_str(d->params[j].type));
                }

                if (d->params[j].glossary != NULL &&
                    *d->params[j].glossary != '\0') {
                    printf("  %s\n", d->params[j].glossary);
                } else {
                    printf("\n");
                }
            }
            if (j == 0) {
                printf("      <NO PARAMS AVAILABLE>");
            }
            printf("\n");
        }
    }
}

typedef struct SolverLookup SolverLookup;

static const SolverLookup *lookup_solver(const char *solver_name) {

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(SOLVERS_REGISTRY); i++) {
        if (0 == strcmp(solver_name, SOLVERS_REGISTRY[i].descriptor->name)) {
            return &SOLVERS_REGISTRY[i];
        }
    }

    return NULL;
}

bool parse_solver_param_val(SolverTypedParam *out, const char *val,
                            SolverParamType type) {
    SolverTypedParam local = {0};
    if (out == NULL) {
        out = &local;
    }

    bool parse_success = false;

    switch (type) {
    case SOLVER_TYPED_PARAM_STR:
        out->sval = val;
        parse_success = true;
        break;
    case SOLVER_TYPED_PARAM_BOOL:
        parse_success = str_to_bool(val, &out->bval);
        break;
    case SOLVER_TYPED_PARAM_INT32:
        parse_success = str_to_int32(val, &out->ival);
        break;
    case SOLVER_TYPED_PARAM_USIZE:
        parse_success = str_to_usize(val, &out->sizeval);
        break;
    case SOLVER_TYPED_PARAM_DOUBLE:
        parse_success = str_to_double(val, &out->dval);
        break;
    case SOLVER_TYPED_PARAM_FLOAT:
        parse_success = str_to_float(val, &out->fval);
        break;
    }

    return parse_success;
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
            const char *descr_param_name = descriptor->params[j].name;
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

    // FIXME:
    // Should we bother about this? Or is it overengineering/complication for
    // little value?
    //   - Check that the descriptor default value (if any) can be parsed.
    // Note that the default value is constantly and statically known and
    // determined, and cannot change... So shall we bother...? This would allows
    // to achieve: mistakes are catched earliar in one centrailzed place and
    // across all params, a wrong default value is catched here, even if is
    // unused. Otherwise we would catch the error later, and only if we try to
    // unpack the default value (lazy evaluation)

    return result;
}

void solver_typed_params_destroy(SolverTypedParams *params) {
    if (params->__sm) {
        shfree(params->__sm);
    }
    memset(params, 0, sizeof(*params));
}

bool resolve_params(const SolverParams *params, const SolverDescriptor *desc,
                    SolverTypedParams *out) {
    solver_typed_params_destroy(out);
    bool result = true;

    for (int32_t di = 0; desc->params[di].name != 0; di++) {
        const char *value = NULL;
        for (int32_t pi = 0;
             pi < MIN(MAX_NUM_SOLVER_PARAMS, params->num_params); pi++) {
            if (0 == strcmp(params->params[pi].name, desc->params[di].name)) {
                if (value) {
                    fprintf(stderr,
                            "ERROR: parameter `%s` specified twice or more.\n",
                            params->params[pi].name);
                    result = false;
                    goto terminate;
                }
                value = params->params[pi].value;
            }
        }

        // If the user didn't specify any value get the default value from the
        // descriptor
        if (!value) {
            value = desc->params[di].default_value;
        }

        // NOTE:
        // The descriptor may not contain a default_value: therefore
        // `value` may still be NULL
        SolverTypedParam t = {0};
        t.type = desc->params[di].type;

        if (!value) {
            t.count = 0;
        } else {
            t.count = 1;

            fprintf(stderr, "%s :: Setting `%s` (%s) to value `%s`\n", __func__,
                    desc->params[di].name,
                    param_type_as_str(desc->params[di].type), value);

            if (!parse_solver_param_val(&t, value, desc->params[di].type)) {
                fprintf(
                    stderr,
                    "ERROR: Failed to parse param `%s=%s` required as a %s\n",
                    desc->params[di].name, value,
                    param_type_as_str(desc->params[di].type));
                result = false;
                goto terminate;
            }
        }

        shput(out->__sm, desc->params[di].name, t);
    }

terminate:
    if (!result) {
        solver_typed_params_destroy(out);
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

typedef void (*sighandler_t)(int);
static THREAD_LOCAL Solver *sighandler_ctx_solver_ptr;

/// \brief Consumes the signal and does not propage it further
static void cptp_sighandler(int signum) {
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

    if (randomseed == 0) {
        srand(time(NULL));
    } else {
        srand(randomseed);
    }

    if (!verify_solver_params(lookup->descriptor, params)) {
        fprintf(stderr, "ERROR: %s :: Failed to verify params", __func__);
        goto fail;
    }

    SolverTypedParams tparams = {0};

    if (!resolve_params(params, lookup->descriptor, &tparams)) {
        fprintf(stderr, "ERROR: %s :: Failed to resolve parameters\n",
                __func__);
        goto fail;
    }

    Solver solver =
        lookup->create_fn(instance, &tparams, timelimit, randomseed);

    {
        // Setup signals
        sighandler_ctx_solver_ptr = &solver;
        sighandler_t prev_sigterm_handler = signal(SIGTERM, cptp_sighandler);
        sighandler_t prev_sigint_handler = signal(SIGINT, cptp_sighandler);
        {
            int64_t begin_time = os_get_usecs();
            status = solver.solve(&solver, instance, solution, begin_time);
        }
        // Resets the signals
        signal(SIGTERM, prev_sigterm_handler);
        signal(SIGINT, prev_sigint_handler);
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
    solver_typed_params_destroy(&tparams);
    return status;

fail:
    solution_invalidate(solution);
    return status;
}
