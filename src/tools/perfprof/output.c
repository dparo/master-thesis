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

static inline double get_raw_stat_val_from_perf(const PerfProfRun *run,
                                                PerfProfStatKind kind) {
    return run->solution.stats[kind];
}

static inline double get_ratioed_stat_val_from_perf(const PerfProfRun *run,
                                                    PerfProfStatKind kind,
                                                    double max_ratio,
                                                    double shift) {
    double v = get_raw_stat_val_from_perf(run, kind);
    return (v + shift) / max_ratio;
}

static inline double
get_properly_encoded_stat_val_from_perf(const PerfProfRun *run,
                                        bool is_time_profile, double max_ratio,
                                        double shift) {
    if (is_time_profile) {
        return get_ratioed_stat_val_from_perf(run, PERFPROF_STAT_KIND_TIME,
                                              max_ratio, shift);
    } else {
        return get_raw_stat_val_from_perf(run, PERFPROF_STAT_KIND_PRIMAL_BOUND);
    }
}

static void generate_performance_profile_using_python_script(
    const PerfProfBatch *batch, char *csv_input_file, bool is_time_profile) {
    char *args[PROC_MAX_ARGS];
    int32_t argidx = 0;

    char title[512];

    char x_min_str[128];
    char x_max_str[128];

    char x_lower_limit_str[128];
    char x_upper_limit_str[128];
    char *xlabel_str = is_time_profile ? "TimeRatio" : "RelativeCost";

    // double shift = batch->shift > 0 ? batch->shift : 1.0;
    // double max_ratio = batch->max_ratio > 0 ? batch->max_ratio : 4.0;

    // -1e99, +1e99 for some parameters means automatically compute it
    // (zoom-to-fit)
    double x_min = 0.0;
    double x_max = 1e99;
    double x_lower_limit = -1e99;
    double x_upper_limit =
        is_time_profile ? get_extended_timelimit(batch->timelimit) : 1e99;

    Path csv_input_file_basename;
    char output_file[OS_MAX_PATH];

    snprintf_safe(output_file, ARRAY_LEN(output_file), "%s/%s_Plot.pdf",
                  os_dirname(csv_input_file, &csv_input_file_basename),
                  xlabel_str);

    snprintf_safe(x_max_str, ARRAY_LEN(x_max_str), "%g", x_max);
    snprintf_safe(x_min_str, ARRAY_LEN(x_min_str), "%g", x_min);

    snprintf_safe(x_lower_limit_str, ARRAY_LEN(x_lower_limit_str), "%g",
                  x_lower_limit);
    snprintf_safe(x_upper_limit_str, ARRAY_LEN(x_upper_limit_str), "%g",
                  x_upper_limit);

    snprintf_safe(title, ARRAY_LEN(title), "%s of %s",
                  is_time_profile ? "Time profile" : "Cost profile",
                  batch->name);

    args[argidx++] = "python3";
    args[argidx++] = PYTHON3_PERF_SCRIPT;
    args[argidx++] = "--delimiter";
    args[argidx++] = ",";
    // NOTE: This parameters make little sense now that we are encoding our
    // own custom baked data
    if (!is_time_profile) {
        args[argidx++] = "--draw-separated-regions";
    }
#if 0
    args[argidx++] = "--x-min";
    args[argidx++] = x_min_str;
    args[argidx++] = "--x-max";
    args[argidx++] = x_max_str;
    args[argidx++] = "--x-lower-limit";
    args[argidx++] = x_lower_limit_str;
    args[argidx++] = "--x-upper-limit";
    args[argidx++] = x_upper_limit_str;
#endif
    args[argidx++] = "--plot-title";
    args[argidx++] = title;
    args[argidx++] = "--startidx"; // Start index to associated with the colors
    args[argidx++] = "0";
    args[argidx++] = "--x-label"; // Start index to associated with the colors
    args[argidx++] = xlabel_str;
    args[argidx++] = "-i";
    args[argidx++] = csv_input_file;
    args[argidx++] = "-o";
    args[argidx++] = output_file;
    args[argidx++] = NULL;

    proc_spawn_sync(args);
}

void generate_perfs_imgs(const AppCtx *ctx, const PerfProfBatch *batch) {
    printf("\n\n\n");
    char dump_dir[OS_MAX_PATH];
    char data_csv_file[OS_MAX_PATH];

    os_mkdir(PERFPROF_DUMP_ROOTDIR, true);
    os_mkdir(PERFPROF_DUMP_ROOTDIR "/Plots", true);
    snprintf_safe(dump_dir, ARRAY_LEN(dump_dir),
                  PERFPROF_DUMP_ROOTDIR "/Plots/%s", batch->name);
    os_mkdir(dump_dir, true);

    for (int32_t data_dump_idx = 0; data_dump_idx < 2; data_dump_idx++) {
        bool is_time_profile = data_dump_idx == 0;
        char *filename = is_time_profile ? "time-data.csv" : "cost-data.csv";

        const double shift = is_time_profile ? 1e-3 : 1e-9;

        snprintf_safe(data_csv_file, ARRAY_LEN(data_csv_file), "%s/%s",
                      dump_dir, filename);

        FILE *fh = fopen(data_csv_file, "w");
        if (!fh) {
            log_warn("%s: failed to output csv data\n", data_csv_file);
            return;
        }

        // Compute the number of solvers in the batch
        int32_t num_solvers = 0;
        for (int32_t i = 0; batch->solvers[i].name; i++) {
            num_solvers++;
        }

        // Write out the header
        fprintf(fh, "%d", num_solvers);
        for (int32_t i = 0; i < num_solvers; i++) {
            fprintf(fh, ",%s", batch->solvers[i].name);
        }
        fprintf(fh, "\n");

        size_t tbl_len = hmlenu(ctx->perf_tbl.buf);
        for (size_t i = 0; i < tbl_len; i++) {
            const PerfTblKey *key = &ctx->perf_tbl.buf[i].key;
            const PerfTblValue *value = &ctx->perf_tbl.buf[i].value;
            assert(value->num_runs == num_solvers);

            const Hash *hash = &key->uid.hash;
            uint8_t seedidx = key->uid.seedidx;

            fprintf(fh, "%d:%s", seedidx, hash->cstr);

            //
            // Compute the min_val, max_val for this instance
            //
            double min_val = INFINITY;
            double max_val = -INFINITY;
            assert(value->num_runs == num_solvers);
            for (int32_t run_idx = 0; run_idx < value->num_runs; run_idx++) {
                const PerfProfRun *run = &value->runs[run_idx];
                double val = get_raw_stat_val_from_perf(run, is_time_profile);
                min_val = MIN(min_val, val);
                max_val = MAX(max_val, val);
            }

            // Due to the out of order generation of the performance table,
            // the perf data may be out of order, compared to the order of
            // the solvers considered when outputting the CSV file.
            // Therefore fix a solver name, and find its perf in the list,
            // to output the perf data in the expected order
            for (int32_t curr_solver_idx = 0; curr_solver_idx < num_solvers;
                 curr_solver_idx++) {
                // Fix the solver name as in order
                char *solver_name = batch->solvers[curr_solver_idx].name;

                for (int32_t run_idx = 0; run_idx < value->num_runs;
                     run_idx++) {
                    const PerfProfRun *run = &value->runs[run_idx];
                    if (0 == strcmp(run->solver_name, solver_name)) {
                        double baked_data =
                            get_properly_encoded_stat_val_from_perf(
                                run, is_time_profile, min_val, shift);
                        fprintf(fh, ",%.17g", baked_data);
                    }
                }
            }

            fprintf(fh, "\n");
        }

        fclose(fh);

        // Generate the performance profile from the CSV file
        generate_performance_profile_using_python_script(batch, data_csv_file,
                                                         is_time_profile);
    }
}
