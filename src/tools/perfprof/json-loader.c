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
    cJSON *itm_took = cJSON_GetObjectItemCaseSensitive(root, "took");
    cJSON *itm_feasible = cJSON_GetObjectItemCaseSensitive(root, "feasible");
    cJSON *itm_valid = cJSON_GetObjectItemCaseSensitive(root, "valid");
    cJSON *itm_dual_bound = cJSON_GetObjectItemCaseSensitive(root, "dualBound");
    cJSON *itm_primal_bound =
        cJSON_GetObjectItemCaseSensitive(root, "primalBound");

    if (itm_took && cJSON_IsNumber(itm_took)) {
        run->perf.time = cJSON_GetNumberValue(itm_took);
    }

    bool valid = false;
    bool feasible = false;

    if (itm_feasible && cJSON_IsBool(itm_feasible)) {
        feasible = cJSON_IsTrue(itm_feasible);
    }

    if (itm_valid && cJSON_IsBool(itm_valid)) {
        valid = cJSON_IsTrue(itm_valid);
    }

    double cost = CRASHED_SOLVER_DEFAULT_COST_VAL;
    double primal_bound = INFINITY;
    double dual_bound = INFINITY;

    if (itm_primal_bound && cJSON_IsNumber(itm_primal_bound)) {
        primal_bound = cJSON_GetNumberValue(itm_primal_bound);
    }
    if (itm_dual_bound && cJSON_IsNumber(itm_dual_bound)) {
        dual_bound = cJSON_GetNumberValue(itm_dual_bound);
    }

    bool primal_bound_equal_dual_bound =
        feq(primal_bound, dual_bound, COST_TOLERANCE);

    if (valid && feasible) {
        if (is_valid_reduced_cost(primal_bound)) {
            cost = primal_bound;
        } else {
            cost = INFEASIBLE_SOLUTION_DEFAULT_COST_VAL;
        }
    } else if (valid && !feasible) {
        // NOTE(dparo):
        //    A solution may be infeasible for two reasons:
        //    1. Given the timelimit we weren't unable to find one (which is
        //    bad!!)
        //    2. We proved to optimality that no solution exist (which is
        //    good!!!)
        if (primal_bound_equal_dual_bound) {
            cost = INFEASIBLE_SOLUTION_DEFAULT_COST_VAL;
        } else {
            cost = CRASHED_SOLVER_DEFAULT_COST_VAL;
        }
    } else {
        assert(!valid);
        cost = CRASHED_SOLVER_DEFAULT_COST_VAL;
    }

    run->perf.solution.feasible = feasible;
    run->perf.solution.cost = cost;
}

void parse_bapcod_solver_json_dump(PerfProfRun *run, cJSON *root) {
    cJSON *rcsp_infos = cJSON_GetObjectItemCaseSensitive(root, "rcsp-infos");

    run->perf.solution.feasible = true;

    if (rcsp_infos && cJSON_IsObject(rcsp_infos)) {

        cJSON *columns_reduced_cost =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "columnsReducedCost");

        cJSON *itm_took =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "seconds");

        cJSON *pricer_success =
            cJSON_GetObjectItemCaseSensitive(rcsp_infos, "pricerSuccess");

        if (itm_took && cJSON_IsNumber(itm_took)) {
            run->perf.time = cJSON_GetNumberValue(itm_took);
        }

        if (columns_reduced_cost && cJSON_IsArray(columns_reduced_cost)) {
            cJSON *elem = NULL;
            int32_t num_elems = 0;
            cJSON_ArrayForEach(elem, columns_reduced_cost) { num_elems += 1; }

            if (num_elems == 1) {
                cJSON_ArrayForEach(elem, columns_reduced_cost) {
                    cJSON *itm_cost = elem;
                    if (itm_cost && cJSON_IsNumber(itm_cost)) {
                        run->perf.solution.cost =
                            cJSON_GetNumberValue(itm_cost);
                    }
                    break;
                }
            }
        }

        //
        // Replace the cost if it is non valid negative reduced cost,
        // or if the solver crashed in the process.
        //
        if (pricer_success && cJSON_IsFalse(pricer_success)) {
            run->perf.solution.cost = CRASHED_SOLVER_DEFAULT_COST_VAL;
        } else if (!is_valid_reduced_cost(run->perf.solution.cost)) {
            run->perf.solution.cost = INFEASIBLE_SOLUTION_DEFAULT_COST_VAL;
        }
    }
}
