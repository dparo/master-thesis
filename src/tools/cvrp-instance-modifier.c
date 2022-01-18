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

#include <argtable3.h>
#include "core.h"
#include "parser.h"

enum {
    MAX_NUMBER_OF_ERRORS_TO_DISPLAY = 16,
};

static void print_usage(FILE *fh, char *progname) {
    fprintf(fh, "%s [INPUT-TEST-INSTANCE] [OUTPUT-TEST-INSTANCE]\n", progname);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    int exitcode = EXIT_SUCCESS;
    struct arg_lit *help = arg_lit0(NULL, "help", "print this help and exit");

    struct arg_file *input = arg_file1("i", NULL, NULL, "input instance file");

    struct arg_file *output =
        arg_file1("o", NULL, NULL, "output instance file");
    struct arg_end *end = arg_end(MAX_NUMBER_OF_ERRORS_TO_DISPLAY);

    void *argtable[] = {help, input, output, end};

    /* verify the argtable[] entries were allocated sucessfully */
    if (arg_nullcheck(argtable) != 0) {
        printf("%s: insufficient memory\n", progname);
        exitcode = 1;
        goto exit;
    }

    {
        int nerrors = arg_parse(argc, argv, argtable);

        if (nerrors > 0) {
            arg_print_errors(stdout, end, progname);
            print_use_help_for_more_information(progname);
            exitcode = 1;
            goto exit;
        }
    }

    if (help->count > 0) {
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-32s %s\n");
        exitcode = 0;
        goto exit;
    }

    Instance instance = parse(input);
    if (!is_valid_instance(&instance)) {
        fprintf(stderr, "%s: failed to parse\n", input);
        exit(EXIT_FAILURE);
    }

    FILE *fh = fopen(output, "w");
    if (!fh) {
        fprintf(stderr, "%s: failed to open file for writing\n", output);
        instance_destroy(&instance);
        exit(EXIT_FAILURE);
    }

    fprintf(fh, "NAME : %s\n", instance.name);
    fprintf(fh, "COMMENT : %s\n", instance.comment ? instance.comment : "");
    fprintf(fh, "TYPE : %s\n", "CVRP");
    fprintf(fh, "DIMENSION : %d\n", instance.num_customers + 1);
    fprintf(fh, "VEHICLES : %d\n", instance.num_vehicles);
    fprintf(fh, "CAPACITY : %g\n", instance.vehicle_cap);
    fprintf(fh, "EDGE_WEIGHT_TYPE : %s\n", "EUC_2D");

    // Generate node coordinate section
    fprintf(fh, "NODE_COORD_SECTION\n");

    for (int32_t i = 0; i < instance.num_customers + 1; i++) {
        fprintf(fh, "%d %g %g\n", i + 1, instance.positions[i].x,
                instance.positions[i].y);
    }

    // Generate demand section
    fprintf(fh, "DEMAND_SECTION\n");
    for (int32_t i = 0; i < instance.num_customers + 1; i++) {
        fprintf(fh, "%d %g\n", i + 1, instance.demands[i]);
    }

    // Generate depot section
    fprintf(fh, "DEPOT_SECTION\n");
    fprintf(fh, "%d\n", 1);
    fprintf(fh, "%d\n", -1);

    // Generate profit section
    fprintf(fh, "PROFIT_SECTION\n");
    for (int32_t i = 0; i < instance.num_customers + 1; i++) {
        fprintf(fh, "%d %.17g\n", i + 1, instance.profits[i]);
    }

    instance_destroy(&instance);
    arg_freetable(argtable, ARRAY_LEN(argtable));

    if (log_file_handle) {
        fclose(log_file_handle);
        log_file_handle = NULL;
    }

exit:
    arg_freetable(argtable, ARRAY_LEN(argtable));
    return exitcode;
}