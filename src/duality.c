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
#include "network.h"

// NOTE(dparo):
//      From
//           Beasley, J.E., Christofides, N., 1989. An algorithm for the
//           resource constrained shortest path problem. Networks 19,
//           379–394. https://doi.org/10.1002/net.3230190402
//      they claim from their ""limited"" (whatever that means)
//      computational experience that these constants value
//      yielded good results
static const int32_t NUM_SUBGRAD_ITERS = 10;
static const double STEP_SIZE_SCALE_FAC = 0.25;

typedef struct {
    int32_t nnodes;
    int32_t *depth;
    int32_t *pred;
    double *dist;
    bool *visited;
} EsppCtx;

EsppCtx espp_ctx_create(int32_t nnodes) {
    EsppCtx result = {0};
    result.nnodes = nnodes;

    result.depth = malloc(nnodes * sizeof(*result.depth));
    result.dist = malloc(nnodes * sizeof(*result.dist));
    result.pred = malloc(nnodes * sizeof(*result.pred));
    result.visited = malloc(nnodes * sizeof(*result.visited));

    return result;
}

void espp_ctx_destroy(EsppCtx *ctx) {
    free(ctx->depth);
    free(ctx->dist);
    free(ctx->pred);
    free(ctx->visited);
    memset(ctx, 0, sizeof(*ctx));
}

typedef struct {
    double cost;
    int32_t length;
    int32_t *succ;
} EsppResult;

EsppResult espp_result_create(int32_t n) {
    EsppResult result = {0};
    result.succ = malloc(n * sizeof(*result.succ));
    return result;
}

void espp_result_destroy(EsppResult *result) {
    free(result->succ);
    memset(result, 0, sizeof(*result));
}

void espp_result_unpack_into_cptp_tour(const Instance *instance,
                                       const EsppResult *espp, Tour *tour) {
    const int32_t n = instance->num_customers;

    assert(espp->succ[0] > 0);
    assert(espp->succ[n] < 0);

    int32_t curr = 0;
    do {
        if (curr == n) {
            continue;
        }

        int32_t next = espp->succ[curr];
        tour->succ[curr] = next;
        tour->comp[curr] = 0;
    } while ((curr = espp->succ[curr]) >= 0);

#ifndef NDEBUG
    validate_tour(instance, tour, 2);
#endif
}

// Solves the ESPP problem, through a customized
// dijkstra algorithm, where we force to visit at least
// 2 customers
double espp_solve(Network *net, EsppCtx *ctx, EsppResult *result) {
    const int32_t n = net->nnodes;
    const int32_t source_vertex = 0;
    const int32_t sink_vertex = net->nnodes + 1;

#ifndef NDEBUG
    {
        // 1. Validate that the network is composed only of positive weights
        // 2. Assert symmetry for customers edges
        for (int32_t i = 1; i < n - 1; i++) {
            for (int32_t j = 1; j < n - 1; j++) {
                if (i != j) {
                    assert(*network_weight(net, i, j) >= 0);
                    assert(*network_weight(net, i, j) ==
                           *network_weight(net, j, i));
                }
            }
        }
    }
#endif

    ctx->dist[source_vertex] = 0;
    ctx->depth[source_vertex] = 0;
    ctx->visited[source_vertex] = true;

    // Visit customers connected to the source_vertex
    for (int32_t i = 0; i < n; i++) {
        if (i != source_vertex) {
            ctx->dist[i] = *network_weight(net, source_vertex, i);
            ctx->pred[i] = source_vertex;
            ctx->depth[i] = 1;
            ctx->visited[i] = false;
        }
    }

    // Find shortest path for all vertices,
    // by ensuring that the sink vertex is visited at least depth >= 3
    for (int32_t count = 0; count < n - 1; count++) {
        // Find vertex u achieving minimum distance cost
        int32_t u = -1;
        {
            double min = INFINITY;
            int32_t min_index = -1;
            for (int32_t v = 0; v < n; v++) {
                if (!ctx->visited[v] && ctx->dist[v] < min) {
                    min_index = v;
                    min = ctx->dist[v];
                }
            }
            u = min_index;
        }

        assert(u >= 0 && u < n);
        ctx->visited[u] = true;

        // Update dist[v] only if is not visited, there is an edge from u to v,
        // and the total weight of path from src to v through u is smaller
        // than current value of dist
        for (int32_t v = 0; v < n; v++) {
            if (!ctx->visited[v]) {
                double w = *network_weight(net, u, v);
                double d = ctx->dist[v] + w;

                bool update = d < ctx->dist[v] ||
                              (v == sink_vertex && ctx->depth[sink_vertex] < 3);

                if (update) {
                    ctx->dist[v] = ctx->dist[u] + w;
                    ctx->pred[v] = u;
                    ctx->depth[v] = ctx->depth[u] + 1;
                }
            }
        }
    }

    assert(ctx->depth[sink_vertex] >= 3);

#ifndef NDEBUG

    // Validate that all nodes have a depth set, are visited and have a
    // predecessor set
    for (int32_t i = 0; i < n; i++) {
        assert(ctx->depth[i] >= 0);
        assert(ctx->visited[i]);
        if (i != source_vertex) {
            assert(ctx->pred[i] >= 0);
        }
    }
    // Validate depth consistency
    for (int32_t sink = 0; sink < n; sink++) {
        // Walk the pred array backward to count the number of nodes
        double dist = 0.0;
        int32_t num_nodes = 0;

        for (int32_t curr = sink; curr >= 0 && curr != source_vertex;
             curr = ctx->pred[curr]) {
            ++num_nodes;
            if (ctx->pred[curr] >= 0) {
                dist += *network_weight(net, curr, ctx->pred[curr]);
            }
        }

        assert(num_nodes == ctx->depth[sink]);
        assert(feq(ctx->dist[sink], dist, 1e-5));
    }
#endif

    double cost = 0.0;

    for (int32_t i = 0; i < n; i++) {
        result->succ[i] = -1;
    }

    int32_t curr = sink_vertex;
    do {
        int32_t pred = ctx->pred[curr];
        result->succ[pred] = curr;
        cost += *network_weight(net, pred, curr);
        ++result->length;
    } while (ctx->pred[curr] != source_vertex);

    assert(result->succ[source_vertex] > 0);
    assert(result->succ[sink_vertex] < 0);
    // Assert that at least two customers are visited
    assert(result->succ[result->succ[source_vertex]] != sink_vertex);

    result->cost = cost;
    return cost;
}

static void init_espp_network_weights(Network *net, const Instance *instance,
                                      CptpLagrangianMultipliers lm) {
    const int32_t n = net->nnodes;

    // Construct the edges between the customers, and
    // default weight to infinity if edge is not between customers
    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            bool i_is_customer = (i >= 1) && (i < n - 1);
            bool j_is_customer = (j >= 1) && (j < n - 1);
            bool valid_espp_edge = (i != j) && (i_is_customer && j_is_customer);
            if (valid_espp_edge) {
                double w = cptp_duality_dist(instance, lm, i, j);
                *network_weight(net, i, j) = w;
            } else {
                *network_weight(net, i, j) = INFINITY;
            }
        }
    }

    // Set weight for outgoing edges from the source vertex,
    //  while excluding (0, n - 1) edge.
    for (int32_t i = 1; i < n - 1; i++) {
        assert(i != n - 1);
        double w = cptp_duality_dist(instance, lm, 0, i);
        *network_weight(net, 0, i) = w;
    }

    // Set weight for ingoing edges for the sink vertex,
    //  while excluding (0, n - 1) edge.
    for (int32_t i = 1; i < n - 1; i++) {
        assert(i != 0);
        double w = cptp_duality_dist(instance, lm, i, n - 1);
        *network_weight(net, i, n - 1) = w;
    }
}

static double
get_min_l1_for_positive_espp_weight(const Instance *instance,
                                    const CptpLagrangianMultipliers *lm,
                                    int32_t i, int32_t j) {
    const int32_t n = instance->num_customers + 1;
    i = i == n ? 0 : i;
    j = j == n ? 0 : j;
    double avg_demand = 0.5 * (instance->demands[i] + instance->demands[j]);

    // Min lagrangian multiplier ub for achieving >= 0 cost
    // for this edge
    double cij = cptp_reduced_cost(instance, i, j);
    double min_l1 = lm->l0 - cij / avg_demand;
    return min_l1;
}

static void fix_lagrange_multipliers(const Instance *instance,
                                     CptpLagrangianMultipliers *lm) {
    // The network has instance->num_customers + 2 nodes
    const int32_t n = instance->num_customers + 2;

    // Fix it between customers
    for (int32_t i = 1; i < n - 1; i++) {
        for (int32_t j = 1; j < n - 1; j++) {
            if (i == j) {
                continue;
            }

            double min_l1 =
                get_min_l1_for_positive_espp_weight(instance, lm, i, j);
            lm->l1 = MAX(lm->l1, min_l1);
        }
    }

    // Fix it between (0, i)
    for (int32_t i = 1; i < n - 1; i++) {
        double min_l1 = get_min_l1_for_positive_espp_weight(instance, lm, 0, i);
        lm->l1 = MAX(lm->l1, min_l1);
    }

    // Fix it between (i, n - 1)
    for (int32_t i = 1; i < n - 1; i++) {
        double min_l1 =
            get_min_l1_for_positive_espp_weight(instance, lm, i, n - 1);
        lm->l1 = MAX(lm->l1, min_l1);
    }
}

static void update_lagrange_multipliers(const Instance *instance,
                                        const EsppResult *espp,
                                        CptpLagrangianMultipliers *lm,
                                        const double B,
                                        const double best_primal_bound,
                                        const double dual_bound) {
    const double Q = instance->vehicle_cap;
    const int32_t n = instance->num_customers + 1;

    assert(espp->succ[0] > 0);
    assert(espp->succ[n] < 0);

    // Calculate subgradients
    double g = +B;
    double h = -Q;
    {
        int32_t curr = 0;
        do {
            int32_t next = espp->succ[curr];

            int32_t i = curr;
            int32_t j = next == n ? 0 : next;
            assert(i >= 0 && i < n);
            assert(j >= 0 && j < n);

            double avg_demand =
                0.5 * (instance->demands[i] + instance->demands[j]);

            g += -avg_demand;
            h += +avg_demand;
            curr = next;
        } while (curr != n);

        assert(espp->succ[curr] < 0);
    }

    double dy = best_primal_bound - dual_bound;
    double dx = g * g + h * h;
    double step_size = STEP_SIZE_SCALE_FAC * (dx / dy);

    printf("%s :: subgradients = {g = %f, h = %f}, step_size = %f\n", __func__,
           g, h, step_size);

    // Update the multipliers:
    lm->l0 = MAX(0.0, lm->l0 + step_size * g);
    lm->l1 = MAX(0.0, lm->l1 + step_size * h);

    printf("%s :: new lagrangians = {lb = %f, ub = %f}\n", __func__, lm->l0,
           lm->l1);
}

static double get_primal_bound(const Instance *instance,
                               const EsppResult *espp_result,
                               bool *primal_is_feasible) {
    const double Q = instance->vehicle_cap;
    const int32_t n = instance->num_customers + 1;

    assert(espp_result->length >= 3);
    assert(espp_result->succ[0] > 0);
    assert(espp_result->succ[n] < 0);

    double cost = 0.0;
    double demand_sum = 0.0;

    for (int32_t i = 0; i < n; i++) {
        if (espp_result->succ[i] > 0) {
            int32_t j = espp_result->succ[i];
            cost += cptp_dist(instance, i, j);
            cost -= instance->profits[i];
            demand_sum += instance->demands[i];
        }
    }

    bool feasible = false;

    if (demand_sum <= Q) {
        feasible = true;
    } else {
        feasible = false;
        cost = -INFINITY;
    }

#ifndef NDEBUG

    // Some more validations
    if (feasible) {
        Solution solution = solution_create(instance);
        solution.upper_bound = cost;
        solution.tour.num_comps = 1;

        // Unpack the ESPP into a CPTP solution tour, and validate the primal
        // bound consistency
        espp_result_unpack_into_cptp_tour(instance, espp_result,
                                          &solution.tour);

        validate_solution(instance, &solution, 2);
        solution_destroy(&solution);
    }
#endif

    *primal_is_feasible = feasible;
    return cost;
}

double duality_subgradient_find_lower_bound(const Instance *instance,
                                            double best_primal_bound,
                                            double B) {
    const double Q = instance->vehicle_cap;
    const int32_t n = instance->num_customers + 1;

    Network net = network_create(n + 1, false);
    EsppCtx ctx = espp_ctx_create(n + 1);
    EsppResult espp_result = espp_result_create(n + 1);

    double best_dual_bound = -INFINITY;
    CptpLagrangianMultipliers lm = {0};

    for (int32_t sg_it = 0; sg_it < NUM_SUBGRAD_ITERS; sg_it++) {
        // Fix the lagrangian multiplier associated with the vehicle
        // capacity upper bound, such that we generate a Network with positive
        // weights, which can be easily solved with Dijkstra
        // algorithm in Theta(n^2)
        fix_lagrange_multipliers(instance, &lm);

        init_espp_network_weights(&net, instance, lm);

        const double path_cost = espp_solve(&net, &ctx, &espp_result);
        const double dual_bound = path_cost + (lm.l0 * B - lm.l1 * Q);

        bool primal_is_feasible = false;
        const double primal_bound =
            get_primal_bound(instance, &espp_result, &primal_is_feasible);

        if (primal_is_feasible && primal_bound < best_primal_bound) {
            best_primal_bound = primal_bound;
        }

        if (dual_bound != INFINITY && dual_bound > best_dual_bound) {
            best_dual_bound = dual_bound;
        }

        printf("------ %s :: primal_bound = %f, dual_bound = %f\n", __func__,
               primal_bound, dual_bound);

        update_lagrange_multipliers(instance, &espp_result, &lm, B,
                                    best_primal_bound, dual_bound);
    }

    network_destroy(&net);
    espp_ctx_destroy(&ctx);
    espp_result_destroy(&espp_result);

    exit(0);

    return best_dual_bound;
}

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
            if (i != j) {
                out->edge_weight[sxpos(n, i, j)] =
                    cptp_duality_dist(instance, lm, i, j);
            }
        }
    }
}
