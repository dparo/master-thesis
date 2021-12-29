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
#include "visualization.h"

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
    bool treat_abort_as_failure;
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
    bool valid = is_valid_solve_status(status);

    fprintf(fh, "%-16s %s\n", "SOLVER:", ctx->solver);
    fprintf(fh, "%-16s %f\n", "TIMELIM:", ctx->timelimit);
    fprintf(fh, "%-16s %d\n", "SEED:", ctx->randomseed);
    fprintf(fh, "%-16s %s\n", "INPUT:", ctx->instance_filepath);

    if (valid) {
        printf("%-16s [%.17g, %.17g]\n", "OBJ:", solution->lower_bound,
               solution->upper_bound);
        print_tour(&solution->tour);
    } else {
        printf("%-16s Could not solve\n", "ERR:");
    }

    double cost = tour_eval(instance, &solution->tour);
    printf("%-16s %.17g\n", "COST:", cost);

    if (instance->zero_reduced_cost_threshold != 0) {
        printf("%-16s %.17g\n",
               "COST THRESHOLD:", instance->zero_reduced_cost_threshold);
        printf("%-16s %.17g\n",
               "RELATIVE COST:", cost - instance->zero_reduced_cost_threshold);
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

    FILE *fh = fopen(ctx->json_report_path, "w");
    if (!fh) {
        log_fatal("%s: failed to open file for writing JSON report",
                  ctx->json_report_path);
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        log_fatal("%s :: Failed to create JSON root object", __func__);
        fclose(fh);
        return;
    }

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

    s &= cJSON_AddItemToObject(root, "instanceName",
                               cJSON_CreateString(instance->name));
    s &= cJSON_AddItemToObject(root, "instanceComment",
                               cJSON_CreateString(instance->comment));
    s &= cJSON_AddItemToObject(root, "vehicleCap",
                               cJSON_CreateNumber(instance->vehicle_cap));
    s &= cJSON_AddItemToObject(root, "numCustomers",
                               cJSON_CreateNumber(instance->num_customers));
    s &= cJSON_AddItemToObject(root, "numVehicles",
                               cJSON_CreateNumber(instance->num_vehicles));

    double obj[2] = {solution->lower_bound, solution->upper_bound};
    s &= cJSON_AddItemToObject(root, "obj",
                               cJSON_CreateDoubleArray(obj, ARRAY_LEN(obj)));

    double cost = tour_eval(instance, &solution->tour);
    s &= cJSON_AddItemToObject(root, "cost", cJSON_CreateNumber(cost));

    s &= cJSON_AddItemToObject(
        root, "zeroReducedCostThreshold",
        cJSON_CreateNumber(instance->zero_reduced_cost_threshold));
    s &= cJSON_AddItemToObject(
        root, "relativeCost",
        cJSON_CreateNumber(cost - instance->zero_reduced_cost_threshold));

    char *time = ctime(&timing.started);
    // Remove newline introduced from ctime
    time[strlen(time) - 1] = 0;

    s &= cJSON_AddItemToObject(root, "started", cJSON_CreateString(time));

    time = ctime(&timing.ended);
    // Remove newline introduced from ctime
    time[strlen(time) - 1] = 0;
    s &= cJSON_AddItemToObject(root, "ended", cJSON_CreateString(time));

    enum {
        TIMEREPR_LEN = 4096,
    };

    char timerepr_str[TIMEREPR_LEN];
    TimeRepr timerepr = timerepr_from_usecs(timing.took_usecs);
    timerepr_to_string(&timerepr, timerepr_str, ARRAY_LEN(timerepr_str));

    s &= cJSON_AddItemToObject(root, "took",
                               cJSON_CreateNumber(timing.took_usecs / 1.0e6));
    s &= cJSON_AddItemToObject(root, "tookRepr",
                               cJSON_CreateString(timerepr_str));

    if (!s) {
        log_fatal("%s :: Failed to create JSON root object", __func__);
        fclose(fh);
        return;
    }
    char *content = cJSON_Print(root);
    fprintf(fh, "%s", content);
    cJSON_Delete(root);
    free(content);
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

            const bool valid = is_valid_solve_status(status);
            const bool aborted = is_aborted_solve_status(status);
            success = ctx->treat_abort_as_failure ? valid && !aborted : valid;

            printf("\n\n###\n###\n###\n\n");
            writeout_results(stdout, ctx, success, &instance, &solution, status,
                             timing);

            if (success) {
                writeout_json_report(ctx, &instance, &solution, status, timing);

                if (ctx->vis_path) {
                    tour_plot(ctx->vis_path, &instance, &solution.tour, NULL);
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
    struct arg_lit *treat_abort_as_failure =
        arg_lit0("a", "treat-abort-as-failure",
                 "treat abortion, eg a SIGTERM (CTRL-C), as failure and exit "
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
                        treat_abort_as_failure,
                        timelimit,
                        randomseed,
                        defines,
                        instance,
                        vis_path,
                        json_report_path,
                        solver,
                        end};

    int nerrors;
    int exitcode = 0;

    /* verify the argtable[] entries were allocated sucessfully */
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

    nerrors = arg_parse(argc, argv, argtable);

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

    /* special case: '--version' takes precedence error reporting */
    if (version->count > 0) {
        print_version();
        exitcode = 0;
        goto exit;
    }

    /* If the parser returned any errors then display them and exit */
    if (nerrors > 0) {
        arg_print_errors(stdout, end, progname);
        print_use_help_for_more_information(progname);
        exitcode = 1;
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
                  .treat_abort_as_failure = treat_abort_as_failure->count > 0,
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
