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
#include "render.h"

enum {
    MAX_NUMBER_OF_ERRORS_TO_DISPLAY = 16,
};

typedef struct {
    const char *input;
    const char *output;
    int32_t num_vehicles;
    double cap_scale_factor;
} AppCtx;

Instance process_instance(const Instance *instance, const AppCtx *ctx) {
    Instance result = instance_copy(instance, true, true);

    result.vehicle_cap = result.vehicle_cap * ctx->cap_scale_factor;
    result.num_vehicles =
        (int32_t)(ceil((double)result.num_vehicles / ctx->cap_scale_factor));
    result.num_vehicles = MAX(1, result.num_vehicles);

    return result;
}

int main2(const AppCtx *ctx) {
    Instance instance = parse(ctx->input);

    // NOTE(dparo):
    //        Unfortunately CVRP instances encode the number of vehicles
    //        in the file name instead of using a special VRPLIB entry
    //        So in order to  keep this program simple, if the NUM_VEHICLES
    //        VRPLIB entry is not found we substitute it using the command line
    //        specified num_vehicles
    if (ctx->num_vehicles > 0 && instance.num_vehicles == 0) {
        instance.num_vehicles = ctx->num_vehicles;
    }

    if (!is_valid_instance(&instance)) {
        fprintf(stderr, "%s: failed to parse\n", ctx->input);
        instance_destroy(&instance);
        return EXIT_FAILURE;
    }

    FILE *fh = fopen(ctx->output, "w");
    if (!fh) {
        fprintf(stderr, "%s: failed to open file for writing\n", ctx->output);
        instance_destroy(&instance);
        return EXIT_FAILURE;
    }

    Instance new_instance = process_instance(&instance, ctx);
    render_instance_into_vrplib_file(fh, &new_instance, false);

    instance_destroy(&instance);
    instance_destroy(&new_instance);
    fclose(fh);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    char *progname = argv[0];
    int exitcode = EXIT_SUCCESS;
    struct arg_lit *help = arg_lit0(NULL, "help", "print this help and exit");

    struct arg_file *input = arg_file1("i", NULL, NULL, "input instance file");

    struct arg_file *output =
        arg_file1("o", NULL, NULL, "output instance file");

    struct arg_int *num_vehicles =
        arg_int1("k", "num-vehicles", NULL, "force number of vehicles");

    struct arg_dbl *cap_scale_factor = arg_dbl0(
        "f", "cap-scale-factor", NULL, "scale factor for the vehicle capacity");

    struct arg_end *end = arg_end(MAX_NUMBER_OF_ERRORS_TO_DISPLAY);

    void *argtable[] = {help, input, output, num_vehicles, cap_scale_factor,
                        end};

    /* verify the argtable[] entries were allocated successfully */
    if (arg_nullcheck(argtable) != 0) {
        printf("%s: insufficient memory\n", progname);
        exitcode = 1;
        goto exit;
    }

    cap_scale_factor->dval[0] = 1.0;

    {
        int nerrors = arg_parse(argc, argv, argtable);

        /* special case: '--help' takes precedence over error reporting */
        if (help->count > 0) {
            printf("Usage: %s", progname);
            arg_print_syntax(stdout, argtable, "\n");
            arg_print_glossary(stdout, argtable, "  %-32s %s\n");
            exitcode = 0;
            goto exit;
        }

        if (nerrors > 0) {
            arg_print_errors(stdout, end, progname);
            exitcode = 1;
            goto exit;
        }
    }

    AppCtx ctx = {.input = input->filename[0],
                  .output = output->filename[0],
                  .num_vehicles = num_vehicles->ival[0],
                  .cap_scale_factor = cap_scale_factor->dval[0]};
    exitcode = main2(&ctx);

exit:
    arg_freetable(argtable, ARRAY_LEN(argtable));
    return exitcode;
}
