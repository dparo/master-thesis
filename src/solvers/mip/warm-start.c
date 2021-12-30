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
    int effortlevel[] = {CPX_MIPSTART_CHECKFEAS};
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
        for (int32_t j = i + 1; j < n; j++) {
            int32_t succ_i = solution->tour.succ[i];
            int32_t succ_j = solution->tour.succ[j];
            int32_t comp_i = solution->tour.comp[i];
            int32_t comp_j = solution->tour.comp[j];

            if (comp_i == 0 && comp_j == 0) {
                assert(succ_i >= 0 && succ_i < n);
                assert(succ_j >= 0 && succ_j < n);

                if (succ_i == j || succ_j == i) {
                    vstar[get_x_mip_var_idx(instance, i, j)] = 1.0;
                } else {
                    vstar[get_x_mip_var_idx(instance, i, j)] = 0.0;
                }
            } else {
                vstar[get_x_mip_var_idx(instance, i, j)] = 0.0;
            }
        }
    }

#ifndef NDEBUG
    for (CPXDIM i = 0; i < solver->data->num_mip_vars; i++) {
        assert(vstar[i] == 0.0 || vstar[i] == 1.0);
    }

    {
        Tour t = tour_create(instance);
        unpack_mip_solution(instance, &t, vstar);
        validate_tour(instance, &t);
        assert(t.num_comps == 1);
        for (int32_t i = 0; i < n; i++) {
            assert(t.comp[i] == solution->tour.comp[i]);
            // NOTE: Cannot perform this comparison since two tours
            //       may express the same route, but not be bit, by bit
            //       compatible. Eg the same route taken in reversed order
            // assert(t.succ[i] == solution->tour.succ[i]);
        }
        tour_destroy(&t);
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

static bool valid_starting_pair(const Instance *instance,
                                InsHeurNodePair *pair) {
    assert(pair->u != pair->v);
    if (pair->u == pair->v) {
        return false;
    }
    const double Q = instance->vehicle_cap;
    if ((instance->demands[pair->u] + instance->demands[pair->v]) <= Q) {
        return true;
    } else {
        return false;
    }
}

static bool random_insheur_starting_pair(const Instance *instance,
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
    assert(valid_starting_pair(instance, start_pair));
    return true;
}

static void ins_heur(Solver *solver, const Instance *instance,
                     Solution *solution, InsHeurNodePair starting_pair) {
    Tour *const tour = &solution->tour;

    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    const int32_t start = starting_pair.u;
    const int32_t end = starting_pair.v;

    assert(start >= 0 && start < n);
    assert(end >= 0 && end < n);
    assert(start != end);

    tour_clear(tour);
    tour->num_comps = 1;
    tour->comp[start] = 0;
    tour->comp[end] = 0;
    tour->succ[start] = end;
    tour->succ[end] = start;

    double cost = cptp_dist(instance, start, end) +
                  cptp_dist(instance, end, start) - instance->duals[start] -
                  instance->duals[end];
    double sum_demands = instance->demands[start] + instance->demands[end];

    while (true) {
        double best_delta_cost = 0.0;
        int32_t best_h = -1;
        int32_t best_a = -1;
        int32_t best_b = -1;

        // Scan for nodes to be inserted
        for (int32_t h = 0; h < n; h++) {
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

                if (h == 0 || delta_cost < best_delta_cost) {
                    best_delta_cost = delta_cost;
                    best_h = h;
                    best_a = a;
                    best_b = b;
                }
            }
        }

        if (best_h < 0) {
            assert(tour->comp[0] == 0);
            break;
        }

        assert(best_h >= 0 && best_h < n);
        assert(best_a >= 0 && best_a < n);
        assert(best_b >= 0 && best_b < n);
        assert(tour->succ[best_a] == best_b);

        if (best_delta_cost < 0.0) {
            cost += best_delta_cost;
            sum_demands += instance->demands[best_h];

            tour->comp[best_h] = 0;
            tour->succ[best_a] = best_h;
            tour->succ[best_h] = best_b;
        } else {
            assert(tour->comp[0] == 0);
            break;
        }

#ifndef NDEBUG
        if (tour->comp[0] == 0) {
            validate_tour(instance, tour);
        }
#endif
    }

    solution->upper_bound = cost;

#ifndef NDEBUG
    validate_tour(instance, tour);
    validate_solution(instance, solution);
#endif
}

bool mip_ins_heur_warm_start(Solver *solver, const Instance *instance) {
    bool result = true;
    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    Solution solution = solution_create(instance);
    InsHeurNodePair starting_pair;
    starting_pair.u = 0;

    for (starting_pair.v = 0; starting_pair.v < n; starting_pair.v++) {
        if (starting_pair.v == starting_pair.u) {
            continue;
        }
        if (valid_starting_pair(instance, &starting_pair)) {
            ins_heur(solver, instance, &solution, starting_pair);
        }
        log_info("%s :: Found a solution of cost %f (relative_cost = %f)",
                 __func__, solution.upper_bound,
                 solution.upper_bound - instance->zero_reduced_cost_threshold);

        if (!register_warm_solution(solver, instance, &solution)) {
            log_fatal("%s :: register_warm_solution_failed", __func__);
            result = false;
            goto terminate;
        }
    }

terminate:
    solution_destroy(&solution);
    return result;
}
