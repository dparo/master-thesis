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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "misc.h"
#include "version.h"
#include "types.h"
#include "core.h"
#include "parser.h"
#include "timing.h"
#include "core-utils.h"
#include "visualization.h"

#include <log.h>
#include <argtable3.h>

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
        const char *value;
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

static int main2(const char *instance_filepath, const char *solver,
                 double timelimit, int32_t randomseed, const char **defines,
                 int32_t num_defines, const char *vis_path) {
    const char *filepath = instance_filepath;
    Instance instance = parse(filepath);
    if (instance.num_customers > 0) {
        SolverParams params =
            make_solver_params_from_cmdline(defines, num_defines);
        Solution solution = solution_create(&instance);

        bool success = false;

        // Solve, timing and printing of final solution
        {
            time_t started = time(NULL);
            usecs_t begin_solve_time = os_get_usecs();
            SolveStatus status =
                cptp_solve(&instance, solver ? solver : "mip", &params,
                           &solution, timelimit, randomseed);

            success = cptp_solve_did_found_tour_solution(status);

            time_t ended = time(NULL);
            usecs_t solve_time = os_get_usecs() - begin_solve_time;

            printf("\n\n###\n###\n###\n\n");

            printf("%-12s %s\n", "SOLVER:", solver);
            printf("%-12s %f\n", "TIMELIM:", timelimit);
            printf("%-12s %s\n", "INPUT:", instance_filepath);

            if (success) {
                printf("%-12s [%f, %f]\n", "OBJ:", solution.lower_bound,
                       solution.upper_bound);
                print_tour(&solution.tour);
            } else {
                printf("%-12s Could not solve\n", "ERR:");
            }

            printf("%-12s %s", "STARTED:", ctime(&started));
            printf("%-12s %s", "ENDED:", ctime(&ended));
            printf("%-12s ", "TOOK:");

            TimeRepr solve_time_repr = timerepr_from_usecs(solve_time);
            print_timerepr(stdout, &solve_time_repr);
            printf("\n");

            if (vis_path) {
                tour_plot(vis_path, &instance, &solution.tour, NULL);
            }
        }

        instance_destroy(&instance);
        solution_destroy(&solution);

        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
        fprintf(stderr, "%s: Failed to parse file\n", instance_filepath);
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
    struct arg_str *defines =
        arg_strn("D", "define", "KEY=VALUE", 0, argc + 2, "define parameters");
    struct arg_str *solver =
        arg_str0("S", "solver", "SOLVER", "solver to use (default \"mip\")");
    struct arg_lit *verbose = arg_lit0("v", "verbose", "verbose messages");
    struct arg_file *logfile =
        arg_file0("l", "log", NULL,
                  "specify an additional file where log informations would be "
                  "stored (default none)");
    struct arg_lit *help = arg_lit0(NULL, "help", "print this help and exit");
    struct arg_lit *version =
        arg_lit0(NULL, "version", "print version information and exit");
    struct arg_file *instance =
        arg_file1("i", "instance", NULL, "input instance file");

    struct arg_file *vis_path =
        arg_file0(NULL, "visualize", NULL, "tour visualization output file");

    struct arg_end *end = arg_end(MAX_NUMBER_OF_ERRORS_TO_DISPLAY);

    void *argtable[] = {help,      version,    verbose, logfile,
                        timelimit, randomseed, defines, instance,
                        vis_path,  solver,     end};

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

    if (verbose->count > 0) {
        log_set_level(LOG_INFO);
    } else {
        log_set_level(LOG_WARN);
    }

    if (logfile->count > 0) {
        log_file_handle = fopen(logfile->filename[0], "w");
        if (log_file_handle) {
            log_add_fp(log_file_handle, LOG_INFO);
        } else {
            fprintf(stderr, "%s: Failed to open for logging\n",
                    logfile->filename[0]);
        }
    }

    exitcode = main2(instance->filename[0], solver->sval[0], timelimit->dval[0],
                     randomseed->ival[0], defines->sval, defines->count,
                     vis_path->filename[0]);

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
    printf("%-12s ", "TOUR:");

    int32_t curr_vertex = 0;
    int32_t next_vertex = curr_vertex;

    while ((next_vertex = *tsucc(t, curr_vertex)) != 0) {
        if (next_vertex == 0) {
            // Do not put the space for cleanliness if is the last vertex to be
            // printed
            printf("%d", curr_vertex);
        } else {
            printf("%d ", curr_vertex);
        }
        curr_vertex = next_vertex;
    }

    printf("\n");
}
