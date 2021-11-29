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

#include "utils.h"
#include "misc.h"
#include "proc.h"
#include "os.h"
#include "parser.h"
#include "core-utils.h"

#include <ftw.h>

#include <sha256.h>
#include <cJSON.h>

typedef struct {
    char cstr[65];
} Hash;

typedef struct {
    int32_t a, b;
} int32_interval_t;

typedef struct {
    char *family; /// TODO: Not supported yet
    int32_interval_t ncustomers;
    int32_interval_t nvehicles;
} Filter;

typedef struct {
    char *name;
    char *args[PROC_MAX_ARGS];
} PerfProfSolver;

typedef struct {
    double time;
    double cost;
} Perf;

typedef struct {
    uint8_t seedidx;
    Hash hash;
} PerfProfInputUniqueId;

typedef struct {
    char instance_name[256];
    char filepath[OS_MAX_PATH];
    PerfProfInputUniqueId uid;
    int32_t seed;
} PerfProfInput;

typedef struct {
    char solver_name[48];
    PerfProfInput input;
    Hash run_hash;
    char json_output_path[OS_MAX_PATH + 32];
} PerfProfRunHandle;

/// NOTE: This struct should remain as packed and as small as possible,
///       since it will be the main cause of memory consumption in
///       this program. It will be stored in a hashmap and will live in memory
///       for the entire duration of the current batch.
typedef struct {
    char solver_name[48];
    Perf perf;
} PerfProfRun;

#define MAX_NUM_SOLVERS_PER_BATCH 8

typedef struct {
    int32_t num_runs;
    PerfProfRun runs[MAX_NUM_SOLVERS_PER_BATCH];
} PerfTblValue;

typedef struct {
    PerfProfInputUniqueId uid;
} PerfTblKey;

typedef struct {
    PerfTblKey key;
    PerfTblValue value;
} PerfTblEntry;

typedef struct {
    int32_t max_num_procs;
    char *name;
    double timelimit;
    int32_t nseeds;
    const char *scan_root_dir;
    Filter filter;
    PerfProfSolver solvers[MAX_NUM_SOLVERS_PER_BATCH];
} PerfProfBatch;

#ifndef NDEBUG
#define CPTP_EXE "./build/Debug/src/cptp"
#else
#define CPTP_EXE "./build/Release/src/cptp"
#endif

#define PYTHON3_PERF_SCRIPT "./src/tools/perfprof/perfprof.py"
#define BAPCOD_SOLVER_NAME "BaPCod"
#define PERFPROF_DUMP_ROOTDIR "perfprof-dump"

#define SHA256_UPDATE_WITH_VAR(shactx, var)                                    \
    do {                                                                       \
        sha256_update((shactx), (const BYTE *)(&(var)), sizeof(var));          \
    } while (0)

#define SHA256_UPDATE_WITH_ARRAY(shactx, array, num_elems)                     \
    do {                                                                       \
        sha256_update((shactx), (const BYTE *)(array),                         \
                      (num_elems) * sizeof(*(array)));                         \
    } while (0)

static Hash G_cptp_exe_hash;
static bool G_should_terminate;
static ProcPool G_pool = {0};
static PerfProfBatch *G_active_batch = NULL;
static PerfTblEntry *G_perftbl = NULL;

static const Filter DEFAULT_FILTER = ((Filter){NULL, {0, 99999}, {0, 99999}});
static const PerfProfSolver BAPCOD_SOLVER =
    ((PerfProfSolver){BAPCOD_SOLVER_NAME, {0}});

// 100 Random integer numbers from [0, 32767] range generated from
// https://www.random.org/integers/
static const int32_t RANDOM_SEEDS[] = {
    8111,  9333,  16884, 2228,  20278, 22042, 18309, 15176, 19175, 21292,
    12903, 19891, 6359,  14333, 27486, 12791, 31021, 855,   2552,  8691,
    12612, 11744, 15720, 20122, 401,   21650, 7144,  21018, 28549, 2660,
    10504, 2060,  1374,  11723, 10932, 21808, 22998, 23168, 31770, 7616,
    26891, 8217,  31272, 28626, 29539, 6930,  29356, 2885,  24150, 15753,
    15869, 6260,  13922, 23839, 27864, 820,   2392,  15204, 10215, 16686,
    26072, 18447, 6101,  5554,  6739,  23735, 31277, 12123, 363,   4562,
    12773, 18146, 22084, 14991, 23488, 5131,  27575, 31055, 25576, 28122,
    32632, 21942, 18007, 11716, 13917, 31899, 15279, 23520, 8192,  24349,
    13567, 32028, 15076, 6717,  1311,  20275, 5547,  5904,  7098,  4718,
};

STATIC_ASSERT(ARRAY_LEN(RANDOM_SEEDS) < UINT8_MAX,
              "Too much number of seeds. Need to be able to encode a seed index"
              "using an uint8_t");

static inline Perf make_invalidated_perf(PerfProfBatch *batch) {
    Perf perf = {0};
    perf.cost = INFINITY;
    perf.time = 2 * batch->timelimit;
    return perf;
}

static inline PerfProfRun make_solver_run(PerfProfBatch *batch,
                                          char *solver_name) {
    PerfProfRun run = {0};
    strncpy_safe(run.solver_name, solver_name, ARRAY_LEN(run.solver_name));
    run.perf = make_invalidated_perf(batch);
    return run;
}

void insert_run_into_table(PerfProfInputUniqueId *uid, PerfProfRun *run) {
    printf("Inserting run into table. Instance hash: %d:%s. Run ::: "
           "solver_name = %s, "
           "time = %.17g, obj_ub "
           "= %.17g\n",
           uid->seedidx, uid->hash.cstr, run->solver_name, run->perf.time,
           run->perf.cost);

    PerfTblKey key = {0};
    memcpy(&key.uid, uid, sizeof(key.uid));

    if (!hmgetp_null(G_perftbl, key)) {
        PerfTblValue zero_value = {0};
        hmput(G_perftbl, key, zero_value);
    }

    PerfTblValue *v = NULL;
    {
        PerfTblEntry *t = hmgetp(G_perftbl, key);
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

void clear_perf_table(void) {
    if (G_perftbl) {
        hmfree(G_perftbl);
    }
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
        G_should_terminate = true;
        G_pool.aborted = true;
    }
}

void extract_perf_data_from_cptp_json_file(PerfProfRun *run, cJSON *root) {
    cJSON *itm_took = cJSON_GetObjectItemCaseSensitive(root, "took");
    cJSON *itm_cost = cJSON_GetObjectItemCaseSensitive(root, "cost");

    if (itm_took && cJSON_IsNumber(itm_took)) {
        run->perf.time = cJSON_GetNumberValue(itm_took);
    }
    if (itm_cost && cJSON_IsNumber(itm_cost)) {
        run->perf.cost = cJSON_GetNumberValue(itm_cost);
    }
}

void update_perf_tbl_with_cptp_json_perf_data(PerfProfRunHandle *handle) {
    PerfProfRun run = make_solver_run(G_active_batch, handle->solver_name);

    char *contents =
        fread_all_into_null_terminated_string(handle->json_output_path, NULL);
    if (!contents) {
        log_warn("Failed to load JSON contents from `%s`\n",
                 handle->json_output_path);
    } else if (contents && contents[0] != '\0') {
        cJSON *root = cJSON_Parse(contents);
        if (!root) {
            log_warn("Failed to parse JSON contents from `%s`\n",
                     handle->json_output_path);
        } else {
            extract_perf_data_from_cptp_json_file(&run, root);
            cJSON_Delete(root);
        }
    }

    if (contents) {
        free(contents);
    }

    insert_run_into_table(&handle->input.uid, &run);
}

void on_async_proc_exit(Process *p, int exit_status, void *user_handle) {
    if (!user_handle) {
        return;
    }

    PerfProfRunHandle *handle = user_handle;
    if (p) {
        if (exit_status == 0) {
            update_perf_tbl_with_cptp_json_perf_data(handle);
        } else {
            log_warn("\n\n\nSolver `%s` returned with non 0 exit status. Got "
                     "%d\n\n\n",
                     handle->solver_name, exit_status);
            PerfProfRun run =
                make_solver_run(G_active_batch, handle->solver_name);
            insert_run_into_table(&handle->input.uid, &run);
        }
    }

    free(handle);
}

static void sha256_hash_finalize(SHA256_CTX *shactx, Hash *hash) {

    BYTE bytes[32];

    sha256_final(shactx, bytes);

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(bytes); i++) {
        snprintf_safe(hash->cstr + 2 * i, 65 - 2 * i, "%02x", bytes[i]);
    }

    hash->cstr[ARRAY_LEN(hash->cstr) - 1] = 0;
}

static Hash hash_instance(const Instance *instance) {
    SHA256_CTX shactx;
    sha256_init(&shactx);

    SHA256_UPDATE_WITH_VAR(&shactx, instance->num_customers);
    SHA256_UPDATE_WITH_VAR(&shactx, instance->num_vehicles);
    SHA256_UPDATE_WITH_VAR(&shactx, instance->vehicle_cap);

    int32_t n = instance->num_customers + 1;

    if (instance->positions) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->positions, n);
    }

    if (instance->demands) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->demands, n);
    }

    if (instance->demands) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->demands, n);
    }

    if (instance->duals) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->duals, n);
    }

    if (instance->edge_weight) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->edge_weight,
                                 hm_nentries(n));
    }

    Hash result = {0};
    sha256_hash_finalize(&shactx, &result);
    return result;
}

static void sha256_hash_file_contents(const char *fpath, Hash *hash) {

    SHA256_CTX shactx;
    sha256_init(&shactx);
    size_t len = 0;
    char *contents = fread_all_into_null_terminated_string(fpath, &len);
    if (contents) {
        sha256_update(&shactx, (const BYTE *)contents, len);
    } else {
        log_fatal("%s: Failed to hash (sha256) file contents\n", fpath);
        abort();
    }
    sha256_hash_finalize(&shactx, hash);
    free(contents);
}

Hash compute_run_hash(const Hash *exe_hash, const PerfProfInput *input,
                      char *args[PROC_MAX_ARGS], int32_t num_args) {
    assert(input);

    SHA256_CTX shactx;
    sha256_init(&shactx);

    for (int32_t i = 0; i < num_args; i++) {
        sha256_update(&shactx, (const BYTE *)(&args[i][0]), strlen(args[i]));
    }

    if (exe_hash) {
        sha256_update(&shactx, (const BYTE *)(&exe_hash->cstr[0]), 64);
    }

    sha256_update(&shactx, (const BYTE *)(&input->uid.seedidx),
                  sizeof(input->uid.seedidx));
    sha256_update(&shactx, (const BYTE *)(&input->uid.hash.cstr[0]), 64);

    Hash result = {0};
    sha256_hash_finalize(&shactx, &result);
    return result;
}

void extract_perf_data_from_bapcod_json_file(PerfProfRun *run, cJSON *root) {
    cJSON *rcsp_infos = cJSON_GetObjectItemCaseSensitive(root, "rcsp-infos");
    if (rcsp_infos && cJSON_IsObject(rcsp_infos)) {

        cJSON *columns_cost =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "columns-cost");

        cJSON *itm_took =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "seconds");

        if (itm_took && cJSON_IsNumber(itm_took)) {
            run->perf.time = cJSON_GetNumberValue(itm_took);
        }

        if (columns_cost && cJSON_IsArray(columns_cost)) {
            cJSON *elem = NULL;
            int32_t num_elems = 0;
            cJSON_ArrayForEach(elem, columns_cost) { num_elems += 1; }

            if (num_elems == 1) {
                cJSON_ArrayForEach(elem, columns_cost) {
                    cJSON *itm_cost = elem;
                    if (itm_cost && cJSON_IsNumber(itm_cost)) {
                        run->perf.cost = cJSON_GetNumberValue(itm_cost);
                    }
                    break;
                }
            }
        }
    }
}

static void
update_perf_tbl_with_bapcod_json_perf_data(PerfProfRunHandle *handle,
                                           char *json_filepath) {
    PerfProfRun run = make_solver_run(G_active_batch, handle->solver_name);

    if (json_filepath) {
        char *contents =
            fread_all_into_null_terminated_string(json_filepath, NULL);
        if (contents && contents[0] != '\0') {
            cJSON *root = cJSON_Parse(contents);
            if (root) {
                extract_perf_data_from_bapcod_json_file(&run, root);
                cJSON_Delete(root);
            }

            free(contents);
        }
    }
    insert_run_into_table(&handle->input.uid, &run);
}

static void handle_bapcod_solver(PerfProfRunHandle *handle) {
    Path instance_filepath_dirname;
    Path instance_filepath_basename;
    char *dirname =
        os_dirname(handle->input.filepath, &instance_filepath_dirname);
    char *basename =
        os_basename(handle->input.filepath, &instance_filepath_basename);

    char json_output_file[OS_MAX_PATH];

    snprintf_safe(json_output_file, ARRAY_LEN(json_output_file),
                  "%s/%.*s.info.json", dirname,
                  (int)(os_get_fext(basename) - basename - 1), basename);

    if (!os_fexists(json_output_file)) {
        log_warn("%s: BapCod JSON output file does not exist!!!\n",
                 json_output_file);
        update_perf_tbl_with_bapcod_json_perf_data(handle, NULL);
    } else {
        update_perf_tbl_with_bapcod_json_perf_data(handle, json_output_file);
    }
}

static void init_handle_json_output_path(PerfProfRunHandle *handle,
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

static void run_cptp_solver(PerfProfSolver *solver, PerfProfInput *input) {
    if (G_should_terminate) {
        return;
    }

    char *args[PROC_MAX_ARGS] = {0};
    int32_t argidx = 0;

    char timelimit[128];
    snprintf_safe(timelimit, ARRAY_LEN(timelimit), "%g",
                  G_active_batch->timelimit);

    char timelimit_extended[128];
    snprintf_safe(timelimit_extended, ARRAY_LEN(timelimit_extended), "%g",
                  G_active_batch->timelimit * 1.05 + 2);

    char killafter[128];
    snprintf_safe(killafter, ARRAY_LEN(killafter), "%g",
                  G_active_batch->timelimit * 1.05 + 2 -
                      G_active_batch->timelimit);

    char seed_str[128];
    snprintf_safe(seed_str, ARRAY_LEN(seed_str), "%d", input->seed);

    args[argidx++] = "timeout";
    args[argidx++] = "-k";
    args[argidx++] = killafter;
    args[argidx++] = timelimit_extended;
    args[argidx++] = CPTP_EXE;
    args[argidx++] = "-t";
    args[argidx++] = timelimit;
    args[argidx++] = "--seed";
    args[argidx++] = seed_str;
    args[argidx++] = "-i";
    args[argidx++] = (char *)input->filepath;

    for (int32_t i = 0; solver->args[i] != NULL; i++) {
        args[argidx++] = solver->args[i];
    }

    PerfProfRunHandle *handle = malloc(sizeof(*handle));
    handle->run_hash = compute_run_hash(&G_cptp_exe_hash, input, args, argidx);
    handle->input.seed = input->seed;
    snprintf_safe(handle->solver_name, ARRAY_LEN(handle->solver_name), "%s",
                  solver->name);
    snprintf_safe(handle->input.filepath, ARRAY_LEN(handle->input.filepath),
                  "%s", input->filepath);

    handle->input.uid.seedidx = input->uid.seedidx;
    strncpy_safe(handle->input.uid.hash.cstr, input->uid.hash.cstr,
                 ARRAY_LEN(handle->input.uid.hash.cstr));

    if (0 != strcmp(solver->name, BAPCOD_SOLVER_NAME)) {
        init_handle_json_output_path(handle, input);
    }

    args[argidx++] = "-w";
    args[argidx++] = (char *)handle->json_output_path;
    args[argidx++] = NULL;

    //
    // Check if the JSON output is already cached on disk
    //
    if (0 == strcmp(solver->name, BAPCOD_SOLVER_NAME) &&
        solver->args[0] == NULL) {
        handle_bapcod_solver(handle);
        free(handle);
    } else {
        if (!os_fexists(handle->json_output_path)) {
            proc_pool_queue(&G_pool, handle, args);
        } else {
            printf("Found cache for hash %s. CMD:", handle->run_hash.cstr);
            for (int32_t i = 0; args[i] && i < argidx; i++) {
                printf(" %s", args[i]);
            }
            printf("\n");
            update_perf_tbl_with_cptp_json_perf_data(handle);
            free(handle);
        }
    }
}

void handle_vrp_instance(PerfProfInput *input) {
    if (G_should_terminate) {
        return;
    }
    for (int32_t solver_idx = 0;
         !G_should_terminate &&
         (G_active_batch->solvers[solver_idx].name != NULL);
         solver_idx++) {
        PerfProfSolver *solver = &G_active_batch->solvers[solver_idx];

        run_cptp_solver(solver, input);
        if (G_pool.max_num_procs == 1) {
            proc_pool_join(&G_pool);
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
    if (typeflag == FTW_F || typeflag == FTW_SL) {
        // Is a regular file
        const char *ext = os_get_fext(fpath);
        if (ext && (0 == strcmp(ext, "vrp"))) {
            // printf("Found file: %s\n", fpath);

            Instance instance = parse(fpath);
            if (is_valid_instance(&instance)) {
                Filter *filter = &G_active_batch->filter;
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
                        UINT8_MAX, MIN(G_active_batch->nseeds,
                                       (int32_t)ARRAY_LEN(RANDOM_SEEDS))));

                    for (uint8_t seedidx = 0;
                         seedidx < num_seeds && !G_should_terminate;
                         seedidx++) {
                        input.uid.seedidx = seedidx;

                        input.seed = RANDOM_SEEDS[seedidx];
                        handle_vrp_instance(&input);
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

    return G_should_terminate ? FTW_STOP : FTW_CONTINUE;
}

void scan_dir_and_solve(const char *dirpath) {
    int result = nftw(dirpath, file_walk_cb, 8, 0);
    if (result == FTW_STOP) {
        proc_pool_join(&G_pool);
        printf("Requested to stop scanning dirpath %s\n", dirpath);
    } else if (result != 0) {
        perror("nftw walk");
        exit(EXIT_FAILURE);
    }
}

static void do_batch(PerfProfBatch *bgroup) {
    proc_pool_join(&G_pool);
    G_active_batch = bgroup;
    G_pool.max_num_procs = bgroup->max_num_procs;
    G_pool.on_async_proc_exit = on_async_proc_exit;

    // Adjust zero-initialized filters
    {
        Filter *filter = &bgroup->filter;
        if (filter->ncustomers.a >= 0 && filter->ncustomers.b == 0) {
            filter->ncustomers.b = 99999;
        }

        if (filter->nvehicles.a >= 0 && filter->nvehicles.b == 0) {
            filter->nvehicles.b = 99999;
        }
    }

    // Detect duplicate names in solver names
    for (int32_t i = 0; bgroup->solvers[i].name; i++) {
        for (int32_t j = 0; bgroup->solvers[j].name; j++) {
            if (i != j) {
                if (0 ==
                    strcmp(bgroup->solvers[i].name, bgroup->solvers[j].name)) {
                    log_fatal("\n\nInternal perfprof error: detected duplicate "
                              "name `%s` in group %s\n",
                              bgroup->solvers[i].name, bgroup->name);
                    abort();
                }
            }
        }
    }

    if (!G_should_terminate) {
        scan_dir_and_solve(bgroup->scan_root_dir);
    }
}

static void init(void) {

    os_mkdir(PERFPROF_DUMP_ROOTDIR, true);
    os_mkdir(PERFPROF_DUMP_ROOTDIR "/cache", true);

    // Compute the cptp exe hash
    {
        SHA256_CTX shactx;

        sha256_init(&shactx);
        sha256_update(&shactx, (BYTE *)CPTP_EXE, strlen(CPTP_EXE));
        sha256_hash_finalize(&shactx, &G_cptp_exe_hash);
    }
}

static void generate_perfs_imgs(PerfProfBatch *batch);

static void main_loop(void) {

    PerfProfBatch batches[] = {
        {1,
         "Integer separation vs Fractional separation",
         600.0,
         4,
         // "./data/ESPPRC - Test Instances/vrps",
         "data/BaPCod generated - Test instances/A-n37-k5",
         (Filter){NULL, {0, 72}, {0, 0}},
         {
             {"A",
              {
                  "--solver",
                  "mip",
              }},
             BAPCOD_SOLVER,
         }}};

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(batches); i++) {
        for (int32_t j = 0; j < (int32_t)ARRAY_LEN(batches); j++) {
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

    for (int32_t i = 0; i < !G_should_terminate && (int32_t)ARRAY_LEN(batches);
         i++) {

        printf("\n\n");
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("     DOING BATCH:\n");
        printf("            Batch max num concurrent procs: %d\n",
               batches[i].max_num_procs);
        printf("            Batch name: %s\n", batches[i].name);
        printf("            Batch timelimit: %g\n", batches[i].timelimit);
        printf("            Batch num seeds: %d\n", batches[i].nseeds);
        printf("            Batch scan_root_dir: %s\n",
               batches[i].scan_root_dir);
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("###########################################################"
               "\n");
        printf("\n\n");

        clear_perf_table();
        do_batch(&batches[i]);
        proc_pool_join(&G_pool);

        if (!G_should_terminate) {
            // Process the perf_table to generate the csv file
            generate_perfs_imgs(&batches[i]);
        }

        clear_perf_table();
    }

    proc_pool_join(&G_pool);
    clear_perf_table();
}

int main(int argc, char *argv[]) {
    init();
    sighandler_t prev_sigterm_handler = signal(SIGTERM, my_sighandler);
    sighandler_t prev_sigint_handler = signal(SIGINT, my_sighandler);
    { main_loop(); }
    signal(SIGTERM, prev_sigterm_handler);
    signal(SIGINT, prev_sigint_handler);
    proc_pool_join(&G_pool);
    return 0;
}

static void generate_performance_profile_using_python_script(
    PerfProfBatch *batch, char *csv_input_file, bool is_time_profile) {
    char *args[PROC_MAX_ARGS];
    int32_t argidx = 0;

    char title[512];

    char shift_str[128];
    char max_ratio_str[128];

    char timelimit_str[128];
    char *xlabel_str = is_time_profile ? "Time Ratio" : "Cost ratio";

    // double shift = batch->shift > 0 ? batch->shift : 1.0;
    // double max_ratio = batch->max_ratio > 0 ? batch->max_ratio : 4.0;

    double shift = 1.0;
    double max_ratio = 4.0;

    Path csv_input_file_basename;
    char output_file[OS_MAX_PATH];

    snprintf_safe(output_file, ARRAY_LEN(output_file), "%s/%s-plot.pdf",
                  os_dirname(csv_input_file, &csv_input_file_basename),
                  xlabel_str);

    snprintf_safe(shift_str, ARRAY_LEN(shift_str), "%g", shift);
    snprintf_safe(max_ratio_str, ARRAY_LEN(max_ratio_str), "%g", max_ratio);
    snprintf_safe(timelimit_str, ARRAY_LEN(timelimit_str), "%g",
                  is_time_profile ? batch->timelimit : 1e99);

    snprintf_safe(title, ARRAY_LEN(title), "%s (S=%s, M=%s)", batch->name,
                  shift_str, max_ratio_str);

    args[argidx++] = "python3";
    args[argidx++] = PYTHON3_PERF_SCRIPT;
    args[argidx++] = "--delimiter";
    args[argidx++] = ",";
    args[argidx++] = "--shift";
    args[argidx++] = shift_str;
    args[argidx++] = "--maxratio";
    args[argidx++] = max_ratio_str;
    args[argidx++] = "--timelimit";
    args[argidx++] = timelimit_str;
    args[argidx++] = "--plot-title";
    args[argidx++] = title;
    args[argidx++] = "--startidx"; // Start index to associated with the colors
    args[argidx++] = "0";
    args[argidx++] = "--x-label"; // Start index to associated with the colors
    args[argidx++] = xlabel_str;
    args[argidx++] = csv_input_file;
    args[argidx++] = output_file;
    args[argidx++] = NULL;

    proc_spawn_sync(args);
}

static void generate_perfs_imgs(PerfProfBatch *batch) {
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

        size_t tbl_len = hmlenu(G_perftbl);
        for (size_t i = 0; i < tbl_len; i++) {
            PerfTblKey *key = &G_perftbl[i].key;
            PerfTblValue *value = &G_perftbl[i].value;

            Hash *hash = &key->uid.hash;
            uint8_t seedidx = key->uid.seedidx;

            fprintf(fh, "%d:%s", seedidx, hash->cstr);

            // Due to the out of order generation of the performance table,
            // the perf data may be out of order, compared to the order of
            // the solvers considered when outputting the CSV file.
            // Therefore fix a solver name, and find its perf in the list,
            // to output the perf data in the expected order
            for (int32_t curr_solver_idx = 0; curr_solver_idx < num_solvers;
                 curr_solver_idx++) {
                // Fix the solver name as in order
                char *solver_name = batch->solvers[curr_solver_idx].name;

                // Scan the list to find the matching solver perf data
                for (int32_t run_idx = 0; run_idx < value->num_runs;
                     run_idx++) {
                    PerfProfRun *run = &value->runs[run_idx];

                    double data =
                        is_time_profile ? run->perf.time : run->perf.cost;

                    if (0 == strcmp(run->solver_name, solver_name)) {
                        fprintf(fh, ",%.17g", data);
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
