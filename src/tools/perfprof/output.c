/*
 * Copyright (c) 2022 Davide Paro
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

#include "output.h"

typedef enum {
    VALUE_PROCESSING_KIND_NONE,
    VALUE_PROCESSING_KIND_TIME,
    VALUE_PROCESSING_KIND_RAW,
} ValueProcessingKind;

typedef struct {
    ValueProcessingKind kind;
    char *name;
    char *title;
    char *x_title;
    double shift;
    double x_max;
} StatPlottingInfo;

static void invoke_plot_pyscript(const PerfProfBatch *batch,
                                 const StatPlottingInfo *plotting_info,
                                 char *csv_input_file) {
    const ValueProcessingKind processing_kind = plotting_info->kind;
    assert(processing_kind != VALUE_PROCESSING_KIND_NONE);

    char title[512] = "No Title";
    char shift[128] = "0.0";
    char x_max[128] = "";
    char x_raw_pper_limit[128] = "1e99";
    char xlabel_str[128] = "NO LABEL";
    Path csv_input_file_basename = {0};
    char output_file[OS_MAX_PATH] = {0};

    snprintf_safe(title, ARRAY_LEN(title), "%s of %s", plotting_info->title,
                  batch->name);

    snprintf_safe(xlabel_str, ARRAY_LEN(xlabel_str), "%s",
                  plotting_info->x_title);

    snprintf_safe(shift, ARRAY_LEN(shift), "%g", plotting_info->shift);
    snprintf_safe(x_max, ARRAY_LEN(x_max), "%g", plotting_info->x_max);

    snprintf_safe(output_file, ARRAY_LEN(output_file), "%s/%s_plot.pdf",
                  os_dirname(csv_input_file, &csv_input_file_basename),
                  plotting_info->name);

    char *args[PROC_MAX_ARGS];
    int32_t argidx = 0;

    args[argidx++] = "python3";
    args[argidx++] = PYTHON3_PERF_SCRIPT;
    args[argidx++] = "--delimiter";
    args[argidx++] = ",";

    switch (processing_kind) {
    case VALUE_PROCESSING_KIND_NONE: {
        assert(!"Invalid code path");
        break;
    }
    case VALUE_PROCESSING_KIND_TIME: {
        snprintf_safe(x_raw_pper_limit, ARRAY_LEN(x_raw_pper_limit), "%g",
                      batch->timelimit);

        args[argidx++] = "--x-max";
        args[argidx++] = x_max;
        args[argidx++] = "--x-raw-upper-limit";
        args[argidx++] = x_raw_pper_limit;
        args[argidx++] = "--shift";
        args[argidx++] = shift;
        break;
    }
    case VALUE_PROCESSING_KIND_RAW: {
        args[argidx++] = "--draw-reduced-cost-regions";
        args[argidx++] = "--raw-data";
        break;
    }
    }

    args[argidx++] = "--plot-title";
    args[argidx++] = title;
    args[argidx++] = "--x-label";
    args[argidx++] = xlabel_str;
    args[argidx++] = "-i";
    args[argidx++] = csv_input_file;
    args[argidx++] = "-o";
    args[argidx++] = output_file;
    args[argidx++] = NULL;

    proc_spawn_sync(args);
}

static void dump_csv(FILE *fh, int32_t stat_kind, const AppCtx *ctx,
                     const PerfProfBatch *batch) {
    // Compute the number of solvers in the batch
    int32_t num_solvers = 0;
    for (int32_t i = 0; batch->solvers[i].name; i++) {
        num_solvers++;
    }

    //
    // Write out the header
    // ncols          |  solver1      |  solver2       |  ...
    // ======================================================
    //
    fprintf(fh, "%d", num_solvers);
    for (int32_t i = 0; i < num_solvers; i++) {
        fprintf(fh, ",%s", batch->solvers[i].name);
    }
    fprintf(fh, "\n");

    //
    // Write out the actual data
    //
    // instance_name1    time(algo1)     time(algo2)      ...
    // instance_name2    time(algo1)     time(algo2)      ...
    // instance_name3    time(algo1)     time(algo2)      ...
    // ...               ...             ...              ...
    //
    size_t tbl_len = hmlenu(ctx->perf_tbl.buf);
    for (size_t i = 0; i < tbl_len; i++) {
        const PerfTblKey *key = &ctx->perf_tbl.buf[i].key;
        const PerfTblValue *value = &ctx->perf_tbl.buf[i].value;
        assert(value->num_runs == num_solvers);

        const Hash *hash = &key->uid.hash;
        uint8_t seedidx = key->uid.seedidx;

        // Output the instance name for this row
        fprintf(fh, "%d:%s", seedidx, hash->cstr);

        // NOTE(dparo)
        // Due to the out of order generation of the performance table,
        // the perf data may be out of order, compared to the order of
        // the solvers considered when outputting the CSV file.
        // Therefore fix a solver name, and find its perf in the list,
        // to output the perf data in the expected order
        for (int32_t solver_idx = 0; solver_idx < num_solvers; solver_idx++) {
            // Fix the solver name as in order
            char *solver_name = batch->solvers[solver_idx].name;

            for (int32_t run_idx = 0; run_idx < value->num_runs; run_idx++) {
                const PerfProfRun *run = &value->runs[run_idx];
                if (0 == strcmp(run->solver_name, solver_name)) {
                    const double val = run->solution.stats[stat_kind];
                    fprintf(fh, ",%.17g", val);
                }
            }
        }

        fprintf(fh, "\n");
    }
}

void dump_performance_profiles(const AppCtx *ctx, const PerfProfBatch *batch) {

    static const StatPlottingInfo PLOTTING_INFO[PERFPROF_MAX_NUM_STATS] = {
        [PERFPROF_STAT_KIND_TIME] =
            {
                VALUE_PROCESSING_KIND_TIME,
                "Time",
                "Time profile",
                "Time Ratio",
                1e-1,
                20.0,
            },
        [PERFPROF_STAT_KIND_PRIMAL_BOUND] = {VALUE_PROCESSING_KIND_RAW,
                                             "PrimalBound",
                                             "Primal Bound profile",
                                             "Primal Bound", 1e-9,
                                             CRASHED_SOLVER_DEFAULT_COST_VAL},
        [PERFPROF_STAT_KIND_DUAL_BOUND] = {VALUE_PROCESSING_KIND_RAW,
                                           "DualBound", "Dual Bound profile",
                                           "Dual Bound", 1e-9,
                                           CRASHED_SOLVER_DEFAULT_COST_VAL},
    };

    printf("\n\n\n");
    char dump_dir[OS_MAX_PATH];

    os_mkdir(PERFPROF_DUMP_ROOTDIR, true);
    os_mkdir(PERFPROF_DUMP_ROOTDIR "/Plots", true);
    snprintf_safe(dump_dir, ARRAY_LEN(dump_dir),
                  PERFPROF_DUMP_ROOTDIR "/Plots/%s", batch->name);
    os_mkdir(dump_dir, true);

    for (int32_t stat_idx = 0; stat_idx < (int32_t)ARRAY_LEN(PLOTTING_INFO);
         stat_idx++) {
        const int32_t processing_kind = PLOTTING_INFO[stat_idx].kind;
        if (processing_kind == VALUE_PROCESSING_KIND_NONE) {
            continue;
        }

        char out_csv_file[OS_MAX_PATH];

        snprintf_safe(out_csv_file, ARRAY_LEN(out_csv_file), "%s/%s.csv",
                      dump_dir, PLOTTING_INFO[stat_idx].name);

        FILE *fh = fopen(out_csv_file, "w");

        if (fh) {
            dump_csv(fh, stat_idx, ctx, batch);
            fclose(fh);
        } else {
            log_warn("%s: failed to output csv data\n", out_csv_file);
            return;
        }

        // Generate the performance profile from the CSV file
        invoke_plot_pyscript(batch, &PLOTTING_INFO[stat_idx], out_csv_file);
    }
}
