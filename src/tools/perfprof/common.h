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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "os.h"
#include "core.h"
#include "proc.h"

#define INSTANCE_NAME_MAX_LEN 256
#define SOLVER_NAME_MAX_LEN 48
#define JSON_OUTPUT_FILEPATH_MAX_LEN (OS_MAX_PATH + 32)

#define SHA256_CSTR_LEN 65

#define DEFAULT_TIME_LIMIT ((double)4) // 20 minutes
#define INFEASIBLE_SOLUTION_DEFAULT_COST_VAL ((double)1.0)
/// Default cost value attributed to a crashed solver, or a solver
/// which cannot produce any cost within the resource limits.
#define CRASHED_SOLVER_DEFAULT_COST_VAL ((double)10.0)

#define CPTP_EXE "./build/Release/src/cptp"
#define PYTHON3_PERF_SCRIPT "./src/tools/perfprof/plot.py"
#define BAPCOD_SOLVER_NAME "libRCSP DP pricer"
#define PERFPROF_DUMP_ROOTDIR "perfprof-dump"

#define MAX_NUM_SOLVERS_PER_BATCH 8
#define BATCH_MAX_NUM_DIRS 64

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

/// Struct that stores an SHA256 hash as a printable c-string
typedef struct {
    char cstr[SHA256_CSTR_LEN];
} Hash;

typedef struct {
    int32_t a, b;
} int32_interval_t;

/// The kind of the statistic being tracked
typedef enum StatKind {
    STAT_TIME,
    STAT_COST,
    STAT_REL_COST,

    //
    // Last field
    //
    MAX_NUM_STATS,
} StatKind;

/// Unique identifier/handle to a parameterized solver.
/// In terms of tracking performance, two solvers having
/// the same name but different parameters, are tracked
/// as "distinct solvers".
typedef struct {
    char *name;
    char *args[PROC_MAX_ARGS];
} PerfProfSolver;

typedef struct {
    SolveStatus status;

    double time;
    double primal_bound;
} SolverSolution;

/// Unique identifier/handle to each (seed, instance).
/// The instance itself is uniquely classified by the problem
/// it contains. An SH256 sum on the parsed instance uniquely
/// identifies each problem (hash field)
/// NOTE: This struct should remain as packed and as small as possible,
///        since it will be one of the two main causes of memory consumption
///       in this program. It will be stored in a hashmap and will live in
///       memory for the entire duration of the current batch.
typedef struct {
    uint8_t seedidx;
    Hash hash;
} PerfProfInputUniqueId;

typedef struct {
    char instance_name[INSTANCE_NAME_MAX_LEN];
    char filepath[OS_MAX_PATH];
    PerfProfInputUniqueId uid;
    int32_t seed;
} PerfProfInput;

/// This struct uniquely identifies a currently running
/// perf-prof run, i.e. a solver solving a specific (seed, problem) instance
/// which is still running and its output has yet to be resolved.
/// This struct may be larger compared to the @PerfProfRun struct,
/// since at most a few dozens of PerfProfRunHandle may be active
/// at the same time.
typedef struct {
    char solver_name[SOLVER_NAME_MAX_LEN];
    PerfProfInput input;
    Hash run_hash;
    char json_output_path[JSON_OUTPUT_FILEPATH_MAX_LEN];
} PerfProfRunHandle;

/// This struct uniquely identifies a resolved run of a solver
/// with its associated performance statistics.
/// NOTE: This struct should remain as packed and as small as possible,
///       since it will be the main cause of memory consumption in
///       this program. It will be stored in a hashmap and will live in
///       memory for the entire duration of the current batch.
typedef struct {
    char solver_name[SOLVER_NAME_MAX_LEN];
    SolverSolution solution;
} PerfProfRun;

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

/// Each batch may have an associated filter to reduce the number
/// of instances considered.
typedef struct {
    char *family; /// TODO: Not supported yet
    int32_interval_t ncustomers;
    int32_interval_t nvehicles;
} Filter;

/// A batch is set of instances extracted from a list of directories, which
/// are recursively scanned.
/// Each instance passes through a filter: for example of all the instances
/// found, keep the ones having a number of customers less than 100. Each
/// instance is solved by the solvers specified by the batch. The batch
/// accumulates the performance of each solver on the
/// considered instances. For each batch a separate performance profile is
/// generated.
/// Caching of the pair (seed, instance, solver_name, params) spans all the
/// batches. Therefore the same (seed, instance, solver_name, params) tuple will
/// be solved exactly once, even it belongs to multiple batches.
typedef struct {
    int32_t max_num_procs;
    char *name;
    double timelimit;
    int32_t nseeds;
    char *dirs[BATCH_MAX_NUM_DIRS];
    Filter filter;
    PerfProfSolver solvers[MAX_NUM_SOLVERS_PER_BATCH];
} PerfProfBatch;

typedef struct {
    PerfTblEntry *buf;
} PerfTbl;

typedef struct {
    Hash cptp_exe_hash;
    Hash bapcod_virtual_exe_hash;
    bool should_terminate;
    ProcPool pool;
    PerfProfBatch *current_batch;
    PerfTbl perf_tbl;
} AppCtx;

static inline double get_extended_timelimit(double timelimit) {
    return ceil(1.05 * timelimit + 2);
}

static inline double get_kill_timelimit(double timelimit) {
    return ceil(1.05 * get_extended_timelimit(timelimit));
}

#if __cplusplus
}
#endif
