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
#include "output.h"

static const Filter DEFAULT_FILTER = ((Filter){NULL, {0, 99999}, {0, 99999}});
static const PerfProfSolver BAPCOD_SOLVER =
    ((PerfProfSolver){BAPCOD_SOLVER_NAME, {0}});

// NOTE(dparo): Global variable.
//    Unfortunately this global variable is necessary since some Unix APIs
//    that we rely on (sighandler, nftw) do not allow to pass user handles
//    to the associated callbacks
static AppCtx *G_app_ctx_ptr = NULL;

static inline SolverSolution
make_invalidated_solver_solution(PerfProfBatch *batch) {
    SolverSolution solution = {0};
    solution.status = SOLVE_STATUS_ERR;
    solution.stats[PERFPROF_STAT_KIND_PRIMAL_BOUND] =
        CRASHED_SOLVER_DEFAULT_COST_VAL;
    solution.stats[PERFPROF_STAT_KIND_TIME] = 2 * batch->timelimit;
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
           uid->seedidx, uid->hash.cstr, run->solver_name,
           run->solution.stats[PERFPROF_STAT_KIND_TIME],
           BOOL(run->solution.status & SOLVE_STATUS_CLOSED_PROBLEM),
           run->solution.stats[PERFPROF_STAT_KIND_PRIMAL_BOUND]);

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

    if (i == v->num_runs && (v->num_runs < ARRAY_LEN_i32(v->runs))) {
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

void on_proc_termination(const Process *p, int exit_status, void *user_handle) {
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
        for (int32_t i = 0; i < argidx && args[i]; i++) {
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
                                       ARRAY_LEN_i32(RANDOM_SEEDS))));

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
enum {
    MAX_NUM_BATCHES = 2048,
};

int32_t define_batches(PerfProfBatch *batches) {
    int32_t num_batches = 0;

    const char DIRPATH_FMT_TEMPLATE[] =
        "data/BAP_Instances/last-10/CVRP-scaled-%d.0/%s";
    const char *FAMILIES[] = {"A", "B", "F", "E", "P"};
    const int32_t SFACTORS[] = {1, 2, 4, 5, 8, 10, 20};

    //
    // Compare the EFL, AFL dual bounds on families E, F and scales {1, 2, 4}
    // with DEFAULT_TIME_LIMIT
    //
    {
        for (int32_t fidx = 0; fidx < ARRAY_LEN_i32(FAMILIES); fidx++) {

            const char *family = FAMILIES[fidx];

            for (int32_t sidx = 0; sidx < ARRAY_LEN_i32(SFACTORS); sidx++) {
                const int32_t scale_factor = SFACTORS[sidx];

                if (scale_factor > 4) {
                    continue;
                }

                char batch_name[256];
                char dirpath[2048];

                snprintf_safe(
                    batch_name, ARRAY_LEN(batch_name),
                    "Fractional-labeling-comparison-for-%s-scaled-%d.0", family,
                    scale_factor);

                snprintf_safe(dirpath, ARRAY_LEN(dirpath), DIRPATH_FMT_TEMPLATE,
                              scale_factor, family);

                if (num_batches < MAX_NUM_BATCHES) {
                    batches[num_batches].max_num_procs = 14;
                    batches[num_batches].name = strdup(batch_name);
                    batches[num_batches].timelimit = 60;
                    batches[num_batches].nseeds = 1;
                    batches[num_batches].dirs[0] = strdup(dirpath);
                    batches[num_batches].dirs[1] = NULL;
                    batches[num_batches].filter = DEFAULT_FILTER;

                    int32_t num_solvers = 0;
                    batches[num_batches].solvers[num_solvers++] =
                        (PerfProfSolver){"BAC MIP Pricer (EFL)",
                                         {"-DNUM_THREADS=1"}};
                    batches[num_batches].solvers[num_solvers++] =
                        (PerfProfSolver){"BAC MIP Pricer (AFL)",
                                         {"-DAMORTIZED_FRACTIONAL_LABELING=1",
                                          "-DNUM_THREADS=1"}};
                    batches[num_batches].solvers[num_solvers++] =
                        (PerfProfSolver){"BAC MIP Pricer (NFL)",
                                         {"-DDISABLE_FRACTIONAL_SEPARATION=1",
                                          "-DNUM_THREADS=1"}};
                }
                ++num_batches;
            }
        }
    }

    //
    // Compare the BAC MIP Pricer (AFL) against BapCod with DEFAULT_TIME_LIMIT
    //
    {
        for (int32_t fidx = 0; fidx < ARRAY_LEN_i32(FAMILIES); fidx++) {

            const char *family = FAMILIES[fidx];
            for (int32_t sidx = 0; sidx < ARRAY_LEN_i32(SFACTORS); sidx++) {

                const int32_t scale_factor = SFACTORS[sidx];
                if (scale_factor > 20) {
                    continue;
                }

                char batch_name[256];
                char dirpath[2048];

                snprintf_safe(batch_name, ARRAY_LEN(batch_name),
                              "%s-scaled-%d.0", family, scale_factor);

                snprintf_safe(dirpath, ARRAY_LEN(dirpath), DIRPATH_FMT_TEMPLATE,
                              scale_factor, family);

                if (num_batches < MAX_NUM_BATCHES) {
                    batches[num_batches].max_num_procs = 1;
                    batches[num_batches].name = strdup(batch_name);
                    batches[num_batches].timelimit = DEFAULT_TIME_LIMIT;
                    batches[num_batches].nseeds = 1;
                    batches[num_batches].dirs[0] = strdup(dirpath);
                    batches[num_batches].dirs[1] = NULL;
                    batches[num_batches].filter = DEFAULT_FILTER;

                    int32_t num_solvers = 0;
                    batches[num_batches].solvers[num_solvers++] =
                        (PerfProfSolver){"BAC MIP Pricer (AFL)",
                                         {"-DAMORTIZED_FRACTIONAL_LABELING=1"}};
                    batches[num_batches].solvers[num_solvers++] = BAPCOD_SOLVER;
                }
                ++num_batches;
            }
        }
    }

    return num_batches;
}

static void verify_batches_consistency(const PerfProfBatch *batches,
                                       int32_t num_batches) {

    //
    // Complain and exit if we exceed the maximum number of allowed
    // batches
    //
    if (num_batches >= MAX_NUM_BATCHES) {
        fprintf(stderr,
                "INTERNAL PERFPROF ERROR: Exceeded MAX_NUM_BATCHES "
                "= %d\n",
                MAX_NUM_BATCHES);
        abort();
    }

    //
    // Abort if there exists duplicate batches identified univocally by
    // their name
    //
    for (int32_t i = 0; i < num_batches; i++) {

        if (strchar(batches[i].name, ' ')) {
            log_fatal("Avoid spaces inside batch names. Found batch name: `%s`",
                      batches[i].name);
            abort();
        } else if (strchar(batches[i].name, '\\')) {
            log_fatal("Avoid backslash `\` inside batch names. Found batch "
                      "name: `%s`",
                      batches[i].name);
            abort();
        }

        for (int32_t j = 0; j < num_batches; j++) {
            if (i == j) {
                continue;
            }

            if (0 == strcmp(batches[i].name, batches[j].name)) {
                log_fatal("\n\nInternal perfprof error: detected "
                          "duplicate batch "
                          "names (`%s`)\n",
                          batches[i].name);
                abort();
            }
        }
    }

    // Detect presence of duplicate solver names in each batch
    for (int32_t batch_idx = 0; batch_idx < num_batches; batch_idx++) {
        const PerfProfBatch *b = &batches[batch_idx];
        for (int32_t i = 0; b->solvers[i].name; i++) {
            for (int32_t j = 0; b->solvers[j].name; j++) {
                if (i != j) {
                    if (0 == strcmp(b->solvers[i].name, b->solvers[j].name)) {
                        log_fatal("\n\nInternal perfprof error: "
                                  "detected duplicate "
                                  "name `%s` in group %s\n",
                                  b->solvers[i].name, b->name);
                        abort();
                    }
                }
            }
        }
    }
}

static void main_loop(AppCtx *ctx) {
    PerfProfBatch *batches = calloc(MAX_NUM_BATCHES, sizeof(*batches));

    // Define batches and verify consistency
    int32_t num_batches = define_batches(batches);
    verify_batches_consistency(batches, num_batches);

    for (int32_t bidx = 0; !ctx->should_terminate && bidx < num_batches;
         bidx++) {
        batches[bidx].timelimit = ceil(batches[bidx].timelimit);

        printf("\n\n");
        printf("###########################################################\n");
        printf("###########################################################\n");
        printf("###########################################################\n");
        printf("     DOING BATCH: %s\n", batches[bidx].name);
        printf("            Batch max num concurrent procs: %d\n",
               batches[bidx].max_num_procs);
        printf("            Batch timelimit: %g\n", batches[bidx].timelimit);
        printf("            Batch num seeds: %d\n", batches[bidx].nseeds);
        printf("            Batch dirs: [");
        for (int32_t dir_idx = 0;
             dir_idx < BATCH_MAX_NUM_DIRS && batches[bidx].dirs[dir_idx];
             dir_idx++) {
            printf("%s, ", batches[bidx].dirs[dir_idx]);
        }
        printf("]\n");
        printf("###########################################################\n");
        printf("###########################################################\n");
        printf("###########################################################\n");
        printf("\n\n");

        clear_perf_table(&ctx->perf_tbl);
        do_batch(ctx, &batches[bidx]);
        proc_pool_join(&ctx->pool);

        if (!ctx->should_terminate) {
            // Process the perf_table to generate the csv file
            dump_performance_profiles(ctx, &batches[bidx]);
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

    // Free the batches
    free(batches);
}

int main(int argc, char *argv[]) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    AppCtx ctx = {0};
    init(&ctx);

    G_app_ctx_ptr = &ctx;

    const int SIGNALS_TO_CATCH[] = {
        SIGTERM,
        SIGINT,
    };

    sighandler_t prev_sighandlers[ARRAY_LEN(SIGNALS_TO_CATCH)] = {0};

    // Setup signals to catch callback
    for (int32_t i = 0; i < (int)ARRAY_LEN(SIGNALS_TO_CATCH); i++) {
        prev_sighandlers[i] = signal(SIGNALS_TO_CATCH[i], my_sighandler);
    }

    // Start with the main loop
    main_loop(&ctx);

    // Restore the signals to catch previous callback
    for (int32_t i = 0; i < (int)ARRAY_LEN(SIGNALS_TO_CATCH); i++) {
        signal(SIGNALS_TO_CATCH[i], prev_sighandlers[i]);
    }

    proc_pool_join(&ctx.pool);
    return 0;
}
