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

#define DEFAULT_TIME_LIMIT ((double)5.0) // 20 minutes
#define INFEASIBLE_SOLUTION_DEFAULT_COST_VAL ((double)1.0)
/// Default cost value attributed to a crashed solver, or a solver
/// which cannot produce any cost within the resource limits.
#define CRASHED_SOLVER_DEFAULT_COST_VAL ((double)10.0)

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

#if __cplusplus
}
#endif
