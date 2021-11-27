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

typedef struct PerfProcessInfo {
    char solver_name[48];
    Hash hash;
    char json_output_path[OS_MAX_PATH + 32];
} PerfProfProcessInfo;

typedef struct {
    Hash hash;
    char solver_name[48];
    double secs;
    double obj_ub;
} Perf;

#define MAX_NUM_SOLVERS_PER_GROUP 16

typedef struct PerfTblEntry {
    int32_t num_perfs;
    Perf perfs[MAX_NUM_SOLVERS_PER_GROUP];
} PerfTblEntry;

typedef struct PerfTbl {
    char *key;
    PerfTblEntry value;
} PerfTbl;

typedef struct {
    int32_t max_num_procs;
    char *name;
    double timelimit;
    int32_t nseeds;
    const char *scan_root_dir;
    Filter filter;
    PerfProfSolver solvers[MAX_NUM_SOLVERS_PER_GROUP];
} PerfProfBatchGroup;

#ifndef NDEBUG
#define CPTP_EXE "./build/Debug/src/cptp"
#else
#define CPTP_EXE "./build/Release/src/cptp"
#endif

#define PYTHON3_PERF_SCRIPT "./src/tools/perfprof/perfprof.py"
#define BAPCOD_SOLVER_NAME "BaPCod"
#define PERFPROF_DUMP_ROOTDIR "perfprof-dump"

static Hash G_cptp_exe_hash;
static bool G_should_terminate;
static ProcPool G_pool = {0};
static PerfProfBatchGroup *G_active_bgroup = NULL;
static PerfTbl *G_perftbl = NULL;

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

Perf make_invalidated_perf(PerfProfBatchGroup *bgroup) {
    Perf perf = {0};
    perf.obj_ub = INFINITY;
    perf.secs = 2.0 * bgroup->timelimit;
    return perf;
}

void insert_perf_to_table(char *solver_name, Hash *hash, Perf *p) {
    if (!G_perftbl) {
        sh_new_strdup(G_perftbl);
    }
    if (!shgetp_null(G_perftbl, hash->cstr)) {
        PerfTblEntry empty_entry = {0};
        shput(G_perftbl, hash->cstr, empty_entry);
    }

    PerfTblEntry *e = NULL;
    {
        PerfTbl *t = shgetp(G_perftbl, hash->cstr);
        assert(t);
        if (t) {
            e = &t->value;
        }
    }

    if (!e) {
        return;
    }

    int32_t i = 0;
    for (i = 0; i < e->num_perfs; i++) {
        if (0 == strcmp(solver_name, e->perfs[i].solver_name)) {
            // Need to update previous perf
            // BUT... This is an invalid case to happen
            assert(0);
            break;
        }
    }

    if (i == e->num_perfs && (e->num_perfs < (int32_t)ARRAY_LEN(e->perfs))) {
        e->num_perfs++;
    } else {
        assert(0);
    }

    if (i < e->num_perfs) {
        memcpy(&e->perfs[i], p, MIN(sizeof(e->perfs[i]), sizeof(*p)));
    } else {
        assert(0);
    }
}

void clear_perf_table(void) {
    if (G_perftbl) {
        shfree(G_perftbl);
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

Perf perf_from_cptp_generated_json(Hash *hash, char *solver_name, cJSON *root) {
    Perf perf = {0};
    make_invalidated_perf(G_active_bgroup);
    memcpy(&perf.hash, hash, sizeof(*hash));

    if (solver_name) {
        snprintf_safe(perf.solver_name, ARRAY_LEN(perf.solver_name), "%s",
                      solver_name);
    }

    cJSON *itm_took = cJSON_GetObjectItemCaseSensitive(root, "took");
    cJSON *itm_cost = cJSON_GetObjectItemCaseSensitive(root, "cost");

    if (itm_took && cJSON_IsNumber(itm_took)) {
        perf.secs = cJSON_GetNumberValue(itm_took);
    }
    if (itm_cost && cJSON_IsNumber(itm_cost)) {
        perf.obj_ub = cJSON_GetNumberValue(itm_cost);
    }

    return perf;
}

void on_async_proc_exit(Process *p, int exit_status, void *user_handle) {
    if (!user_handle) {
        return;
    }

    PerfProfProcessInfo *info = user_handle;
    if (p) {
        Perf perf = make_invalidated_perf(G_active_bgroup);

        if (exit_status == 0) {
            char *contents = fread_all_into_null_terminated_string(
                info->json_output_path, NULL);
            if (!contents) {
                fprintf(stderr,
                        "Failed to load JSON contents from `%s` produced from "
                        "PID %d\n",
                        info->json_output_path, p->pid);
                exit(1);
            }
            cJSON *root = cJSON_Parse(contents);
            if (!root) {
                fprintf(stderr,
                        "Failed to parse JSON contents from `%s` produced "
                        "from PID "
                        "%d\n",
                        info->json_output_path, p->pid);
                exit(1);
            } else {
                perf = perf_from_cptp_generated_json(&info->hash,
                                                     info->solver_name, root);
            }
            cJSON_Delete(root);
            free(contents);
        } else {
            fprintf(stderr,
                    "\n\n\nSolver `%s` returned with non 0 exit status. Got "
                    "%d\n\n\n",
                    info->solver_name, exit_status);
        }

        printf("Got perf ::: sha = %s, solver_name = %s, time = %.17g, obj_ub "
               "= %.17g\n",
               perf.hash.cstr, info->solver_name, perf.secs, perf.obj_ub);
        insert_perf_to_table(info->solver_name, &info->hash, &perf);
    }
    free(info);
}

static void sha256_finalize_to_string(SHA256_CTX *shactx, Hash *hash) {

    BYTE bytes[32];

    sha256_final(shactx, bytes);

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(bytes); i++) {
        snprintf_safe(hash->cstr + 2 * i, 65 - 2 * i, "%02x", bytes[i]);
    }

    hash->cstr[ARRAY_LEN(hash->cstr) - 1] = 0;
}

static void sha256_hash_file_contents(const char *fpath, Hash *hash) {

    SHA256_CTX shactx;
    sha256_init(&shactx);
    size_t len = 0;
    char *contents = fread_all_into_null_terminated_string(fpath, &len);
    if (contents) {
        sha256_update(&shactx, (BYTE *)contents, len);
    } else {
        fprintf(stderr, "%s: Failed to hash (sha256) file contents\n", fpath);
        abort();
    }
    sha256_finalize_to_string(&shactx, hash);
    free(contents);
}

static void compute_whole_sha256(Hash *hash, const Hash *exe_hash,
                                 const Hash *instance_hash,
                                 char *args[PROC_MAX_ARGS], int32_t num_args) {

    SHA256_CTX shactx;
    sha256_init(&shactx);
    for (int32_t i = 0; i < num_args; i++) {
        sha256_update(&shactx, (const BYTE *)(&args[i][0]), strlen(args[i]));
    }

    if (exe_hash) {
        sha256_update(&shactx, (const BYTE *)(&exe_hash->cstr[0]), 64);
    }

    if (instance_hash) {
        sha256_update(&shactx, (const BYTE *)(&instance_hash->cstr[0]), 64);
    }

    sha256_finalize_to_string(&shactx, hash);
}

static void run_solver(PerfProfSolver *solver, const char *fpath, int32_t seed,
                       Hash *instance_hash) {
    if (G_should_terminate) {
        return;
    }

    char *args[PROC_MAX_ARGS];
    int32_t argidx = 0;

    char timelimit[128];
    snprintf_safe(timelimit, ARRAY_LEN(timelimit), "%g",
                  G_active_bgroup->timelimit);

    char timelimit_extended[128];
    snprintf_safe(timelimit_extended, ARRAY_LEN(timelimit_extended), "%g",
                  G_active_bgroup->timelimit * 1.05 + 2);

    char killafter[128];
    snprintf_safe(killafter, ARRAY_LEN(killafter), "%g",
                  G_active_bgroup->timelimit * 1.05 + 2 -
                      G_active_bgroup->timelimit);

    char seed_str[128];
    snprintf_safe(seed_str, ARRAY_LEN(seed_str), "%d", seed);

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
    args[argidx++] = (char *)fpath;

    for (int32_t i = 0; solver->args[i] != NULL; i++) {
        args[argidx++] = solver->args[i];
    }

    PerfProfProcessInfo *pinfo = malloc(sizeof(*pinfo));
    compute_whole_sha256(&pinfo->hash, &G_cptp_exe_hash, instance_hash, args,
                         argidx);
    snprintf_safe(pinfo->solver_name, ARRAY_LEN(pinfo->solver_name), "%s",
                  solver->name);

    Path fpath_basename;
    Path json_report_path_basename;

    snprintf_safe(pinfo->json_output_path, ARRAY_LEN(pinfo->json_output_path),
                  "%s/%s/%s.json", PERFPROF_DUMP_ROOTDIR, pinfo->hash.cstr,
                  os_basename(fpath, &fpath_basename));

    args[argidx++] = "-w";
    args[argidx++] = (char *)pinfo->json_output_path;
    args[argidx++] = NULL;

    os_mkdir(os_dirname(pinfo->json_output_path, &json_report_path_basename),
             true);

    //
    // Check if the JSON output is already cached on disk
    //
    if (!os_fexists(pinfo->json_output_path)) {
        proc_pool_queue(&G_pool, pinfo, args);
    } else {
        printf("Found cache for hash %s. CMD:", pinfo->hash.cstr);
        for (int32_t i = 0; args[i] && i < argidx; i++) {
            printf(" %s", args[i]);
        }
        printf("\n");
        free(pinfo);
    }
}

void handle_vrp_instance(const char *fpath, int32_t seed, Hash *instance_hash) {
    if (G_should_terminate) {
        return;
    }
    for (int32_t solver_idx = 0;
         !G_should_terminate &&
         (G_active_bgroup->solvers[solver_idx].name != NULL);
         solver_idx++) {
        PerfProfSolver *solver = &G_active_bgroup->solvers[solver_idx];
        run_solver(solver, fpath, seed, instance_hash);
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
                Filter *filter = &G_active_bgroup->filter;
                if (!is_filtered_instance(filter, &instance)) {
                    Hash instance_hash;
                    sha256_hash_file_contents(fpath, &instance_hash);
                    for (int32_t seedidx = 0;
                         seedidx < MIN(G_active_bgroup->nseeds,
                                       (int32_t)ARRAY_LEN(RANDOM_SEEDS)) &&
                         !G_should_terminate;
                         seedidx++) {
                        handle_vrp_instance(fpath, RANDOM_SEEDS[seedidx],
                                            &instance_hash);
                    }
                } else {
                    printf("%s: Skipping since it does not match filter\n",
                           fpath);
                }
                instance_destroy(&instance);
            } else {
                fprintf(stderr, "%s: Failed to parse input file\n", fpath);
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

static void do_batch(PerfProfBatchGroup *bgroup) {
    proc_pool_join(&G_pool);
    G_active_bgroup = bgroup;
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
                    fprintf(stderr,
                            "\n\nInternal perfprof error: detected duplicate "
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

    // Compute the cptp exe hash
    {
        SHA256_CTX shactx;

        sha256_init(&shactx);
        sha256_update(&shactx, (BYTE *)CPTP_EXE, strlen(CPTP_EXE));
        sha256_finalize_to_string(&shactx, &G_cptp_exe_hash);
    }
}

static void output_csv_file(PerfProfBatchGroup *batch);

static void main_loop(void) {

    PerfProfBatchGroup batches[] = {
        {1,
         "Integer separation vs Fractional separation",
         600.0,
         4,
         "./data/ESPPRC - Test Instances/vrps",
         (Filter){NULL, {0, 72}, {0, 0}},
         {{"A",
           {
               "--solver",
               "mip",
           }},
          {"B",
           {
               "--solver",
               "mip",
           }}}}};

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(batches); i++) {
        for (int32_t j = 0; j < (int32_t)ARRAY_LEN(batches); j++) {
            if (i == j) {
                continue;
            }

            if (0 == strcmp(batches[i].name, batches[j].name)) {
                fprintf(stderr,
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

        // Process the perf_table to generate the csv file
        output_csv_file(&batches[i]);

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
    PerfProfBatchGroup *batch, char *csv_input_file, bool is_time_profile) {
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

    proc_spawn_sync(args);
}

static void output_csv_file(PerfProfBatchGroup *batch) {
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
            fprintf(stderr, "%s: failed to output csv data\n", data_csv_file);
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

        size_t tbl_len = shlenu(G_perftbl);
        for (size_t i = 0; i < tbl_len; i++) {
            char *key = G_perftbl[i].key;
            PerfTblEntry *value = &G_perftbl[i].value;

            fprintf(fh, "%s", key);

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
                for (int32_t perf_idx = 0; perf_idx < value->num_perfs;
                     perf_idx++) {
                    Perf *perf = &value->perfs[perf_idx];

                    double data = is_time_profile ? perf->secs : perf->obj_ub;

                    if (0 == strcmp(perf->solver_name, solver_name)) {
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
