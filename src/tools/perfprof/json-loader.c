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

#include "json-loader.h"
#include "core.h"

cJSON *load_json(char *filepath) {
    cJSON *root = NULL;
    char *contents = fread_all_into_cstr(filepath, NULL);
    if (!contents) {
        log_warn("Failed to load JSON contents from `%s`\n", filepath);
    } else if (contents && contents[0] != '\0') {
        root = cJSON_Parse(contents);
        if (!root) {
            log_warn("Failed to parse JSON contents from `%s`\n", filepath);
        }
    }

    if (contents)
        free(contents);

    return root;
}

void parse_cptp_solver_json_dump(PerfProfRun *run, cJSON *root) {
    cJSON *itm_solve_status = NULL;
    cJSON *itm_timing_info = NULL;
    cJSON *itm_bounds = NULL;

    cJSON *itm_took = NULL;
    cJSON *itm_solve_status_code = NULL;
    cJSON *itm_primal_bound = NULL;

    itm_solve_status = cJSON_GetObjectItemCaseSensitive(root, "solveStatus");
    itm_timing_info = cJSON_GetObjectItemCaseSensitive(root, "timingInfo");
    itm_bounds = cJSON_GetObjectItemCaseSensitive(root, "bounds");

    if (itm_solve_status) {
        itm_solve_status_code =
            cJSON_GetObjectItemCaseSensitive(itm_solve_status, "code");
    }

    if (itm_timing_info) {
        itm_took = cJSON_GetObjectItemCaseSensitive(itm_timing_info, "took");
    }

    if (itm_bounds) {
        itm_primal_bound =
            cJSON_GetObjectItemCaseSensitive(itm_bounds, "primal");
    }

    SolveStatus status = SOLVE_STATUS_NULL;
    double time = INFINITY;
    double primal_bound = CRASHED_SOLVER_DEFAULT_COST_VAL;

    if (itm_solve_status_code && cJSON_IsNumber(itm_solve_status_code)) {
        status = cJSON_GetNumberValue(itm_solve_status_code);
    } else {
        status |= SOLVE_STATUS_ERR;
    }

    if (itm_took && cJSON_IsNumber(itm_took)) {
        time = cJSON_GetNumberValue(itm_took);
    } else {
        status |= SOLVE_STATUS_ERR;
    }

    if (itm_primal_bound && cJSON_IsNumber(itm_primal_bound)) {
        primal_bound = cJSON_GetNumberValue(itm_primal_bound);
    } else {
        status |= SOLVE_STATUS_ERR;
    }

    if (BOOL(status & SOLVE_STATUS_CLOSED_PROBLEM) &&
        !BOOL(status & SOLVE_STATUS_PRIMAL_SOLUTION_AVAIL)) {
        primal_bound = INFEASIBLE_SOLUTION_DEFAULT_COST_VAL;
    } else if (!BOOL(status & SOLVE_STATUS_CLOSED_PROBLEM)) {
        primal_bound = CRASHED_SOLVER_DEFAULT_COST_VAL;
    } else if (status == SOLVE_STATUS_NULL || BOOL(status & SOLVE_STATUS_ERR)) {
        primal_bound = CRASHED_SOLVER_DEFAULT_COST_VAL;
    }

    run->solution.status = status;
    run->solution.time = time;
    run->solution.primal_bound = primal_bound;
}

void parse_bapcod_solver_json_dump(PerfProfRun *run, cJSON *root) {
    cJSON *rcsp_infos = cJSON_GetObjectItemCaseSensitive(root, "rcsp-infos");

    SolveStatus status =
        SOLVE_STATUS_CLOSED_PROBLEM | SOLVE_STATUS_PRIMAL_SOLUTION_AVAIL;
    double primal_bound = CRASHED_SOLVER_DEFAULT_COST_VAL;
    double time = INFINITY;

    if (rcsp_infos && cJSON_IsObject(rcsp_infos)) {
        cJSON *columns_reduced_cost =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "columnsReducedCost");
        cJSON *itm_took =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "seconds");
        cJSON *pricer_success =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "pricerSuccess");

        if (itm_took && cJSON_IsNumber(itm_took)) {
            time = cJSON_GetNumberValue(itm_took);
        }

        if (columns_reduced_cost && cJSON_IsArray(columns_reduced_cost)) {
            cJSON *elem = NULL;
            int32_t num_elems = 0;
            cJSON_ArrayForEach(elem, columns_reduced_cost) { num_elems += 1; }

            if (num_elems == 1) {
                cJSON_ArrayForEach(elem, columns_reduced_cost) {
                    cJSON *itm_cost = elem;
                    if (itm_cost && cJSON_IsNumber(itm_cost)) {
                        primal_bound = cJSON_GetNumberValue(itm_cost);
                    }
                    break;
                }
            } else {
                status = SOLVE_STATUS_ERR;
            }
        }

        //
        // Replace the cost if it is non valid negative reduced cost,
        // or if the solver crashed in the process.
        //
        if (pricer_success && cJSON_IsFalse(pricer_success)) {
            primal_bound = CRASHED_SOLVER_DEFAULT_COST_VAL;
            status = SOLVE_STATUS_ERR;
        } else if (!is_valid_reduced_cost(primal_bound)) {
            status = SOLVE_STATUS_CLOSED_PROBLEM;
            primal_bound = INFEASIBLE_SOLUTION_DEFAULT_COST_VAL;
        }
    }

    run->solution.status = status;
    run->solution.time = time;
    run->solution.primal_bound = primal_bound;
}
