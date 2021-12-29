#include "warm-start.h"
#include "validation.h"

typedef struct InsHeurNodePair {
    int32_t u, v;
} InsHeurNodePair;

static bool register_warm_solution(Solver *solver, const Instance *instance,
                                   const Solution *solution) {
    bool result = true;
    const int32_t n = instance->num_customers + 1;
    CPXNNZ beg[] = {0};
    int effortlevel[] = {CPX_MIPSTART_NOCHECK};
    // int *effortlevel = NULL;
    CPXDIM *varindices =
        malloc(solver->data->num_mip_vars * sizeof(*varindices));

    double *vstar = malloc(solver->data->num_mip_vars * sizeof(*vstar));
#ifndef NDEBUG
    memset(vstar, 0xCD, solver->data->num_mip_vars * sizeof(*vstar));
#endif

    for (int32_t i = 0; i < n; i++) {
        bool i_is_visited = solution->tour.comp[i] == 0;
        vstar[get_y_mip_var_idx(instance, i)] = i_is_visited ? 1.0 : 0.0;
    }

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (i == j) {
                continue;
            }

            int32_t succ_i = solution->tour.succ[i];

            if (succ_i == j) {
                vstar[get_x_mip_var_idx(instance, i, j)] = 1.0;
            } else {
                vstar[get_x_mip_var_idx(instance, i, j)] = 0.0;
            }
        }
    }

#ifndef NDEBUG
    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (i == j) {
                continue;
            }

            double x = vstar[get_x_mip_var_idx(instance, i, j)];
            assert(x == 0.0 || x == 1.0);
        }
    }

    for (CPXDIM i = 0; i < solver->data->num_mip_vars; i++) {
        assert(vstar[i] == 0.0 || vstar[i] == 1.0);
    }

#endif

    for (CPXDIM i = 0; i < solver->data->num_mip_vars; i++) {
        varindices[i] = i;
    }

    if (0 != CPXXaddmipstarts(solver->data->env, solver->data->lp, 1,
                              solver->data->num_mip_vars, beg, varindices,
                              vstar, effortlevel, NULL)) {
        log_fatal("%s :: Failed to call CPXXaddmipstarts()", __func__);
        result = false;
        goto terminate;
    }

terminate:
    free(vstar);
    free(varindices);
    return result;
}

bool ins_heur_find_starting_pair(const Instance *instance,
                                 InsHeurNodePair *start_pair) {
    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    int32_t s = -1;
    int32_t e = -1;

    // Start is always the depot node
    s = 0;
    if (instance->demands[s] > Q) {
        return false;
    }

    const double rel_Q = Q - instance->demands[s];

    bool any_e = false;
    for (int32_t i = 1; i < n; i++) {
        if (instance->demands[i] <= rel_Q) {
            any_e = true;
            break;
        }
    }

    if (!any_e) {
        return false;
    }

    e = -1;
    do {
        e = rand() % n;
    } while (e == s || instance->demands[e] > rel_Q);

    start_pair->u = s;
    start_pair->v = e;
    return true;
}

bool mip_ins_heur_warm_start(Solver *solver, const Instance *instance) {
    bool result = true;
    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    Solution solution = solution_create(instance);
    Tour *const tour = &solution.tour;
    tour->num_comps = 1;

    InsHeurNodePair starting_pair;
    if (!ins_heur_find_starting_pair(instance, &starting_pair)) {
        log_fatal("%s :: Unable to find starting pair", __func__);
        result = false;
        goto terminate;
    }

    const int32_t start = starting_pair.u;
    const int32_t end = starting_pair.v;

    assert(start >= 0 && start < n);
    assert(end >= 0 && end < n);
    assert(start != end);

    tour->comp[start] = 0;
    tour->comp[end] = 0;
    tour->succ[start] = end;
    tour->succ[end] = start;

    double cost = cptp_dist(instance, start, end) +
                  cptp_dist(instance, end, start) - instance->duals[start] -
                  instance->duals[end];
    double sum_demands = instance->demands[start] + instance->demands[end];

    while (true) {
        double best_delta_cost = INFINITY;
        int32_t best_h = -1;
        int32_t best_a = -1;
        int32_t best_b = -1;

        // Scan for node to be inserted
        for (int32_t h = 1; h < n; h++) {
            bool h_is_visited = tour->comp[h] == 0;
            if (h_is_visited) {
                continue;
            }

            for (int32_t a = 0; a < n; a++) {
                bool a_is_visited = tour->comp[a] == 0;
                if (!a_is_visited) {
                    continue;
                }

                int32_t b = tour->succ[a];
                assert(b >= 0 && b < n);
                assert(tour->succ[a] == b);

                const double rel_Q = Q - sum_demands;
                double delta_cost = INFINITY;

                if (instance->demands[h] <= rel_Q) {
                    double c_ah = cptp_dist(instance, a, h);
                    double c_hb = cptp_dist(instance, h, b);
                    double c_ab = cptp_dist(instance, a, b);
                    delta_cost = c_ah + c_hb - c_ab - instance->duals[h];
                }

                if (delta_cost < best_delta_cost) {
                    best_delta_cost = delta_cost;
                    best_h = h;
                    best_a = a;
                    best_b = b;
                }
            }
        }

        if (best_h < 0 || best_delta_cost > 0.0) {
            break;
        }

        assert(best_h >= 0 && best_h < n);
        assert(best_a >= 0 && best_a < n);
        assert(best_b >= 0 && best_b < n);
        assert(tour->succ[best_a] == best_b);

        cost += best_delta_cost;
        sum_demands += instance->demands[best_h];

        tour->comp[best_h] = 0;
        tour->succ[best_a] = best_h;
        tour->succ[best_h] = best_b;

#ifndef NDEBUG
        validate_tour(instance, tour);
#endif
    }

    solution.upper_bound = cost;
    printf("%s :: Warm starting with a solution of cost %f\n", __func__, cost);
    log_info("%s :: Found a solution of cost %f", __func__, cost);

#ifndef NDEBUG
    validate_tour(instance, tour);
    validate_solution(instance, &solution);
#endif

    if (!register_warm_solution(solver, instance, &solution)) {
        log_fatal("%s :: register_warm_solution_failed", __func__);
        result = false;
        goto terminate;
    }

terminate:
    solution_destroy(&solution);
    return result;
}
