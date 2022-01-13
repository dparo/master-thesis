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

#include "solvers.h"
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
    free(instance->profits);
    free(instance->edge_weight);

    memset(instance, 0, sizeof(*instance));
}

bool tour_is_valid(Tour *tour) {
    return tour->comp && tour->succ && tour->num_customers > 0;
}

void tour_clear(Tour *tour) {
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

    tour_clear(&result);
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

void solution_clear(Solution *solution) {
    solution->lower_bound = INFINITY;
    solution->upper_bound = -INFINITY;
    tour_clear(&solution->tour);
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

const char *param_type_as_str(ParamType type) {
    switch (type) {
    case TYPED_PARAM_DOUBLE:
        return "DOUBLE";
    case TYPED_PARAM_FLOAT:
        return "FLOAT";
    case TYPED_PARAM_BOOL:
        return "BOOL";
    case TYPED_PARAM_INT32:
        return "INT32";
    case TYPED_PARAM_USIZE:
        return "USIZE";
    case TYPED_PARAM_STR:
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
                const char *name = d->params[j].name;
                const char *type = param_type_as_str(d->params[j].type);
                const char *glossary =
                    d->params[j].glossary ? d->params[j].glossary : "";
                const char *default_val = d->params[j].default_value;

                if (default_val != NULL && default_val[0] != '\0') {
                    printf("      - %-20s  (%s, default: %s) %-32s\n", name,
                           type, default_val, glossary);
                } else {
                    printf("      - %-20s  (%s) %-64s\n", name, type, glossary);
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

bool parse_solver_param_val(TypedParam *out, const char *val, ParamType type) {
    TypedParam local = {0};
    if (out == NULL) {
        out = &local;
    }

    bool parse_success = false;

    switch (type) {
    case TYPED_PARAM_STR:
        out->sval = val;
        parse_success = true;
        break;
    case TYPED_PARAM_BOOL:
        parse_success = str_to_bool(val, &out->bval);
        break;
    case TYPED_PARAM_INT32:
        parse_success = str_to_int32(val, &out->ival);
        break;
    case TYPED_PARAM_USIZE:
        parse_success = str_to_usize(val, &out->sizeval);
        break;
    case TYPED_PARAM_DOUBLE:
        parse_success = str_to_double(val, &out->dval);
        break;
    case TYPED_PARAM_FLOAT:
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
            if (0 == strcmp(name1, name2)) {
                log_fatal(
                    "%s :: INTERNAL ERROR! Solver descriptor lists duplicate "
                    "param `%s` (error index i: %d, j: %d",
                    __func__, name1, i, j);
                result = false;
                assert(!"Internal error");
            }
        }
    }

    // Validate that the solver descriptor lists param default values (if any)
    // which
    //   their string representation can be correctly parsed
    for (int32_t i = 0; descriptor->params[i].name != NULL; i++) {
        const char *def = descriptor->params[i].default_value;
        TypedParam t = {0};
        t.type = descriptor->params[i].type;
        if (def && def[0] != 0) {
            bool parse_success =
                parse_solver_param_val(&t, def, descriptor->params[i].type);
            if (!parse_success) {
                log_fatal("%s :: Solver descriptor specifies an invalid "
                          "default value `%s` for parameter `%s`",
                          __func__, def, descriptor->params[i].name);
                result = false;
            }
            assert(parse_success);
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

    return result;
}

void solver_typed_params_destroy(SolverTypedParams *params) {
    if (params->entries) {
        shfree(params->entries);
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
        TypedParam t = {0};
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

        shput(out->entries, desc->params[di].name, t);
    }

terminate:
    if (!result) {
        solver_typed_params_destroy(out);
    }
    return result;
}

static void log_solve_status(SolveStatus status, const char *solver_name) {
    log_info("Solver `%s` returned with solve status: %s", solver_name,
             ENUM_TO_STR(SolveStatus, status));
}

static void postprocess_solver_solution(const Instance *instance,
                                        SolveStatus status,
                                        Solution *solution) {
    switch (status) {
    case SOLVE_STATUS_ERR:
    case SOLVE_STATUS_ABORTED_ERR:
    case SOLVE_STATUS_INVALID:
    case SOLVE_STATUS_ABORTED_INVALID:
        solution_clear(solution);
        if (status == SOLVE_STATUS_INFEASIBLE) {
            solution->upper_bound = INFINITY;
            solution->lower_bound = INFINITY;
        }
        break;

    case SOLVE_STATUS_INFEASIBLE:
        solution_clear(solution);
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
            assert(feq(gap, 0.0, 6.0 / 100));
#endif
        }

        break;
    }
}

typedef void (*sighandler_t)(int);
static THREAD_LOCAL volatile Solver *sighandler_ctx_solver_ptr;

/// \brief Consumes the signal and does not propagate it further
static void cptp_sighandler(int signum) {
    switch (signum) {
    case SIGINT:
        log_warn("Received SIGINT");
        break;
    case SIGTERM:
        log_warn("Received SIGTERM");
        break;
    default:
        break;
    }
    if (signum == SIGTERM || signum == SIGINT) {
        if (sighandler_ctx_solver_ptr) {
            sighandler_ctx_solver_ptr->should_terminate = true;
            sighandler_ctx_solver_ptr->should_terminate_int = 1;
        }
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
        randomseed = (int32_t)(time(NULL) % INT32_MAX);
    }

    printf("%s :: Setting seed = %d\n", __func__, randomseed);
    srand(randomseed);

    printf("%s :: Setting timelimit = %f\n", __func__, timelimit);

    if (!verify_solver_params(lookup->descriptor, params)) {
        fprintf(stderr, "ERROR: %s :: Failed to verify params\n", __func__);
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
        solver.should_terminate = false;
        solver.should_terminate_int = 0;

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
    postprocess_solver_solution(instance, status, solution);
    solver_typed_params_destroy(&tparams);
    return status;

fail:
    solution_clear(solution);
    return status;
}

static inline TypedParam *solver_params_get_key(SolverTypedParams *params,
                                                char *key) {
    SolverTypedParamsEntry *entry = shgetp_null(params->entries, key);
    if (!entry) {
        log_fatal("%s :: Internal error. Key `%s` is invalid. Make sure that "
                  "the SolverDescriptor listed parameters match with the key "
                  "you are trying to unpack.",
                  __func__, key);
        abort();
    }
    return &entry->value;
}

static inline TypedParam *solver_params_get_val(SolverTypedParams *params,
                                                char *key, ParamType type) {
    TypedParam *p = solver_params_get_key(params, key);
    if (p) {
        if (p->count <= 0) {
            log_fatal(
                "Internal error. Tryed to unpack key `%s`, but its count is <= "
                "0     (%d).",
                key, p->count);

            abort();
        } else if (p->type != type) {
            log_fatal("Internal error. Tryed to unpack key `%s` as type `%s`, "
                      "but the actual type is `%s`.",
                      key, param_type_as_str(type), param_type_as_str(p->type));

            abort();
        }
    }

    log_info("Getting parameter `%s` as type `%s`", key,
             param_type_as_str(type));
    return p;
}

bool solver_params_contains(SolverTypedParams *params, char *key) {
    TypedParam *p = solver_params_get_key(params, key);
    return p->count > 0;
}

bool solver_params_get_bool(SolverTypedParams *params, char *key) {
    TypedParam *p = solver_params_get_val(params, key, TYPED_PARAM_BOOL);
    return p->bval;
}

int32_t solver_params_get_int32(SolverTypedParams *params, char *key) {
    TypedParam *p = solver_params_get_val(params, key, TYPED_PARAM_INT32);
    return p->ival;
}

double solver_params_get_double(SolverTypedParams *params, char *key) {
    TypedParam *p = solver_params_get_val(params, key, TYPED_PARAM_DOUBLE);
    return p->dval;
}

Instance instance_copy(const Instance *instance, bool allocate,
                       bool deep_copy) {
    Instance result = {0};
    int32_t n = instance->num_customers + 1;

    result.num_customers = instance->num_customers;
    result.num_vehicles = instance->num_vehicles;
    result.vehicle_cap = instance->vehicle_cap;

    if (allocate) {
        if (instance->edge_weight) {
            result.edge_weight =
                malloc(sizeof(*instance->edge_weight) * hm_nentries(n));
        }

        result.profits = malloc(n * sizeof(*result.profits));
        result.demands = malloc(n * sizeof(*result.demands));

        result.name = strdup(instance->name);
        result.comment = strdup(instance->comment);
    }

    if (deep_copy) {
        memcpy(result.profits, instance->profits, n * sizeof(*result.profits));
        memcpy(result.demands, instance->demands, n * sizeof(*result.demands));
        if (instance->edge_weight) {
            memcpy(result.edge_weight, instance->edge_weight,
                   hm_nentries(n) * sizeof(*result.edge_weight));
        }
    }

    return result;
}

void generate_dual_instance(const Instance *instance, Instance *out,
                            double lagrangian_multiplier_lb,
                            double lagrangian_multiplier_ub) {

    const double u = lagrangian_multiplier_ub;
    const double b = lagrangian_multiplier_lb;

    int32_t n = instance->num_customers + 1;

    if (!out->profits || !out->demands) {
        instance_destroy(out);
        *out = instance_copy(instance, true, false);
    } else {
        *out = instance_copy(instance, false, false);
    }

    out->edge_weight = malloc(sizeof(*out->edge_weight) * hm_nentries(n));

    for (int32_t i = 0; i < n; i++) {
        // NOTE(dparo):
        //     In the dual formulation all profits associated to each
        //     city are cleared, and are instead encoded in the reduced
        //     cost associated to each arc.
        out->profits[i] = 0.0;
        // demands are instead copied as is
        out->demands[i] = instance->demands[i];
    }

    for (int32_t i = 0; i < n; i++) {
        double di = instance->demands[i];
        for (int32_t j = 0; j < n; j++) {
            double dj = instance->demands[j];
            double rc = cptp_reduced_cost(instance, i, j);
            double v = rc + (u - b) * 0.5 * (di + dj);
            out->edge_weight[sxpos(n, i, j)] = v;
        }
    }
}
