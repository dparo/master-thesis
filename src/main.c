// Copyright (c) 2021 Davide Paro
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "misc.h"
#include "core-utils.h"
#include "core.h"
#include "os.h"
#include "parser.h"
#include "types.h"
#include "version.h"
#include "render.h"

#include <argtable3.h>
#include <cJSON.h>
#include <log.h>
#include <sha256.h>

#define DEFAULT_TIME_LIMIT ((double)600.0) // 10 minutes

static void print_brief_description(const char *progname);
static void print_version(void);
static void print_use_help_for_more_information(const char *progname);
static void print_tour(Tour *t);

static inline SolverParams
make_solver_params_from_cmdline(const char **defines, int32_t num_defines) {
    SolverParams result = {0};
    if (num_defines > MAX_NUM_SOLVER_PARAMS) {
        fprintf(stderr,
                "Too many parameters definitions, %d max, got %d instead",
                MAX_NUM_SOLVER_PARAMS, num_defines);
        fflush(stderr);
        abort();
    }

    for (int32_t i = 0; i < MIN(MAX_NUM_SOLVER_PARAMS, num_defines); i++) {
        const char *name = defines[i];
        const char *value = NULL;
        char *equal = strchr(defines[i], '=');
        if (equal) {
            *equal = 0;
            value = equal + 1;
        } else {
            value = defines[i] + strlen(defines[i]);
        }

        result.params[i].name = name;
        result.params[i].value = value;
        result.num_params++;
    }

    return result;
}

typedef struct {
    int32_t loglvl;
    bool treat_sigterm_as_failure;
    const char *instance_filepath;
    const char *solver;
    double timelimit;
    int32_t randomseed;
    const char **defines;
    int32_t num_defines;
    const char *vis_path;
    const char *json_report_path;
} AppCtx;

typedef struct {
    time_t started;
    time_t ended;
    int64_t took_usecs;
} Timing;

static void writeout_results(FILE *fh, AppCtx *ctx, bool success,
                             Instance *instance, Solution *solution,
                             SolveStatus status, Timing timing) {
    bool primal_sol_avail = BOOL(status & SOLVE_STATUS_PRIMAL_SOLUTION_AVAIL);
    bool valid = status != 0 && !BOOL(status & SOLVE_STATUS_ERR);

    fprintf(fh, "%-16s %s\n", "SOLVER:", ctx->solver);
    fprintf(fh, "%-16s %f\n", "TIMELIM:", ctx->timelimit);
    fprintf(fh, "%-16s %d\n", "SEED:", ctx->randomseed);
    fprintf(fh, "%-16s %s\n", "INPUT:", ctx->instance_filepath);
    fprintf(fh, "%-16s %.17g\n", "VEHICLE_CAP:", instance->vehicle_cap);

    fprintf(fh, "%-16s %x\n", "STATUS:", status);

    if (valid) {
        printf("%-16s [%.17g, %.17g]\n", "BOUNDS:", solution->dual_bound,
               solution->primal_bound);
        printf("%-16s %.17g\n", "GAP", solution_relgap(solution));
        if (primal_sol_avail) {
            print_tour(&solution->tour);
        }
    } else if (!valid) {
        printf("%-16s Could not solve\n", "ERR:");
    }

    if (primal_sol_avail && valid) {
        double cost = tour_eval(instance, &solution->tour);
        double demand = tour_demand(instance, &solution->tour);
        double profit = tour_profit(instance, &solution->tour);
        printf("%-16s %.17g\n", "TOUR COST:", cost);
        printf("%-16s %.17g\n", "TOUR PROFIT:", profit);
        printf("%-16s %.17g   (%.3g%%)\n", "TOUR DEMAND:", demand,
               demand / instance->vehicle_cap * 100.0);
    }

    printf("%-16s %s", "STARTED:", ctime(&timing.started));
    printf("%-16s %s", "ENDED:", ctime(&timing.ended));
    printf("%-16s ", "TOOK:");

    TimeRepr solve_time_repr = timerepr_from_usecs(timing.took_usecs);
    print_timerepr(stdout, &solve_time_repr);
    printf("\n");

    printf("%-16s %s\n", "SUCCESS", success ? "TRUE" : "FALSE");
}

static void writeout_json_report(AppCtx *ctx, Instance *instance,
                                 Solution *solution, SolveStatus status,
                                 Timing timing) {
    if (!ctx->json_report_path) {
        return;
    }

    cJSON *root = NULL;
    FILE *fh = fopen(ctx->json_report_path, "w");
    if (!fh) {
        log_fatal("%s: failed to open file for writing JSON report",
                  ctx->json_report_path);
        goto cleanup;
    }

    root = cJSON_CreateObject();
    if (!root) {
        log_fatal("%s :: Failed to create JSON root object", __func__);
        goto cleanup;
    }

    bool primal_sol_avail = BOOL(status & SOLVE_STATUS_PRIMAL_SOLUTION_AVAIL);
    bool valid = status != 0 && !BOOL(status & SOLVE_STATUS_ERR);
    bool sigterm_abortion = BOOL(status & SOLVE_STATUS_ABORTION_SIGTERM);
    bool res_exhaustion_abortion =
        BOOL(status & SOLVE_STATUS_ABORTION_RES_EXHAUSTED);
    bool closed_problem = BOOL(status & SOLVE_STATUS_CLOSED_PROBLEM);

    bool s = true;
    s &= cJSON_AddItemToObject(root, "solverName",
                               cJSON_CreateString(ctx->solver));
    s &= cJSON_AddItemToObject(root, "timeLimit",
                               cJSON_CreateNumber(ctx->timelimit));
    s &= cJSON_AddItemToObject(root, "randomSeed",
                               cJSON_CreateNumber(ctx->randomseed));
    s &= cJSON_AddItemToObject(
        root, "cmdLineDefines",
        cJSON_CreateStringArray(ctx->defines, ctx->num_defines));
    s &= cJSON_AddItemToObject(root, "inputFile",
                               cJSON_CreateString(ctx->instance_filepath));

    cJSON *instance_info_obj = cJSON_CreateObject();
    s &= cJSON_AddItemToObject(root, "instanceInfo", instance_info_obj);
    {
        s &= cJSON_AddItemToObject(
            instance_info_obj, "name",
            cJSON_CreateString(instance->name ? instance->name : ""));
        s &= cJSON_AddItemToObject(
            instance_info_obj, "comment",
            cJSON_CreateString(instance->comment ? instance->comment : ""));
        s &= cJSON_AddItemToObject(instance_info_obj, "vehicleCap",
                                   cJSON_CreateNumber(instance->vehicle_cap));
        s &= cJSON_AddItemToObject(instance_info_obj, "numCustomers",
                                   cJSON_CreateNumber(instance->num_customers));
        s &= cJSON_AddItemToObject(instance_info_obj, "numVehicles",
                                   cJSON_CreateNumber(instance->num_vehicles));
    }

    cJSON *status_obj = cJSON_CreateObject();
    s &= cJSON_AddItemToObject(root, "solveStatus", status_obj);
    {
        s &= cJSON_AddItemToObject(status_obj, "code",
                                   cJSON_CreateNumber(status));

        s &= cJSON_AddItemToObject(
            status_obj, "erroredOut",
            cJSON_CreateBool(BOOL(status & SOLVE_STATUS_ERR)));

        s &= cJSON_AddItemToObject(status_obj, "containsPrimalSolution",
                                   cJSON_CreateBool(primal_sol_avail));

        s &= cJSON_AddItemToObject(status_obj, "closedProblem",
                                   cJSON_CreateBool(closed_problem));

        s &= cJSON_AddItemToObject(status_obj, "resExhaustionAbortion",
                                   cJSON_CreateBool(res_exhaustion_abortion));

        s &= cJSON_AddItemToObject(status_obj, "sigTermAbortion",
                                   cJSON_CreateBool(sigterm_abortion));
    }

    cJSON *timing_obj = cJSON_CreateObject();
    s &= cJSON_AddItemToObject(root, "timingInfo", timing_obj);
    {
        enum {
            TIMEREPR_LEN = 4096,
        };

        char timerepr_str[TIMEREPR_LEN];
        TimeRepr timerepr = timerepr_from_usecs(timing.took_usecs);
        timerepr_to_string(&timerepr, timerepr_str, ARRAY_LEN(timerepr_str));

        s &= cJSON_AddItemToObject(
            timing_obj, "took",
            cJSON_CreateNumber((double)timing.took_usecs * USECS_TO_SECS));
        s &= cJSON_AddItemToObject(timing_obj, "tookRepr",
                                   cJSON_CreateString(timerepr_str));

        char *time = ctime(&timing.started);
        // Remove newline introduced from ctime
        time[strlen(time) - 1] = 0;

        s &= cJSON_AddItemToObject(timing_obj, "started",
                                   cJSON_CreateString(time));

        time = ctime(&timing.ended);
        // Remove newline introduced from ctime
        time[strlen(time) - 1] = 0;
        s &= cJSON_AddItemToObject(timing_obj, "ended",
                                   cJSON_CreateString(time));
    }

    cJSON *bounds_obj = cJSON_CreateObject();
    s &= cJSON_AddItemToObject(root, "bounds", bounds_obj);
    {

        s &= cJSON_AddItemToObject(bounds_obj, "dual",
                                   cJSON_CreateNumber(solution->dual_bound));

        s &= cJSON_AddItemToObject(bounds_obj, "primal",
                                   cJSON_CreateNumber(solution->primal_bound));

        s &= cJSON_AddItemToObject(
            bounds_obj, "gap", cJSON_CreateNumber(solution_relgap(solution)));
    }

    if (primal_sol_avail) {
        cJSON *tour_info_obj = cJSON_CreateObject();
        s &= cJSON_AddItemToObject(root, "tourInfo", tour_info_obj);
        {
            double cost = tour_eval(instance, &solution->tour);
            double profit = tour_profit(instance, &solution->tour);
            double demand = tour_demand(instance, &solution->tour);

            s &= cJSON_AddItemToObject(tour_info_obj, "cost",
                                       cJSON_CreateNumber(cost));
            s &= cJSON_AddItemToObject(tour_info_obj, "profit",
                                       cJSON_CreateNumber(profit));
            s &= cJSON_AddItemToObject(tour_info_obj, "demand",
                                       cJSON_CreateNumber(demand));

            cJSON *route_array = cJSON_CreateArray();
            int32_t curr_vertex = 0;
            int32_t next_vertex = curr_vertex;
            do {
                next_vertex = *tsucc(&solution->tour, curr_vertex);
                cJSON_AddItemToArray(route_array,
                                     cJSON_CreateNumber(curr_vertex));

                curr_vertex = next_vertex;
            } while (curr_vertex != 0);
            s &= cJSON_AddItemToObject(tour_info_obj, "route", route_array);
        }
    }

    cJSON *constants_obj = cJSON_CreateObject();
    s &= cJSON_AddItemToObject(root, "constants", constants_obj);
    {
        s &= cJSON_AddItemToObject(constants_obj, "COST_TOLERANCE",
                                   cJSON_CreateNumber(COST_TOLERANCE));
    }

    if (!s) {
        log_fatal("%s :: Failed to add all the necessary JSON elements to the "
                  "parent JSON root object",
                  __func__);
        goto cleanup;
    }

    char *content = cJSON_Print(root);
    fprintf(fh, "%s", content);
    free(content);

cleanup:
    if (root)
        cJSON_Delete(root);
    if (fh)
        fclose(fh);
}

static int main2(AppCtx *ctx) {
    Instance instance = parse(ctx->instance_filepath);
    if (is_valid_instance(&instance)) {
        SolverParams params =
            make_solver_params_from_cmdline(ctx->defines, ctx->num_defines);
        Solution solution = solution_create(&instance);

        bool success = true;

        // Solve, timing and printing of final solution
        {
            Timing timing;
            timing.started = time(NULL);

            int64_t begin_solve_time = os_get_usecs();
            SolveStatus status =
                cptp_solve(&instance, ctx->solver ? ctx->solver : "mip",
                           &params, &solution, ctx->timelimit, ctx->randomseed);

            timing.ended = time(NULL);
            timing.took_usecs = os_get_usecs() - begin_solve_time;

            success = status != 0 && !BOOL(status & SOLVE_STATUS_ERR);
            if (ctx->treat_sigterm_as_failure &&
                BOOL(status & SOLVE_STATUS_ABORTION_SIGTERM)) {
                success = false;
            }

            printf("\n\n###\n###\n###\n\n");
            writeout_results(stdout, ctx, success, &instance, &solution, status,
                             timing);

            if (success) {
                writeout_json_report(ctx, &instance, &solution, status, timing);

                if (ctx->vis_path) {
                    render_tour_image(ctx->vis_path, &instance, &solution.tour,
                                      NULL);
                }
            }
        }

        instance_destroy(&instance);
        solution_destroy(&solution);

        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
        fprintf(stderr, "%s: Failed to parse file\n", ctx->instance_filepath);
        return EXIT_FAILURE;
    }
}

enum {
    MAX_NUMBER_OF_ERRORS_TO_DISPLAY = 16,
};

int main(int argc, char **argv) {
    const char *progname = argv[0];
    FILE *log_file_handle = NULL;

    if (argc == 1) {
        print_brief_description(progname);
        print_version();
        printf("\n");
        print_use_help_for_more_information(progname);
        return EXIT_FAILURE;
    }

    struct arg_dbl *timelimit = arg_dbl0(
        "t", "timelimit", NULL,
        "define the maximum timelimit in seconds (default 10 minutes)");
    struct arg_int *randomseed =
        arg_int0("s", "seed", NULL,
                 "define the random seed to use (default is 0, eg compute it "
                 "from the current time)");
    struct arg_lit *treat_sigterm_as_failure =
        arg_lit0("a", "treat-sigterm-as-failure",
                 "treat SIGTERM/SIGINT (CTRL-C) abortion as failure and exit "
                 "with non zero exit status. The JSON report output file will "
                 "not be generated");
    struct arg_str *defines =
        arg_strn("D", "define", "KEY=VALUE", 0, argc + 2, "define parameters");
    struct arg_str *solver =
        arg_str0("S", "solver", "SOLVER", "solver to use (default \"mip\")");
    struct arg_int *loglvl =
        arg_int0(NULL, "loglvl", NULL,
                 "control the log level (0: fatal&warning logs, 1: info logs, "
                 "2: trace logs, 3: debug logs (only in debug builds)");
    struct arg_file *logfile =
        arg_file0("l", "log", NULL,
                  "specify an additional file where logs would be "
                  "stored (default none)");
    struct arg_lit *help = arg_lit0(NULL, "help", "print this help and exit");
    struct arg_lit *version =
        arg_lit0(NULL, "version", "print version information and exit");
    struct arg_file *instance =
        arg_file1("i", "instance", NULL, "input instance file");

    struct arg_file *vis_path =
        arg_file0(NULL, "visualize", NULL, "tour visualization output file");
    struct arg_file *json_report_path = arg_file0(
        "w", "write-report", NULL, "write a JSON report output file.");

    struct arg_end *end = arg_end(MAX_NUMBER_OF_ERRORS_TO_DISPLAY);

    void *argtable[] = {help,
                        version,
                        loglvl,
                        logfile,
                        treat_sigterm_as_failure,
                        timelimit,
                        randomseed,
                        defines,
                        instance,
                        vis_path,
                        json_report_path,
                        solver,
                        end};

    int exitcode = 0;

    /* verify the argtable[] entries were allocated successfully */
    if (arg_nullcheck(argtable) != 0) {
        printf("%s: insufficient memory\n", progname);
        exitcode = 1;
        goto exit;
    }

    // Default time limit
    timelimit->dval[0] = DEFAULT_TIME_LIMIT;
    // Default random seed
    randomseed->ival[0] = 0;
    // Default solver
    solver->sval[0] = "mip";
    // No logging file by default
    logfile->filename[0] = NULL;
    // No tour visualization file by default
    vis_path->filename[0] = NULL;
    // No JSON report output file by default
    json_report_path->filename[0] = NULL;
    // Contain only fatal&warning log messages by default
    loglvl->ival[0] = 0;

    {
        int nerrors = arg_parse(argc, argv, argtable);

        /* special case: '--help' takes precedence over error reporting */
        if (help->count > 0) {
            print_brief_description(progname);
            printf("Usage: %s", progname);
            arg_print_syntax(stdout, argtable, "\n");
            arg_print_glossary(stdout, argtable, "  %-32s %s\n");
            cptp_print_list_of_solvers_and_params();
            exitcode = 0;
            goto exit;
        }

        if (nerrors > 0) {
            arg_print_errors(stdout, end, progname);
            print_use_help_for_more_information(progname);
            exitcode = 1;
            goto exit;
        }
    }

    /* special case: '--version' takes precedence error reporting */
    if (version->count > 0) {
        print_version();
        exitcode = 0;
        goto exit;
    }

    int32_t iloglvl = LOG_WARN;
    if (loglvl->ival[0] <= 0) {
        iloglvl = LOG_WARN;
    } else if (loglvl->ival[0] == 1) {
        iloglvl = LOG_INFO;
    } else if (loglvl->ival[0] == 2) {
        iloglvl = LOG_TRACE;
    } else {
        iloglvl = LOG_DEBUG;
    }

    log_set_level(iloglvl);

    if (logfile->count > 0) {
        log_file_handle = fopen(logfile->filename[0], "w");
        if (log_file_handle) {
            log_add_fp(log_file_handle, iloglvl);
        } else {
            fprintf(stderr, "%s: Failed to open for logging\n",
                    logfile->filename[0]);
        }
    }

    AppCtx ctx = {.loglvl = iloglvl,
                  .instance_filepath = instance->filename[0],
                  .treat_sigterm_as_failure =
                      treat_sigterm_as_failure->count > 0,
                  .solver = solver->sval[0],
                  .timelimit = timelimit->dval[0],
                  .randomseed = randomseed->ival[0],
                  .defines = defines->sval,
                  .num_defines = defines->count,
                  .vis_path = vis_path->filename[0],
                  .json_report_path = json_report_path->filename[0]};

    if (ctx.randomseed == 0) {
        ctx.randomseed = (int32_t)(time(NULL) % INT32_MAX);
    }

    exitcode = main2(&ctx);

exit:
    arg_freetable(argtable, ARRAY_LEN(argtable));

    if (log_file_handle) {
        fclose(log_file_handle);
        log_file_handle = NULL;
    }

    return exitcode;
}

static void print_brief_description(const char *progname) {
    UNUSED_PARAM(progname);
    printf("%s: %s\n", PROJECT_NAME, PROJECT_DESCRIPTION);
}

static void print_version(void) {
    printf("%s v%s (%s, revision: %s)\n", PROJECT_NAME, PROJECT_VERSION,
           GIT_DATE, GIT_SHA1);
    printf("Compiled with %s v%s (%s), %s build\n", C_COMPILER_ID,
           C_COMPILER_VERSION, C_COMPILER_ABI, BUILD_TYPE);
}

static void print_use_help_for_more_information(const char *progname) {
    printf("Try '%s --help' for more information.\n", progname);
}

static void print_tour(Tour *t) {
    printf("%-16s ", "TOUR:");

    int32_t curr_vertex = 0;
    int32_t next_vertex = curr_vertex;

    do {
        next_vertex = *tsucc(t, curr_vertex);
        if (next_vertex == 0) {
            // Do not put the space for cleanliness if is the last vertex to be
            // printed
            printf("%d", curr_vertex);
        } else {
            printf("%d ", curr_vertex);
        }
        curr_vertex = next_vertex;
    } while (curr_vertex != 0);

    printf("\n");
}
