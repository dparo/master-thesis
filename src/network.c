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

#include "network.h"
#include <memory.h>

static const double EPS = 1e-6;

MaxFlowResult max_flow_result_create(int32_t nnodes) {
    MaxFlowResult result = {0};
    result.bipartition.nnodes = nnodes;
    result.bipartition.data = malloc(nnodes * sizeof(*result.bipartition.data));
    return result;
}

void max_flow_result_destroy(MaxFlowResult *m) {
    free(m->bipartition.data);
    memset(m, 0, sizeof(*m));
}

FlowNetwork flow_network_create(int32_t nnodes) {
    FlowNetwork net = {0};
    net.nnodes = nnodes;

    int32_t nsquared = nnodes * nnodes;
    net.flow = calloc(nsquared, sizeof(*net.flow));
    net.cap = calloc(nsquared, sizeof(*net.cap));

    if (!net.flow || !net.cap) {
        flow_network_destroy(&net);
    }

    return net;
}

void flow_network_clear(FlowNetwork *net, bool clear_cap) {
    int32_t nsquared = net->nnodes * net->nnodes;
    memset(net->flow, 0, nsquared * sizeof(*net->flow));
    if (clear_cap) {
        // NOTE: We do not need to pay for the reset since in most cases
        //       we are going to manually populate the matrix in its entirety
        //       anyway.
        memset(net->cap, 0, nsquared * sizeof(*net->cap));
    }
}

void flow_network_destroy(FlowNetwork *net) {
    if (net->flow) {
        free(net->flow);
    }
    if (net->cap) {
        free(net->cap);
    }

    memset(net, 0, sizeof(*net));
}

/// \brief Just a shorter alias for network_flow
static inline double *flow(FlowNetwork *net, int32_t i, int32_t j) {
    return network_flow(net, i, j);
}

/// \brief Just a shorter alias for network_cap
static inline double *cap(FlowNetwork *net, int32_t i, int32_t j) {
    return network_cap(net, i, j);
}

static inline double residual_cap(FlowNetwork *net, int32_t i, int32_t j) {
    assert(feq(*flow(net, i, j), -*flow(net, j, i), EPS));
    double result = *cap(net, i, j) - *flow(net, i, j);
    return result;
}

static inline double flow_entering(FlowNetwork *net, int32_t i) {
    double sum = 0.0;
    for (int32_t j = 0; j < net->nnodes; j++) {
        if (i != j) {
            double f = *flow(net, j, i);
            if (f >= 0) {
                sum += f;
            }
        }
    }
    return sum;
}

static inline double flow_exiting(FlowNetwork *net, int32_t i) {
    double sum = 0.0;
    for (int32_t j = 0; j < net->nnodes; j++) {
        if (i != j) {
            double f = *flow(net, i, j);
            if (f >= 0) {
                sum += f;
            }
        }
    }
    return sum;
}

static bool can_push(FlowNetwork *net, PushRelabelCtx *ctx, int32_t u,
                     int32_t v) {
    if ((ctx->height[u] == (ctx->height[v] + 1)) &&
        (residual_cap(net, u, v) > 0.0)) {
        return true;
    } else {
        return false;
    }
}

// Push flow
static void push(FlowNetwork *net, PushRelabelCtx *ctx, int32_t u, int32_t v) {
    assert(ctx->excess_flow[u] > 0.0);

    assert(u != v);

    assert(ctx->height[u] == ctx->height[v] + 1);
    double rescap = residual_cap(net, u, v);
    assert(rescap > 0.0);
    double delta = MIN(ctx->excess_flow[u], rescap);

    assert(feq(*flow(net, u, v), -*flow(net, v, u), EPS));
    assert(flte(*flow(net, u, v), *cap(net, u, v), EPS));
    assert(flte(*flow(net, v, u), *cap(net, v, u), EPS));
    *flow(net, u, v) += delta;
    *flow(net, v, u) -= delta;
    assert(flte(*flow(net, u, v), *cap(net, u, v), EPS));
    assert(flte(*flow(net, v, u), *cap(net, v, u), EPS));
    assert(feq(*flow(net, u, v), -*flow(net, v, u), EPS));

    ctx->excess_flow[u] -= delta;
    ctx->excess_flow[v] += delta;
}

// Increase the node height
static void relabel(FlowNetwork *net, PushRelabelCtx *ctx, int32_t u) {
    assert(ctx->excess_flow[u] > 0.0);

#ifndef NDEBUG
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (u != v && residual_cap(net, u, v) > 0.0) {
            assert(ctx->height[u] <= ctx->height[v]);
        }
    }
#endif
    assert(u != ctx->source_vertex && u != ctx->sink_vertex);

    int32_t min_height = INT32_MAX;
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (residual_cap(net, u, v) > 0) {
            min_height = MIN(min_height, ctx->height[v]);
        }
    }

    assert(min_height != INT32_MAX);
    int32_t new_height = 1 + min_height;
    assert(new_height >= ctx->height[u] + 1);
    ctx->height[u] = new_height;
    assert(ctx->height[u] < 2 * net->nnodes - 1);
}

static void discharge(FlowNetwork *net, PushRelabelCtx *ctx, int32_t u) {
    assert(u != ctx->source_vertex && u != ctx->sink_vertex);
    while (fgt(ctx->excess_flow[u], 0.0, EPS)) {
        int32_t v = ctx->curr_neigh[u];
        if (v >= net->nnodes) {
            relabel(net, ctx, u);
            ctx->curr_neigh[u] = 0;
        } else if (can_push(net, ctx, u, v)) {
            push(net, ctx, u, v);
        } else {
            ctx->curr_neigh[u] += 1;
        }
    }
}

static void greedy_preflow(FlowNetwork *net, PushRelabelCtx *ctx) {
    int32_t s = ctx->source_vertex;

    for (int32_t i = 0; i < net->nnodes; i++) {
        ctx->excess_flow[i] = 0.0;
        ctx->height[i] = 0;
    }

    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            *flow(net, i, j) = 0.0;
        }
    }

    // For each edge leaving the source s, saturate all out-arcs of s
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (v == s) {
            continue;
        }

        double c = *cap(net, s, v);
        assert(c >= 0.0);
        *flow(net, s, v) = c;
        *flow(net, v, s) = -c;
        ctx->excess_flow[v] = c;
        ctx->excess_flow[s] -= c;
    }

    ctx->height[s] = net->nnodes;
}

static void compute_bipartition_from_height(FlowNetwork *net,
                                            MaxFlowResult *result,
                                            PushRelabelCtx *ctx) {

    for (int32_t h = net->nnodes; h >= 0; h--) {
        bool found = false;
        for (int32_t i = 0; i < net->nnodes; i++) {
            if (ctx->height[i] == h) {
                found = true;
                break;
            }
        }
        if (!found) {
            for (int32_t i = 0; i < net->nnodes; i++) {
                result->bipartition.data[i] = ctx->height[i] > h;
            }
            break;
        }
    }
}

static double get_flow_from_source_node(FlowNetwork *net, PushRelabelCtx *ctx) {
    int32_t s = ctx->source_vertex;
    double max_flow = 0.0;
    for (int32_t i = 0; i < net->nnodes; i++) {
        if (i == s) {
            continue;
        }
        max_flow += *flow(net, s, i);
    }

    // Round to 0.0 if close
    if (feq(max_flow, 0.0, EPS)) {
        max_flow = 0.0;
    }

    assert(max_flow >= 0.0);
    return max_flow;
}

static void validate_flow(FlowNetwork *net, PushRelabelCtx *ctx,
                          double max_flow) {
#ifndef NDEBUG
    int32_t s = ctx->source_vertex;
    int32_t t = ctx->sink_vertex;

    for (int32_t i = 0; i < net->nnodes; i++) {
        double fenter = flow_entering(net, i);
        double fexit = flow_exiting(net, i);

        if (i == s) {
            assert(feq(fexit - fenter, max_flow, EPS));
        } else if (i == t) {
            assert(feq(fenter - fexit, max_flow, EPS));
        } else {
            // This assertion is only valid for all vertices except {s, t}.
            // This is verified in the CLRS (Introduction to algorithms) book
            assert(feq(ctx->excess_flow[i], 0.0, EPS));
            // Verify flow entering node i is equal to flow exiting node i
            assert(feq(fenter, fexit, EPS));
        }
    }

    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            // Assert flow on edge (i, j) does not exceed the capacity of edge
            // (i, j)
            assert(flte(*flow(net, i, j), *cap(net, i, j), EPS));
            assert(feq(*flow(net, i, j), -*flow(net, j, i), EPS));
        }
    }
#else
    UNUSED_PARAM(net);
    UNUSED_PARAM(ctx);
    UNUSED_PARAM(max_flow);
#endif
}

static void validate_min_cut(FlowNetwork *net, MaxFlowResult *result,
                             double max_flow) {
#ifndef NDEBUG
    double section_flow = 0.0;
    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            int32_t li = (int32_t)result->bipartition.data[i];
            int32_t lj = (int32_t)result->bipartition.data[j];
            assert(feq(*flow(net, i, j), -*flow(net, j, i), EPS));
            double f = *flow(net, i, j);
            double c = *cap(net, i, j);
            assert(c >= 0.0);
            assert(flte(f, c, EPS));
            if (f >= 0) {
                if (li == 1 && lj == 0) {
                    // All edges should be saturated
                    double r = residual_cap(net, i, j);
                    assert(feq(0.0, r, EPS));
                    section_flow += f;
                } else if (li == 0 && lj == 1) {
                    // All edges should be drained
                    assert(feq(f, 0, EPS));
                    section_flow -= f;
                }
            }
        }
    }
    assert(feq(section_flow, max_flow, EPS));
#else
    UNUSED_PARAM(net);
    UNUSED_PARAM(result);
    UNUSED_PARAM(max_flow);
#endif
}

double push_relabel_max_flow2(FlowNetwork *net, int32_t source_vertex,
                              int32_t sink_vertex, MaxFlowResult *result,
                              PushRelabelCtx *ctx) {
    assert(net->cap);
    assert(net->flow);
    assert(net->nnodes >= 2);
    assert(source_vertex != sink_vertex);

    ctx->source_vertex = source_vertex;
    ctx->sink_vertex = sink_vertex;

    int32_t s = source_vertex;
    int32_t t = sink_vertex;

#ifndef NDEBUG
    for (int32_t i = 0; i < net->nnodes; i++) {
        assert(*cap(net, i, i) == 0.0);
    }
#endif

    // PREFLOW
    flow_network_clear(net, false);
    greedy_preflow(net, ctx);

    for (int32_t i = 0; i < net->nnodes; i++) {
        ctx->curr_neigh[i] = 0;
    }

    ctx->list_len = 0;
    for (int32_t i = 0; i < net->nnodes; i++) {
        if (i != s && i != t) {
            ctx->list[ctx->list_len++] = i;
        }
    }

    // MAIN LOOP
    {
        int32_t curr_node = 0;
        while (curr_node < ctx->list_len) {
            int32_t u = ctx->list[curr_node];
            int32_t old_height = ctx->height[u];
            discharge(net, ctx, u);
            if (ctx->height[u] > old_height) {
                // Make space at the start of the list to move u at the front
                memmove(ctx->list + 1, ctx->list,
                        curr_node * sizeof(*ctx->list));
                ctx->list[0] = u;
                assert(feq(ctx->excess_flow[u], 0.0, EPS));
                curr_node = 1;
            } else {
                curr_node += 1;
            }
        }
    }

    // COMPUTE maxflow: Sum the flow of outgoing edges from s
    double max_flow = get_flow_from_source_node(net, ctx);

    validate_flow(net, ctx, max_flow);

    if (result) {
        assert(result->bipartition.data);
        assert(result->bipartition.nnodes == net->nnodes);

        result->maxflow = max_flow;
        result->bipartition.nnodes = net->nnodes;
        compute_bipartition_from_height(net, result, ctx);

        // Assert that the cross section induced from the bipartition is
        // consistent with the computed maxflow
        validate_min_cut(net, result, max_flow);
    }
    return max_flow;
}

PushRelabelCtx push_relabel_ctx_create(int32_t nnodes) {
    PushRelabelCtx ctx = {0};
    ctx.height = malloc(nnodes * sizeof(*ctx.height));
    ctx.excess_flow = malloc(nnodes * sizeof(*ctx.excess_flow));
    ctx.curr_neigh = malloc(nnodes * sizeof(*ctx.curr_neigh));
    ctx.list = malloc((nnodes - 2) * sizeof(*ctx.list));

#ifndef NDEBUG
    // randomly initialize the array to make accumulation errors apparent
    for (int32_t i = 0; i < nnodes; i++) {
        ctx.height[i] = rand();
        ctx.excess_flow[i] = (double)rand() / RAND_MAX;
        ctx.curr_neigh[i] = rand();
    }

    for (int32_t i = 0; i < nnodes - 2; i++) {
        ctx.list[i] = rand();
    }
#endif

    return ctx;
}

void push_relabel_ctx_destroy(PushRelabelCtx *ctx) {
    free(ctx->height);
    free(ctx->excess_flow);
    free(ctx->curr_neigh);
    free(ctx->list);
    memset(ctx, 0, sizeof(*ctx));
}

// This implementation uses the relabel-to-front max flow algorithm version
// See:
// 1. https://en.wikipedia.org/wiki/Push%E2%80%93relabel_maximum_flow_algorithm
// 2. Goldberg, A.V., 1997. An efficient implementation of a scaling
//    minimum-cost flow algorithm. Journal of algorithms, 22(1), pp.1-29.
double push_relabel_max_flow(FlowNetwork *net, int32_t source_vertex,
                             int32_t sink_vertex, MaxFlowResult *result) {

    PushRelabelCtx ctx = push_relabel_ctx_create(net->nnodes);
    double max_flow =
        push_relabel_max_flow2(net, source_vertex, sink_vertex, result, &ctx);
    push_relabel_ctx_destroy(&ctx);
    return max_flow;
}

static double compute_flow_from_labels(FlowNetwork *net, int32_t *labels) {
    double flow = 0.0;

    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            if (i == j) {
                continue;
            }
            if (labels[i] == 1 && labels[j] == 0) {
                flow += *network_cap(net, i, j);
            }
        }
    }
    return flow;
}

BruteforceMaxFlowResult max_flow_bruteforce(FlowNetwork *net,
                                            int32_t source_vertex,
                                            int32_t sink_vertex) {
    int32_t s = source_vertex;
    int32_t t = sink_vertex;
    assert(net->nnodes >= 2 && net->nnodes <= 10);
    int32_t *labels = calloc(net->nnodes, sizeof(*labels));

    int32_t num_sections = 0;
    double max_flow = INFINITY;

    for (int32_t label_it = 0; label_it < 1 << net->nnodes; label_it++) {
        for (int32_t k = 0; k < net->nnodes; k++) {
            labels[k] = (label_it & (1 << k)) >> k;
        }
        if (labels[s] != 1 || labels[t] != 0) {
            continue;
        }

        labels[s] = 1;
        labels[t] = 0;

        double flow = compute_flow_from_labels(net, labels);
        if (flte(flow, max_flow, EPS)) {
            if (feq(flow, max_flow, EPS)) {
                num_sections += 1;
            } else {
                max_flow = MIN(max_flow, flow);
                num_sections = 1;
            }
        }
    }

    BruteforceMaxFlowResult result = {0};
    result.maxflow = max_flow;
    result.num_sections = num_sections;
    result.sections = malloc(result.num_sections * sizeof(*result.sections));

    for (int32_t i = 0; i < result.num_sections; i++) {
        result.sections[i] = max_flow_result_create(net->nnodes);
        result.sections[i].maxflow = max_flow;
    }

    // Populate the sections
    int32_t cut_idx = 0;
    for (int32_t label_it = 0; label_it < 1 << net->nnodes; label_it++) {
        for (int32_t k = 0; k < net->nnodes; k++) {
            labels[k] = (label_it & (1 << k)) >> k;
        }
        if (labels[s] != 1 || labels[t] != 0) {
            continue;
        }

        labels[s] = 1;
        labels[t] = 0;

        double flow = compute_flow_from_labels(net, labels);

        if (feq(flow, max_flow, EPS)) {
            for (int32_t i = 0; i < net->nnodes; i++) {
                assert(cut_idx < num_sections);
                result.sections[cut_idx].bipartition.data[i] = labels[i];
            }
            cut_idx += 1;
        }
    }
    assert(cut_idx == num_sections);

    free(labels);

    return result;
}

void gomory_hu_tree_contract(FlowNetwork *net, GomoryHuTree *output,
                             GomoryHuTreeCtx *ctx) {}

void gomory_hu_tree_split(FlowNetwork *net, GomoryHuTree *output,
                          GomoryHuTreeCtx *ctx, int32_t s, int32_t t) {
    double max_flow =
        push_relabel_max_flow2(net, s, t, &ctx->mfr, &ctx->pr_ctx);
}

static double ght_modified_max_flow(FlowNetwork *net, GomoryHuTree *output,
                                    GomoryHuTreeCtx *ctx) {
    assert(!"TODO");
    return 0.0;
}

enum {
    BLACK = 0,
    GRAY = 1,
    WHITE = 2,
};

static void gomory_hu_tree_using_ford_fulkerson(FlowNetwork *net,
                                                GomoryHuTree *output,
                                                GomoryHuTreeCtx *ctx) {
    ATTRIB_MAYBE_UNUSED const int32_t n = net->nnodes;
    for (int32_t i = 0; i < n; i++) {
        ctx->ff.p[i] = 0;
        ctx->ff.flows[i] = 0.0;
        for (int32_t j = 0; j < n; j++) {
            *network_cap(&ctx->ff.reduced_net, i, j) = 0.0;
        }
    }

    for (int32_t s = 1; s < n; s++) {
        int32_t t = ctx->ff.p[s];

        double max_flow = ght_modified_max_flow(net, output, ctx);
        ctx->ff.flows[s] = max_flow;

        for (int32_t i = 0; i < n; i++) {
            if (i != s && ctx->ff.p[i] == t) {
                if (ctx->ff.colors[i] == BLACK) {
                    ctx->ff.p[i] = s;
                }
            }
        }

        if (s == n - 1) {
            for (int32_t i = 1; i < s + 1; i++) {
                // Produce the tree
                double f = ctx->ff.flows[i];
                int32_t u = ctx->ff.p[i];
                *network_cap(&ctx->ff.reduced_net, i, u) = f;
                *network_cap(&ctx->ff.reduced_net, u, i) = f;
            }
        }
    }
}

void gomory_hu_tree2(FlowNetwork *net, GomoryHuTree *output,
                     GomoryHuTreeCtx *ctx) {
    ATTRIB_MAYBE_UNUSED const int32_t n = net->nnodes;

#ifndef NDEBUG

    // IMPORTANT:
    //     This implementation only works with undirected graphs.
    //     Since the FlowNetwork allows representations of directed graphs
    //     We are going to assert that the network is undirected here
    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (i != j) {
                assert(*network_cap(net, i, j) == *network_cap(net, j, i));
            }
        }
    }

#endif

#if 1
    gomory_hu_tree_using_ford_fulkerson(net, output, ctx);
#else

    ctx->num_edges = 0;
    ctx->num_sets = 1;
    ctx->sets_size[0] = n;

    for (int32_t i = 0; i < n; i++) {
        ctx->sets[i] = 0;
    }

    while (true) {
        int32_t set = -1;
        for (int32_t c = 0; c < ctx->num_sets; c++) {
            if (ctx->sets_size[c] >= 2) {
                set = c;
                break;
            }
        }

        if (set < 0) {
            // break: We are done!
            break;
        }

        const bool apply_contraction = ctx->num_sets > 1;
        if (apply_contraction) {
            gomory_hu_tree_contract(net, output, ctx);
        }

        //
        // Choose two vertices s, t ∈ X and find a minimum s-t cut (A',B') in G'
        //
        // TODO: Pick two random source and sink indices in the candidate set
        int32_t s_idx = -1;
        int32_t t_idx = -1;

        s_idx = rand() % ctx->sets_size[set];

        do {
            t_idx = rand() % ctx->sets_size[set];
        } while (t_idx == s_idx);

        // Convert the s_idx, t_idx, to their corresponding node in the original
        // flow formulation
        int32_t s = -1;
        int32_t t = -1;
        {
            int32_t cnt = 0;
            for (int32_t i = 0; i < n; i++) {
                if (ctx->sets[i] == set) {
                    if (cnt == s_idx) {
                        s = cnt;
                    } else if (cnt == t_idx) {
                        t = cnt;
                    }
                    ++cnt;
                }
            }
            assert(cnt == ctx->sets_size[set]);
        }

        assert(s >= 0 && s < n);
        assert(t >= 0 && t < n);
        assert(s != t);

        gomory_hu_tree_split(net, output, ctx, s, t);
    }

    // Output the final tree
    assert(ctx->num_edges == n - 1);
    assert(ctx->num_sets == n);

    output->nedges = 0;

    for (int32_t edge_idx = 0; edge_idx < ctx->num_edges; edge_idx++) {
        int32_t set1 = ctx->edges[edge_idx].u;
        int32_t set2 = ctx->edges[edge_idx].v;
        assert(set1 >= 0 && set1 < ctx->num_sets);
        assert(set2 >= 0 && set2 < ctx->num_sets);
        assert(set1 != set2);

        int32_t u = -1;
        int32_t v = -1;

        // Convert the sets to their corresponding unique node
        for (int32_t i = 0; i < n; i++) {
            if (ctx->sets[i] == set1) {
                u = i;
            } else if (ctx->sets[i] == set2) {
                v = i;
            }
        }

        assert(u >= 0 && u < n);
        assert(v >= 0 && v < n);
        assert(u != v);

        output->edges[output->nedges++] = (GomoryHuEdge){u, v};
    }

    assert(output->nedges == n - 1);
#endif
}

bool gomory_hu_tree_ctx_create(GomoryHuTreeCtx *ctx, int32_t nnodes) {
    ctx->ff.p = malloc(nnodes * sizeof(*ctx->ff.p));
    ctx->ff.flows = malloc(nnodes * sizeof(*ctx->ff.flows));
    ctx->ff.colors = malloc(nnodes * sizeof(*ctx->ff.colors));
    ctx->ff.bfs_queue = malloc(nnodes * sizeof(*ctx->ff.bfs_queue));

    ctx->ff.reduced_net = flow_network_create(nnodes);
    if (ctx->ff.p && ctx->ff.flows && ctx->ff.colors && ctx->ff.bfs_queue &&
        ctx->ff.reduced_net.flow && ctx->ff.reduced_net.cap) {
        return true;
    } else {
        return false;
    }
}

void gomory_hu_tree_ctx_destroy(GomoryHuTreeCtx *ctx) {
    free(ctx->ff.p);
    free(ctx->ff.flows);
    free(ctx->ff.colors);
    free(ctx->ff.bfs_queue);
    flow_network_destroy(&ctx->ff.reduced_net);
    memset(ctx, 0, sizeof(*ctx));
}

bool gomory_hu_tree(FlowNetwork *net, GomoryHuTree *output) {
    GomoryHuTreeCtx ctx = {0};

    bool create_success = gomory_hu_tree_ctx_create(&ctx, net->nnodes);
    if (create_success) {
        gomory_hu_tree2(net, output, &ctx);
    }

    gomory_hu_tree_ctx_destroy(&ctx);
    return create_success;
}
