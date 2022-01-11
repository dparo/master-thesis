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

#include "warm-start.h"
#include "validation.h"

typedef struct InsHeurNodePair {
    int32_t u, v;
} InsHeurNodePair;

static bool feed_warm_solution(Solver *solver, const Instance *instance,
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

static void ins_heur(const Instance *instance, Solution *solution,
                     InsHeurNodePair starting_pair) {
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
                  cptp_dist(instance, end, start) - instance->profits[start] -
                  instance->profits[end];
    double sum_demands = instance->demands[start] + instance->demands[end];

    int32_t num_visited = 2;

    while (true) {
        double best_delta_cost = -COST_TOLERANCE;
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
                    delta_cost = c_ah + c_hb - c_ab - instance->profits[h];
                } else {
                    // This city requires too much demand compared to what is
                    // the remainder capacity of the truck
                    delta_cost = INFINITY;
                }

                // NOTE(dparo):
                //     A vertex is a good candidate for insertion, considering
                //     it is not already visited, if it is the depot (must be
                //     visited due to the formulation of the problem), only 2
                //     nodes are visited in the current tour (the MIP
                //     formulation accepts only tours having at least 3 nodes
                //     visited) or if it improves the previous candidate
                //     delta_cost. Recall that the best_delta_cost starts from
                //     0.0, and therefore only improving vertices will be
                //     inserted.

                assert(!h_is_visited);

                // NOTE(dparo):
                //      Marking the depot node once (best_h = 0) as a
                //      candidate raises the best_delta_cost above 0, therefore
                //      allowing non improving vertices to now become valid
                //      candidate for insertions. Therefore make a smarter check
                //      if the delta_cost: if the best_h insertion candidate is
                //      the depot then compare the delta cost using the `0.0 -
                //      COST_TOLERANCE` threshold, otherwise do a conventional
                //      comparison against the previous best_delta_cost
                bool improving_delta_cost =
                    best_h == 0
                        ? delta_cost < -COST_TOLERANCE
                        : delta_cost < (best_delta_cost - COST_TOLERANCE);

                bool good_candidate_for_insertion =
                    h == 0 || num_visited == 2 || improving_delta_cost;

                if (good_candidate_for_insertion) {
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

        assert(best_delta_cost < 0.0 || num_visited == 2 || best_h == 0);
        cost += best_delta_cost;
        sum_demands += instance->demands[best_h];

        tour->comp[best_h] = 0;
        tour->succ[best_a] = best_h;
        tour->succ[best_h] = best_b;
        ++num_visited;

#ifndef NDEBUG
        if (tour->comp[0] == 0) {
            validate_tour(instance, tour);
            assert(feq(tour_eval(instance, tour), cost, 1e-5));
        }
#endif
    }

    solution->upper_bound = cost;

#ifndef NDEBUG
    validate_tour(instance, tour);
    validate_solution(instance, solution);

    // NOTE(dparo):
    //    Due to the MIP structure of the formulation,
    //    the MIP formulation can accept tours having at least 3 nodes visited
    {
        int32_t num_visited = 0;
        for (int32_t i = 0; i < n; i++) {
            if (tour->comp[i] >= 0) {
                assert(tour->succ[i] >= 0);
                ++num_visited;
            }
        }
        assert(num_visited >= 2);
    }
#endif
}

static void twoopt_exchange(Solver *solver, const Instance *instance,
                            Tour *tour, int32_t a, int32_t b) {
    const int32_t n = instance->num_customers + 1;
    assert(a >= 0 && a < n);
    assert(b >= 0 && b < n);

    int32_t succ_a = tour->succ[a];
    int32_t succ_b = tour->succ[b];

    assert(succ_a >= 0 && succ_a < n);
    assert(succ_b >= 0 && succ_b < n);

    // Reverse part of the tour
    {
        int32_t prev = succ_a;
        int32_t current = tour->succ[succ_a];
        assert(current >= 0 && current < n);

        while (current != succ_b) {
            int32_t next = tour->succ[current];
            assert(next >= 0 && next < n);
            tour->succ[current] = prev;
            prev = current;
            current = next;
        }
    }

    // Fix the edge crossing
    tour->succ[a] = b;
    tour->succ[succ_a] = succ_b;

#ifndef NDEBUG
    validate_tour(instance, tour);
#endif
}

static void twoopt_refine(Solver *solver, const Instance *instance,
                          Solution *solution) {
    const int32_t n = instance->num_customers + 1;

    // NOTE(dparo):
    //      A two-opt exchange maintain feasibiliby of the original solution.
    //      After a two-opt exchange the same set of vertices are visited,
    //      and therefore capacity constraints remain satisfied.
    //      On top of that profits do not change.
    //      The only thing that changes is the cost associated with the
    //      the travelling distance.

    //
    // While there are edges crossing (2-opt exchanges are available)
    //
    while (true) {
        double best_delta_cost = -COST_TOLERANCE;
        int32_t best_a = -1;
        int32_t best_b = -1;

        //
        // Scan for the best NodePair to perform the 2-opt exchange
        //
        for (int32_t a = 0; a < n; a++) {
            for (int32_t b = 0; b < n; b++) {
                if (a == b) {
                    continue;
                }

                // Vertices a,b are valid 2opt exchange candidates only if they
                // are visited in the tour
                if (solution->tour.comp[a] < 0 || solution->tour.comp[b] < 0) {
                    continue;
                }

                int32_t succ_a = solution->tour.succ[a];
                int32_t succ_b = solution->tour.succ[b];

                assert(succ_a >= 0 && succ_a < n);
                assert(succ_b >= 0 && succ_b < n);
                assert(succ_a != succ_b);

                // Compute delta_cost
                double delta_cost = cptp_dist(instance, a, b) +
                                    cptp_dist(instance, succ_a, succ_b) -
                                    cptp_dist(instance, a, succ_a) -
                                    cptp_dist(instance, b, succ_b);

                if (delta_cost < best_delta_cost) {
                    best_delta_cost = delta_cost;
                    best_a = a;
                    best_b = b;
                }
            }
        }

        if (best_a >= 0) {
            assert(best_delta_cost < 0.0);
            assert(best_b >= 0);

            // Perform the two-opt exchange and reverse part of the tour
            twoopt_exchange(solver, instance, &solution->tour, best_a, best_b);
            solution->upper_bound += best_delta_cost;
#ifndef NDEBUG
            validate_solution(instance, solution);
#endif
        } else {
            // NOTE(dparo): No more 2-opt exchanges are available
            break;
        }
    }

#ifndef NDEBUG
    validate_solution(instance, solution);
#endif
}

bool mip_ins_heur_warm_start(Solver *solver, const Instance *instance,
                             bool pricer_mode_enabled) {
    bool result = true;
    const int32_t n = instance->num_customers + 1;

    double min_ub_found = INFINITY;
    Solution solution = solution_create(instance);
    InsHeurNodePair starting_pair;

    starting_pair.u = 0;
    for (starting_pair.v = 0; starting_pair.v < n; starting_pair.v++) {
        if (starting_pair.v == starting_pair.u) {
            continue;
        }
        if (valid_starting_pair(instance, &starting_pair)) {
            ins_heur(instance, &solution, starting_pair);
            log_info("%s :: ins_heur -- found a solution of cost %f", __func__,
                     solution.upper_bound);

            // NOTE(dparo):
            //       Since 2opt refinements are not cheap, and may cost us
            //       computation time, try to do them only when absolutely
            //       necessary. This will keep the main execution path as
            //       fast as possible
            if (!pricer_mode_enabled ||
                !is_valid_reduced_cost(solution.upper_bound)) {
                // Try to improve the solution using 2opt
                double prev_ub = solution.upper_bound;
                twoopt_refine(solver, instance, &solution);
                log_trace("%s :: two opt refine -- Improved solution from %f "
                          "to %f (%f delta improvement)",
                          __func__, prev_ub, solution.upper_bound,
                          solution.upper_bound - prev_ub);
            }

            if (!feed_warm_solution(solver, instance, &solution)) {
                log_fatal("%s :: register_warm_solution_failed", __func__);
                result = false;
                goto terminate;
            }

            // NOTE(dparo):
            //     Whenever we find a reduced cost tour and we are in pricer
            //     mode, there's no point in continuining feeding other warm
            //     start solutions, we've already found what we are looking
            //     for. Break out of the loop.

            min_ub_found = MIN(min_ub_found, solution.upper_bound);

            if (pricer_mode_enabled && is_valid_reduced_cost(min_ub_found)) {
                log_info("%s :: Found reduced_cost tour (%f), early "
                         "terminating warm-start feeding\n",
                         __func__, min_ub_found);
                break;
            }
        }
    }

terminate:
    solution_destroy(&solution);
    return result;
}
