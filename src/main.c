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

#include <log.h>
#include "misc.h"
#include "version.h"
#include "types.h"
#include <argtable3.h>
#include "core.h"
#include "parser.h"

static void print_brief_description(const char *progname);
static void print_version(void);
static void print_use_help_for_more_information(const char *progname);

static int main2(const char *instance_filepath, const char *solver,
                 double timelimit, const char **defines, int32_t num_defines) {

    const char *filepath = instance_filepath;
    Instance instance = parse(filepath);
    if (instance.num_customers > 0) {
        SolverParams params = {0};
        Solution solution = solution_create(&instance);
        SolveStatus status =
            cptp_solve(&instance, solver ? solver : "mip", &params, &solution);

        printf("\n\n###\n###    Terminating with solution cost in "
               "[%f, %f]\n###\n\n",
               solution.lower_bound, solution.upper_bound);
        instance_destroy(&instance);
        solution_destroy(&solution);
        bool success =
            status == SOLVE_STATUS_OPTIMAL || status == SOLVE_STATUS_FEASIBLE;

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
    struct arg_str *defines =
        arg_strn("D", "define", "KEY=VALUE", 0, argc + 2, "define parameters");
    struct arg_str *solver =
        arg_str0("S", "solver", "SOLVER", "solver to use (default \"mip\")");
    struct arg_lit *verbose =
        arg_lit0("v", "verbose,debug", "verbose messages");
    struct arg_lit *help = arg_lit0(NULL, "help", "print this help and exit");
    struct arg_lit *version =
        arg_lit0(NULL, "version", "print version information and exit");
    struct arg_file *instance =
        arg_file1("i", "instance", NULL, "input instance file");
    struct arg_end *end = arg_end(MAX_NUMBER_OF_ERRORS_TO_DISPLAY);

    void *argtable[] = {help,    version,  verbose, timelimit,
                        defines, instance, solver,  end};

    int nerrors;
    int exitcode = 0;

    /* verify the argtable[] entries were allocated sucessfully */
    if (arg_nullcheck(argtable) != 0) {
        /* NULL entries were detected, some allocations must have failed */
        printf("%s: insufficient memory\n", progname);
        exitcode = 1;
        goto exit;
    }

    // Default time limit of 10 minutes
    timelimit->dval[0] = 10.0 * 60.0;
    // Default solver
    solver->sval[0] = "mip";

    /* Parse the command line as defined by argtable[] */
    nerrors = arg_parse(argc, argv, argtable);

    /* special case: '--help' takes precedence over error reporting */
    if (help->count > 0) {
        print_brief_description(progname);
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-32s %s\n");
        exitcode = 0;
        goto exit;
    }

    /* special case: '--version' takes precedence error reporting */
    if (version->count > 0) {
        printf("%s (GIT SHA: %s)\n", GIT_DATE, GIT_SHA1);
        exitcode = 0;
        goto exit;
    }

    /* If the parser returned any errors then display them and exit */
    if (nerrors > 0) {
        /* Display the error details contained in the arg_end struct.*/
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

    /* normal case: take the command line options at face value */
    exitcode = main2(instance->filename[0], solver->sval[0], timelimit->dval[0],
                     defines->sval, defines->count);

exit:
    /* deallocate each non-null entry in argtable[] */
    arg_freetable(argtable, ARRAY_LEN(argtable));

    return exitcode;
}

static void print_brief_description(const char *progname) {
    printf("The Capacitated Profitable Tour Problem (CPTP) solver\n");
}

static void print_version(void) {
    printf("%s (GIT SHA: %s)\n", GIT_DATE, GIT_SHA1);
}

static void print_use_help_for_more_information(const char *progname) {
    printf("Try '%s --help' for more information.\n", progname);
}
