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

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include "types.h"
#include "utils.h"
#include "misc.h"
#include "proc.h"
#include "os.h"
#include "parser.h"
#include "core-utils.h"
#include "core.h"

#include <sha256.h>
#include <cJSON.h>
#include <ftw.h>

#include "json-loader.h"
#include "hashing.h"

static const Filter DEFAULT_FILTER = ((Filter){NULL, {0, 99999}, {0, 99999}});
static const PerfProfSolver BAPCOD_SOLVER =
    ((PerfProfSolver){BAPCOD_SOLVER_NAME, {0}});

// NOTE(dparo): Global variable.
//    Unfortunately this global variable is necesary since some Unix APIs
//    that we rely on (sighandler, nftw) do not allow to pass user handles
//    to the associated callbacks
static AppCtx *G_app_ctx_ptr = NULL;

static inline SolverSolution
make_invalidated_solver_solution(PerfProfBatch *batch) {
    SolverSolution solution = {0};
    solution.status = SOLVE_STATUS_ERR;
    solution.primal_bound = CRASHED_SOLVER_DEFAULT_COST_VAL;
    solution.time = 2 * batch->timelimit;
    return solution;
}

static inline PerfProfRun make_solver_run(PerfProfBatch *batch,
                                          char *solver_name) {
    PerfProfRun run = {0};
    strncpy_safe(run.solver_name, solver_name, ARRAY_LEN(run.solver_name));
    run.solution = make_invalidated_solver_solution(batch);
    return run;
}

void store_perfprof_run(PerfTbl *tbl, PerfProfInputUniqueId *uid,
                        PerfProfRun *run) {
    printf("Inserting run into table. Instance hash: %d:%s. Run ::: "
           "solver_name = %s, "
           "time = %.17g, closedProblem = %d, obj_ub "
           "= %.17g\n",
           uid->seedidx, uid->hash.cstr, run->solver_name, run->solution.time,
           BOOL(run->solution.status & SOLVE_STATUS_CLOSED_PROBLEM),
           run->solution.primal_bound);

    PerfTblKey key = {0};
    memcpy(&key.uid, uid, sizeof(key.uid));

    if (!hmgetp_null(tbl->buf, key)) {
        PerfTblValue zero_value = {0};
        hmput(tbl->buf, key, zero_value);
    }

    PerfTblValue *v = NULL;
    {
        PerfTblEntry *t = hmgetp(tbl->buf, key);
        assert(t);
        if (t) {
            v = &t->value;
        }
    }

    if (!v) {
        return;
    }

    int32_t i = 0;
    for (i = 0; i < v->num_runs; i++) {
        if (0 == strcmp(run->solver_name, v->runs[i].solver_name)) {
            // Need to update previous perf
            // BUT... This is an invalid case to happen
            assert(0);
            break;
        }
    }

    if (i == v->num_runs && (v->num_runs < (int32_t)ARRAY_LEN(v->runs))) {
        v->num_runs++;
    } else {
        assert(0);
    }

    if (i < v->num_runs) {
        memcpy(&v->runs[i], run, MIN(sizeof(v->runs[i]), sizeof(*run)));
    } else {
        log_fatal("Bad internal error. Too much solvers specified in the same "
                  "batch, or internal bug!!");
        assert(0);
        abort();
    }
}

void clear_perf_table(PerfTbl *tbl) {
    if (tbl->buf) {
        hmfree(tbl->buf);
    }
    tbl->buf = NULL;
}

static void my_sighandler(int signum) {
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
        G_app_ctx_ptr->should_terminate = true;
        G_app_ctx_ptr->pool.aborted = true;
    }
}

static void
update_perf_tbl_with_cptp_json_perf_data(AppCtx *ctx,
                                         PerfProfRunHandle *handle) {
    PerfProfRun run = make_solver_run(ctx->current_batch, handle->solver_name);
    if (handle->json_output_path[0] != '\0') {
        cJSON *root = load_json(handle->json_output_path);
        if (root) {
            parse_cptp_solver_json_dump(&run, root);
            cJSON_Delete(root);
        }
    }
    store_perfprof_run(&ctx->perf_tbl, &handle->input.uid, &run);
}

static void update_perf_tbl_with_bapcod_json_perf_data(
    AppCtx *ctx, PerfProfRunHandle *handle, char *json_filepath) {
    PerfProfRun run = make_solver_run(ctx->current_batch, handle->solver_name);
    if (json_filepath) {
        cJSON *root = load_json(json_filepath);
        if (root) {
            parse_bapcod_solver_json_dump(&run, root);
            cJSON_Delete(root);
        }
    }
    store_perfprof_run(&ctx->perf_tbl, &handle->input.uid, &run);
}

void on_proc_termination(Process *p, int exit_status, void *user_handle) {
    AppCtx *ctx = G_app_ctx_ptr;

    if (!user_handle) {
        return;
    }

    PerfProfRunHandle *handle = user_handle;
    if (p) {
        if (exit_status == 0) {
            update_perf_tbl_with_cptp_json_perf_data(ctx, handle);
        } else {
            log_warn("\n\n\nSolver `%s` returned with non 0 exit status. Got "
                     "%d\n\n\n",
                     handle->solver_name, exit_status);
            PerfProfRun run =
                make_solver_run(ctx->current_batch, handle->solver_name);
            store_perfprof_run(&ctx->perf_tbl, &handle->input.uid, &run);
        }
    }

    free(handle);
}

static void handle_bapcod_solver_run(AppCtx *ctx, PerfProfRunHandle *handle) {
    Path instance_filepath_dirname;
    Path instance_filepath_basename;
    char *dirname =
        os_dirname(handle->input.filepath, &instance_filepath_dirname);
    char *basename =
        os_basename(handle->input.filepath, &instance_filepath_basename);

    char json_output_file[OS_MAX_PATH];

    snprintf_safe(json_output_file, ARRAY_LEN(json_output_file), "%s/%.*s.json",
                  dirname, (int)(os_get_fext(basename) - basename - 1),
                  basename);

    if (!os_fexists(json_output_file)) {
        log_warn("%s: BapCod JSON output file does not exist!!!\n",
                 json_output_file);
        update_perf_tbl_with_bapcod_json_perf_data(ctx, handle, NULL);
    } else {
        update_perf_tbl_with_bapcod_json_perf_data(ctx, handle,
                                                   json_output_file);
    }
}

static void prep_unique_cptp_json_output_file(PerfProfRunHandle *handle,
                                              PerfProfInput *input) {
    snprintf_safe(handle->json_output_path, ARRAY_LEN(handle->json_output_path),
                  "%s/%s/%s", PERFPROF_DUMP_ROOTDIR, "cache",
                  input->instance_name);

    os_mkdir(handle->json_output_path, true);

    snprintf_safe(handle->json_output_path + strlen(handle->json_output_path),
                  ARRAY_LEN(handle->json_output_path) -
                      strlen(handle->json_output_path),
                  "/%d:%s", input->uid.seedidx, input->uid.hash.cstr);

    os_mkdir(handle->json_output_path, true);

    snprintf_safe(handle->json_output_path + strlen(handle->json_output_path),
                  ARRAY_LEN(handle->json_output_path) -
                      strlen(handle->json_output_path),
                  "/%s.json", handle->run_hash.cstr);
}

static double get_extended_timelimit(double timelimit) {
    return ceil(1.05 * timelimit + 2);
}

static double get_kill_timelimit(double timelimit) {
    return ceil(1.05 * get_extended_timelimit(timelimit));
}

PerfProfRunHandle *new_perfprof_run_handle(const Hash *exe_hash,
                                           const PerfProfInput *input,
                                           char *args[PROC_MAX_ARGS],
                                           int32_t num_args,
                                           const PerfProfSolver *solver) {
    PerfProfRunHandle *handle = malloc(sizeof(*handle));
    handle->run_hash = compute_run_hash(exe_hash, input, args, num_args);
    handle->input.seed = input->seed;
    snprintf_safe(handle->solver_name, ARRAY_LEN(handle->solver_name), "%s",
                  solver->name);
    snprintf_safe(handle->input.filepath, ARRAY_LEN(handle->input.filepath),
                  "%s", input->filepath);

    handle->input.uid.seedidx = input->uid.seedidx;
    strncpy_safe(handle->input.uid.hash.cstr, input->uid.hash.cstr,
                 ARRAY_LEN(handle->input.uid.hash.cstr));
    return handle;
}

static void handle_cptp_solver_run(AppCtx *ctx, PerfProfSolver *solver,
                                   PerfProfInput *input) {
    if (ctx->should_terminate) {
        return;
    }

    char *args[PROC_MAX_ARGS] = {0};
    int32_t argidx = 0;

    char timelimit[128];
    snprintf_safe(timelimit, ARRAY_LEN(timelimit), "%g",
                  ctx->current_batch->timelimit);

    char timelimit_extended[128];
    snprintf_safe(timelimit_extended, ARRAY_LEN(timelimit_extended), "%g",
                  get_extended_timelimit(ctx->current_batch->timelimit));

    char killafter[128];
    snprintf_safe(killafter, ARRAY_LEN(killafter), "%g",
                  get_kill_timelimit(ctx->current_batch->timelimit) -
                      ctx->current_batch->timelimit);

    char seed_str[128];
    snprintf_safe(seed_str, ARRAY_LEN(seed_str), "%d", input->seed);

    args[argidx++] = "timeout";
    args[argidx++] = "-k";
    args[argidx++] = killafter;
    args[argidx++] = timelimit_extended;
    args[argidx++] = CPTP_EXE;
    args[argidx++] = "-a"; // Treat abort as failure
    args[argidx++] = "-t";
    args[argidx++] = timelimit;
    args[argidx++] = "--seed";
    args[argidx++] = seed_str;
    args[argidx++] = "-DHEUR_PRICER_MODE=0";
    args[argidx++] = "-DAPPLY_UPPER_CUTOFF=1";

    for (int32_t i = 0; solver->args[i] != NULL; i++) {
        args[argidx++] = solver->args[i];
    }

    PerfProfRunHandle *handle = new_perfprof_run_handle(
        &ctx->cptp_exe_hash, input, args, argidx, solver);

    prep_unique_cptp_json_output_file(handle, input);

    args[argidx++] = "-i";
    args[argidx++] = (char *)input->filepath;
    args[argidx++] = "-w";
    args[argidx++] = (char *)handle->json_output_path;
    args[argidx++] = NULL;

    //
    // Check if the JSON output is already cached on disk
    //
    if (os_fexists(handle->json_output_path)) {
        printf("Found cache for hash %s. CMD:", handle->run_hash.cstr);
        for (int32_t i = 0; args[i] && i < argidx; i++) {
            printf(" %s", args[i]);
        }
        printf("\n");
        update_perf_tbl_with_cptp_json_perf_data(ctx, handle);
        free(handle);
    } else {
        proc_pool_queue(&ctx->pool, handle, args);
    }
}

void handle_vrp_instance(AppCtx *ctx, PerfProfInput *input) {
    if (ctx->should_terminate) {
        return;
    }

    for (int32_t solver_idx = 0;
         !ctx->should_terminate &&
         (ctx->current_batch->solvers[solver_idx].name != NULL);
         solver_idx++) {
        PerfProfSolver *solver = &ctx->current_batch->solvers[solver_idx];

        if (0 == strcmp(solver->name, BAPCOD_SOLVER_NAME) &&
            solver->args[0] == NULL) {
            PerfProfRunHandle *handle = new_perfprof_run_handle(
                &ctx->bapcod_virtual_exe_hash, input, NULL, 0, solver);

            handle_bapcod_solver_run(ctx, handle);
            free(handle);
        } else {
            handle_cptp_solver_run(ctx, solver, input);
            if (ctx->pool.max_num_procs == 1) {
                proc_pool_join(&ctx->pool);
            }
        }
    }
}

static bool is_filtered_instance(Filter *f, const Instance *instance) {
    if (instance->num_customers < f->ncustomers.a ||
        instance->num_customers > f->ncustomers.b) {
        return true;
    } else if (instance->num_vehicles < f->nvehicles.a ||
               instance->num_vehicles > f->nvehicles.b) {
        return true;
    }
    return false;
}

int file_walk_cb(const char *fpath, const struct stat *sb, int typeflag,
                 struct FTW *ftwbuf) {
    UNUSED_PARAM(sb);
    UNUSED_PARAM(ftwbuf);

    AppCtx *ctx = G_app_ctx_ptr;

    if (typeflag == FTW_F || typeflag == FTW_SL) {
        // Is a regular file
        const char *ext = os_get_fext(fpath);
        if (ext && (0 == strcmp(ext, "vrp"))) {
            // printf("Found file: %s\n", fpath);

            Instance instance = parse(fpath);
            if (is_valid_instance(&instance)) {
                Filter *filter = &ctx->current_batch->filter;
                if (!is_filtered_instance(filter, &instance)) {

                    PerfProfInput input = {0};
                    strncpy_safe(input.filepath, fpath,
                                 ARRAY_LEN(input.filepath));

                    strncpy_safe(input.instance_name, instance.name,
                                 ARRAY_LEN(input.instance_name));

                    input.uid.hash = hash_instance(&instance);

                    printf("--- instance_hash :: computed_hash = %s\n",
                           input.uid.hash.cstr);

                    const uint8_t num_seeds = (uint8_t)(MIN(
                        UINT8_MAX, MIN(ctx->current_batch->nseeds,
                                       (int32_t)ARRAY_LEN(RANDOM_SEEDS))));

                    for (uint8_t seedidx = 0;
                         seedidx < num_seeds && !ctx->should_terminate;
                         seedidx++) {
                        input.uid.seedidx = seedidx;

                        input.seed = RANDOM_SEEDS[seedidx];
                        handle_vrp_instance(ctx, &input);
                    }
                } else {
                    printf("%s: Skipping since it does not match filter\n",
                           fpath);
                }
                instance_destroy(&instance);
            } else {
                log_fatal("%s: Failed to parse input file\n", fpath);
                exit(EXIT_FAILURE);
            }
        }
    } else if (typeflag == FTW_D) {
        printf("Found dir: %s\n", fpath);
    }

    return ctx->should_terminate ? FTW_STOP : FTW_CONTINUE;
}

void batch_scan_dir_and_solve(AppCtx *ctx, const char *dirpath) {
    if (!dirpath || dirpath[0] == 0) {
        return;
    }

    int result = nftw(dirpath, file_walk_cb, 8, 0);
    if (result == FTW_STOP) {
        proc_pool_join(&ctx->pool);
        printf("Requested to stop scanning dirpath %s\n", dirpath);
    } else if (result != 0) {
        perror("nftw walk");
        exit(EXIT_FAILURE);
    }
}

static void do_batch(AppCtx *ctx, PerfProfBatch *current_batch) {
    proc_pool_join(&ctx->pool);
    ctx->current_batch = current_batch;
    ctx->pool.max_num_procs = current_batch->max_num_procs;
    ctx->pool.on_async_proc_exit = on_proc_termination;

    // Adjust zero-initialized filters
    {
        Filter *filter = &current_batch->filter;
        if (filter->ncustomers.a >= 0 && filter->ncustomers.b == 0) {
            filter->ncustomers.b = 99999;
        }

        if (filter->nvehicles.a >= 0 && filter->nvehicles.b == 0) {
            filter->nvehicles.b = 99999;
        }
    }

    // Detect duplicate names in solver names
    for (int32_t i = 0; current_batch->solvers[i].name; i++) {
        for (int32_t j = 0; current_batch->solvers[j].name; j++) {
            if (i != j) {
                if (0 == strcmp(current_batch->solvers[i].name,
                                current_batch->solvers[j].name)) {
                    log_fatal("\n\nInternal perfprof error: detected duplicate "
                              "name `%s` in group %s\n",
                              current_batch->solvers[i].name,
                              current_batch->name);
                    abort();
                }
            }
        }
    }

    if (current_batch->nseeds > 0) {
        if (!ctx->should_terminate) {
            for (int32_t dir_idx = 0;
                 dir_idx < BATCH_MAX_NUM_DIRS && current_batch->dirs[dir_idx];
                 dir_idx++) {
                batch_scan_dir_and_solve(ctx, current_batch->dirs[dir_idx]);
            }
        }
    }
}

static void init(AppCtx *ctx) {
    G_app_ctx_ptr = ctx;

    os_mkdir(PERFPROF_DUMP_ROOTDIR, true);
    os_mkdir(PERFPROF_DUMP_ROOTDIR "/cache", true);

    // Compute the cptp exe hash
    {
        SHA256_CTX shactx;

        sha256_init(&shactx);
        sha256_update(&shactx, (BYTE *)CPTP_EXE, strlen(CPTP_EXE));
        sha256_finalize_to_cstr(&shactx, &ctx->cptp_exe_hash);
    }
    // Compute the BAPCOD exe hash
    {
        SHA256_CTX shactx;

        sha256_init(&shactx);
        sha256_update(&shactx, (BYTE *)BAPCOD_SOLVER_NAME,
                      strlen(BAPCOD_SOLVER_NAME));
        sha256_finalize_to_cstr(&shactx, &ctx->bapcod_virtual_exe_hash);
    }
}

static void generate_perfs_imgs(AppCtx *ctx, PerfProfBatch *batch);

static void main_loop(AppCtx *ctx) {
    enum {
        MAX_NUM_BATCHES = 1024,
        STRING_BUF_SIZE = 1024,
    };

    PerfProfBatch *batches = calloc(MAX_NUM_BATCHES, sizeof(*batches));

    int32_t num_batches = 0;
    const char *FAMILIES[] = {"F", "E"};
    const char *SCALE_FACTORS[] = {
        "1.0", "2.0", "4.0",
        //"5.0", "8.0", "10.0", "20.0"
    };

    for (int32_t family_idx = 0; family_idx < (int32_t)ARRAY_LEN(FAMILIES);
         family_idx++) {
        for (int32_t scale_factor_idx = 0;
             scale_factor_idx < (int32_t)ARRAY_LEN(SCALE_FACTORS);
             scale_factor_idx++) {

            if (num_batches == MAX_NUM_BATCHES) {
                fprintf(
                    stderr,
                    "INTERNAL PERFPROF ERROR: Exceeded MAX_NUM_BATCHES = %d\n",
                    MAX_NUM_BATCHES);
                exit(1);
            }

            char batch_name[STRING_BUF_SIZE];
            char dirpath[STRING_BUF_SIZE];

            snprintf_safe(batch_name, ARRAY_LEN(batch_name),
                          "%s-scaled-%s-last-10", FAMILIES[family_idx],
                          SCALE_FACTORS[scale_factor_idx]);

            snprintf_safe(dirpath, ARRAY_LEN(dirpath),
                          "data/BAP_Instances/last-10/CVRP-scaled-%s/%s",
                          SCALE_FACTORS[scale_factor_idx],
                          FAMILIES[family_idx]);

            batches[num_batches].max_num_procs = 1;
            batches[num_batches].name = strdup(batch_name);
            batches[num_batches].timelimit = DEFAULT_TIME_LIMIT;
            batches[num_batches].nseeds = 1;
            batches[num_batches].dirs[0] = strdup(dirpath);
            batches[num_batches].dirs[1] = NULL;
            batches[num_batches].filter = DEFAULT_FILTER;

            batches[num_batches].solvers[0] =
                (PerfProfSolver){"BAC MIP Pricer", {}};
            batches[num_batches].solvers[1] = BAPCOD_SOLVER;
            ++num_batches;
        }
    }

    for (int32_t i = 0; i < num_batches; i++) {
        for (int32_t j = 0; j < num_batches; j++) {
            if (i == j) {
                continue;
            }

            if (0 == strcmp(batches[i].name, batches[j].name)) {
                log_fatal(
                    "\n\nInternal perfprof error: detected duplicate batch "
                    "names (`%s`)\n",
                    batches[i].name);
                abort();
            }
        }
    }

    for (int32_t i = 0; !ctx->should_terminate && i < num_batches; i++) {
        batches[i].timelimit = ceil(batches[i].timelimit);

        printf("\n\n");
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("     DOING BATCH: %s\n", batches[i].name);
        printf("            Batch max num concurrent procs: %d\n",
               batches[i].max_num_procs);
        printf("            Batch timelimit: %g\n", batches[i].timelimit);
        printf("            Batch num seeds: %d\n", batches[i].nseeds);
        printf("            Batch dirs: [");
        for (int32_t dir_idx = 0;
             dir_idx < BATCH_MAX_NUM_DIRS && batches[i].dirs[dir_idx];
             dir_idx++) {
            printf("%s, ", batches[i].dirs[dir_idx]);
        }
        printf("]\n");
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("\n\n");

        clear_perf_table(&ctx->perf_tbl);
        do_batch(ctx, &batches[i]);
        proc_pool_join(&ctx->pool);

        if (!ctx->should_terminate) {
            // Process the perf_table to generate the csv file
            generate_perfs_imgs(ctx, &batches[i]);
        }

        clear_perf_table(&ctx->perf_tbl);
    }

    proc_pool_join(&ctx->pool);
    clear_perf_table(&ctx->perf_tbl);

    // Free the temporary memory data associated with each batch
    for (int32_t i = 0; i < num_batches; i++) {
        free(batches[i].name);
        for (int32_t dir_idx = 0;
             dir_idx < BATCH_MAX_NUM_DIRS && batches[i].dirs[dir_idx];
             dir_idx++) {
            free(batches[i].dirs[dir_idx]);
        }
    }
    free(batches);
}

int main(int argc, char *argv[]) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    AppCtx ctx;
    init(&ctx);
    sighandler_t prev_sigterm_handler = signal(SIGTERM, my_sighandler);
    sighandler_t prev_sigint_handler = signal(SIGINT, my_sighandler);
    { main_loop(&ctx); }
    signal(SIGTERM, prev_sigterm_handler);
    signal(SIGINT, prev_sigint_handler);
    proc_pool_join(&ctx.pool);
    return 0;
}

static void generate_performance_profile_using_python_script(
    PerfProfBatch *batch, char *csv_input_file, bool is_time_profile) {
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

static inline double get_costval_for_csv(double costval, double base_ref_val,
                                         double shift) {
    return fratio(base_ref_val, costval, shift);
}

static inline double get_timeval_for_csv(double timeval, double base_ref_val,
                                         double shift) {
    return (timeval + shift) / base_ref_val;
}

static inline double get_raw_val_from_perf(const PerfProfRun *run,
                                           bool is_time_profile) {
    return is_time_profile ? run->solution.time : run->solution.primal_bound;
}

static inline double get_baked_val_from_perf(const PerfProfRun *run,
                                             bool is_time_profile,
                                             double base_ref_val,
                                             double shift) {
    double val = get_raw_val_from_perf(run, is_time_profile);
    if (is_time_profile) {
        return get_timeval_for_csv(val, base_ref_val, shift);
    } else {
        return get_raw_val_from_perf(run, is_time_profile);
        // return get_costval_for_csv(val, base_ref_val, shift);
    }
}

static void generate_perfs_imgs(AppCtx *ctx, PerfProfBatch *batch) {
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

        const double shift = is_time_profile ? 1e-4 : 1e-4;

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
                double val = get_raw_val_from_perf(run, is_time_profile);
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
                        double baked_data = get_baked_val_from_perf(
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
