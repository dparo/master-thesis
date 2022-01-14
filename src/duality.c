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

#include "duality.h"
#include "core.h"
#include "validation.h"

void generate_dual_instance(const Instance *instance, Instance *out,
                            CptpLagrangianMultipliers lm) {
    int32_t n = instance->num_customers + 1;

    if (!out->profits || !out->demands) {
        instance_destroy(out);
        *out = instance_copy(instance, true, false);
    } else {
        *out = instance_copy(instance, false, false);
    }

    out->edge_weight = malloc(sizeof(*out->edge_weight) * hm_nentries(n));

    for (int32_t i = 0; i < n; i++) {
        // NOTE(dparo):
        //     In the dual formulation all profits associated to each
        //     city are cleared, and are instead encoded in the reduced
        //     cost associated to each arc.
        out->profits[i] = 0.0;
        // demands are instead copied as is
        out->demands[i] = instance->demands[i];
    }

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            out->edge_weight[sxpos(n, i, j)] =
                cptp_duality_dist(instance, lm, i, j);
        }
    }
}

static void
validate_duality_distance_is_positive(const Instance *instance,
                                      CptpLagrangianMultipliers lm) {
#ifndef NDEBUG
    const int32_t n = instance->num_customers + 1;

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (i == j) {
                continue;
            }
            assert(cptp_duality_dist(instance, lm, i, j) >= 0.0);
        }
    }

#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(lm);
#endif
}

static void validate_dual_problem_solution(const Instance *instance,
                                           CptpLagrangianMultipliers lm,
                                           Solution *dual_solution) {
#ifndef NDEBUG
    Instance dual_instance = {0};
    generate_dual_instance(instance, &dual_instance, lm);
    validate_solution(&dual_instance, dual_solution, 2);
    instance_destroy(&dual_instance);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(lm);
    UNUSED_PARAM(dual_solution);
#endif
}

static double solve_dual_problem(const Instance *instance,
                                 CptpLagrangianMultipliers lm,
                                 Solution *solution) {
    validate_duality_distance_is_positive(instance, lm);
    return INFINITY;
}

static double compute_feasible_primal_bound(const Instance *instance,
                                            Solution *solution) {
    validate_tour(instance, &solution->tour, 2);
    double result = 0.0;

    int32_t curr_vertex = 0;

    do {
        int32_t next_vertex = solution->tour.succ[curr_vertex];
        result += cptp_reduced_cost(instance, curr_vertex, next_vertex);
        curr_vertex = next_vertex;
    } while (curr_vertex != 0);

    return result;
}

double duality_subgradient_find_lower_bound(const Instance *instance,
                                            double best_primal_bound,
                                            double cap_lb) {
    const int32_t n = instance->num_customers + 1;

    // NOTE(dparo):
    //      From
    //           Beasley, J.E., Christofides, N., 1989. An algorithm for the
    //           resource constrained shortest path problem. Networks 19,
    //           379–394. https://doi.org/10.1002/net.3230190402
    //      they claim from their ""limited"" (whatever that means)
    //      computational experience that these constants value
    //      yielded good results
    const int32_t MAX_NUM_SUBGRAD_ITERS = 10;
    const double STEP_SIZE_SCALE_FAC = 0.25;

    Solution curr_dual_solution = solution_create(instance);

    double best_dual_bound = -INFINITY;
    CptpLagrangianMultipliers lm = {0};
    for (int32_t subgrad_it = 0; subgrad_it < MAX_NUM_SUBGRAD_ITERS;
         subgrad_it++) {

        // Fix the lagrangian multiplier associated with the vehicle
        // capacity upper bound, such we generate a Network with positive
        // associated weights, which can be easily solved with Dijkstra
        // algorithm in Theta(n^2)

        for (int32_t i = 0; i < n; i++) {
            for (int32_t j = 0; j < n; j++) {
                if (i == j) {
                    continue;
                }
                double avg_demand =
                    (instance->demands[i] + instance->demands[j]) / 2.0;

                // Min lagrangian multiplier ub for achieving >= 0 cost
                // for this edge
                double min_lm_cap_ub =
                    lm.cap_lb - cptp_reduced_cost(instance, i, j) / avg_demand;

                // Raise the lagrangian multiplier
                lm.cap_ub = MAX(lm.cap_ub, min_lm_cap_ub);
            }
        }

        solve_dual_problem(instance, lm, &curr_dual_solution);
#ifndef NDEBUG
        validate_dual_problem_solution(instance, lm, &curr_dual_solution);
#endif

        double feasible_dual_bound = curr_dual_solution.lower_bound;
        double feasible_primal_bound =
            compute_feasible_primal_bound(instance, &curr_dual_solution);

        if (feasible_primal_bound < best_primal_bound) {
            best_primal_bound = feasible_primal_bound;
        }

        if (feasible_dual_bound > best_dual_bound) {
            best_dual_bound = feasible_dual_bound;
        }

        bool reached_optimality =
            is_valid_reduced_cost(best_primal_bound) &&
            best_dual_bound >= (best_primal_bound - COST_TOLERANCE);

        if (reached_optimality) {
            break;
        }

        // Calculate subgradients
        double g = cap_lb;
        double h = -instance->vehicle_cap;
        {
            int32_t curr_vertex = 0;
            do {
                int32_t next_vertex = curr_dual_solution.tour.succ[curr_vertex];
                double di = instance->demands[curr_vertex];
                double dj = instance->demands[next_vertex];
                double avg_demand = 0.5 * (di + dj);
                g += -avg_demand;
                h += +avg_demand;

            } while (curr_vertex != 0);
        }

        double dy = best_primal_bound - feasible_dual_bound;
        double dx = g * g + h * h;
        double step_size = STEP_SIZE_SCALE_FAC * (dx / dy);

        // Update the multipliers:
        lm.cap_lb = MAX(0.0, lm.cap_lb + step_size * g);
        lm.cap_ub = MAX(0.0, lm.cap_ub + step_size * h);
    }

    solution_destroy(&curr_dual_solution);
    return best_dual_bound;
}
