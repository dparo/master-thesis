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
    result.nnodes = nnodes;
    result.colors = malloc(nnodes * sizeof(*result.colors));
    return result;
}

void max_flow_result_destroy(MaxFlowResult *m) {
    free(m->colors);
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
                result->colors[i] = (ctx->height[i] > h) ? BLACK : WHITE;
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
            int32_t li = (int32_t)result->colors[i];
            int32_t lj = (int32_t)result->colors[j];
            assert(feq(*flow(net, i, j), -*flow(net, j, i), EPS));
            double f = *flow(net, i, j);
            double c = *cap(net, i, j);
            assert(c >= 0.0);
            assert(flte(f, c, EPS));
            if (f >= 0) {
                if (li == BLACK && lj == WHITE) {
                    // All edges should be saturated
                    double r = residual_cap(net, i, j);
                    assert(feq(0.0, r, EPS));
                    section_flow += f;
                } else if (li == WHITE && lj == BLACK) {
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
        assert(result->colors);
        assert(result->nnodes == net->nnodes);

        result->maxflow = max_flow;
        result->nnodes = net->nnodes;
        compute_bipartition_from_height(net, result, ctx);

        // Assert that the cross section induced from the bipartition is
        // consistent with the computed maxflow
        validate_min_cut(net, result, max_flow);
    }

    result->source = source_vertex;
    result->sink = sink_vertex;
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
                result.sections[cut_idx].colors[i] = labels[i] ? BLACK : WHITE;
            }
            cut_idx += 1;
        }
    }
    assert(cut_idx == num_sections);

    free(labels);

    return result;
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

    for (int32_t i = 0; i < n; i++) {
        ctx->p[i] = 0;
        ctx->flows[i] = 0.0;
    }

    memset(output->reduced_net.cap, 0,
           n * n * sizeof(*output->reduced_net.cap));

    for (int32_t s = 1; s < n; s++) {

        int32_t t = ctx->p[s];
        double max_flow = push_relabel_max_flow2(net, s, t, &ctx->mf, &ctx->pr);

        assert(ctx->mf.colors[s] == BLACK);
        assert(ctx->mf.colors[t] == WHITE);

        ctx->flows[s] = max_flow;

        // Setup the next sink candidate for each vertex according to their
        // bipartition (s, t) as valid max_flow candidates.
        for (int32_t i = 0; i < n; i++) {
            bool black = ctx->mf.colors[i] == BLACK;
            bool white = ctx->mf.colors[i] == WHITE;
            if (i != s && ctx->p[i] == t && black) {
                ctx->p[i] = s;
            } else if (i != t && ctx->p[i] == s && white) {
                ctx->p[i] = t;
            }
        }

        // If the next sink candidate for t is of BLACK COLOR (eg belongs to the
        // s bipartition), fix the candidates, and swap the flows
        if (ctx->mf.colors[ctx->p[t]] == BLACK) {
            ctx->p[s] = ctx->p[t];
            ctx->p[t] = s;
            SWAP(double, ctx->flows[s], ctx->flows[t]);
        }
    }

    // Output the tree
    for (int32_t i = 1; i < n; i++) {
        double f = ctx->flows[i];
        int32_t u = ctx->p[i];
        *network_cap(&output->reduced_net, i, u) = f;
        *network_cap(&output->reduced_net, u, i) = f;
    }
}

bool gomory_hu_tree_ctx_create(GomoryHuTreeCtx *ctx, int32_t nnodes) {
    ctx->nnodes = nnodes;
    ctx->p = malloc(nnodes * sizeof(*ctx->p));
    ctx->flows = malloc(nnodes * sizeof(*ctx->flows));
    ctx->ff.visited = malloc(nnodes * sizeof(*ctx->ff.visited));
    ctx->ff.parent = malloc(nnodes * sizeof(*ctx->ff.parent));
    ctx->ff.bfs_queue = malloc((nnodes + 2) * sizeof(*ctx->ff.bfs_queue));
    ctx->mf = max_flow_result_create(nnodes);
    ctx->pr = push_relabel_ctx_create(nnodes);

    if (ctx->p && ctx->flows && ctx->ff.visited && ctx->ff.bfs_queue) {
        return true;
    } else {
        return false;
    }
}

GomoryHuTree gomory_hu_tree_create(int32_t nnodes) {
    GomoryHuTree tree = {0};
    tree.reduced_net = flow_network_create(nnodes);
    return tree;
}

void gomory_hu_tree_destroy(GomoryHuTree *tree) {
    flow_network_destroy(&tree->reduced_net);
}

void gomory_hu_tree_ctx_destroy(GomoryHuTreeCtx *ctx) {
    free(ctx->p);
    free(ctx->flows);
    free(ctx->ff.visited);
    free(ctx->ff.bfs_queue);
    free(ctx->ff.parent);
    max_flow_result_destroy(&ctx->mf);
    push_relabel_ctx_destroy(&ctx->pr);
    memset(ctx, 0, sizeof(*ctx));
}

static inline void dfs(FlowNetwork *net, MaxFlowResult *result, int32_t s) {
    result->colors[s] = BLACK;

    for (int32_t i = 0; i < net->nnodes; i++) {
        bool i_visited = result->colors[i] == BLACK;
        if (!i_visited && residual_cap(net, s, i) > 0.0) {
            dfs(net, result, i);
        }
    }
}

// Due to the Gomory Hu Tree max flow algorithm: only N max flows computations
// are required to build the tree. The tree is then a simpler data structure
// that can answer max flow computations for any (s,t) pair by a simple
// single iteration FordFulkerson's BFS algorithm. Because the tree is
// guaranteed to create a unique path between any pair of nodes
double gomory_hu_query(GomoryHuTree *tree, int32_t source, int32_t sink,
                       MaxFlowResult *result, GomoryHuTreeCtx *ctx) {
#if 0
    return push_relabel_max_flow2(&tree->reduced_net, source, sink, result,
                                  &ctx->pr);
#else
    const int32_t n = ctx->nnodes;
    result->source = source;
    result->sink = sink;

    memset(ctx->ff.visited, 0, n * sizeof(*ctx->ff.visited));
    flow_network_clear(&tree->reduced_net, false);

    // BREADTH First search
    // NOTE:
    //    A single BFS iteration is sufficient because only a unique flow path
    //    exists thanks to the gomory hu tree
    {
        int32_t *queue = ctx->ff.bfs_queue;

        int32_t head = 0;
        int32_t tail = 0;
        queue[tail++] = source;
        ctx->ff.visited[source] = true;
        ctx->ff.parent[source] = -1;

        // While queue is not empty
        while (head != tail) {
            // Pop vertex
            int32_t u = queue[head++];

            for (int32_t v = 0; v < n; v++) {
                bool explore_v = !ctx->ff.visited[v] &&
                                 residual_cap(&tree->reduced_net, u, v) > 0.0;
                if (explore_v) {
                    // Queue v for further processing, and mark it as "visited"
                    queue[tail++] = v;
                    ctx->ff.parent[v] = u;
                    ctx->ff.visited[v] = true;
                }
            }
        }
    }

    double max_flow = INFINITY;
    if (!ctx->ff.visited[sink]) {
        max_flow = 0.0;
    } else {
        // Ford fulkerson single iteration update
        // Walk the augmenting path backward, and find the minimum capacity
        // available in the path
        max_flow = INFINITY;
        for (int32_t v = sink; v != source; v = ctx->ff.parent[v]) {
            int32_t u = ctx->ff.parent[v];
            double rescap = residual_cap(&tree->reduced_net, u, v);
            max_flow = MIN(max_flow, rescap);
        }

        assert(max_flow != INFINITY);

        // Update residual capacities of the edges and reverse edges
        // along the path
        for (int32_t v = sink; v != source; v = ctx->ff.parent[v]) {
            int32_t u = ctx->ff.parent[v];
            *network_flow(&tree->reduced_net, u, v) += max_flow;
            *network_flow(&tree->reduced_net, v, u) -= max_flow;
        }
    }

    assert(max_flow != INFINITY);

    // DFS to determine the final bipartition
    for (int32_t i = 0; i < n; i++) {
        result->colors[i] = WHITE;
    }
    dfs(&tree->reduced_net, result, source);
    result->maxflow = max_flow;
    assert(result->colors[source] == BLACK);
    assert(result->colors[sink] == WHITE);
    return max_flow;
#endif
}

Network network_create(int32_t nnodes, bool clear_weights) {
    Network result = {0};
    result.nnodes = nnodes;

    if (clear_weights) {
        result.weights = calloc(nnodes * nnodes, sizeof(*result.weights));
    } else {
        result.weights = malloc(nnodes * nnodes * sizeof(*result.weights));
    }
    return result;
}

void network_destroy(Network *network) {
    free(network->weights);
    memset(network, 0, sizeof(*network));
}

DijkstraCtx dijkstra_ctx_create(int32_t nnodes) {
    DijkstraCtx result = {0};
    result.nnodes = nnodes;

    result.depth = malloc(nnodes * sizeof(*result.depth));
    result.dist = malloc(nnodes * sizeof(*result.dist));
    result.pred = malloc(nnodes * sizeof(*result.pred));

    return result;
}

void dijkstra_ctx_destroy(DijkstraCtx *ctx) {
    free(ctx->depth);
    free(ctx->dist);
    free(ctx->pred);
    memset(ctx, 0, sizeof(*ctx));
}

void dijkstra(Network *net, int32_t source_vertex, ShortestPath *result,
              DijkstraCtx *ctx) {
    const int32_t n = net->nnodes;

#ifndef NDEBUG
    // - Validate that the network is composed only of positive weights
    // - Assert symmetry
    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (i != j) {
                assert(*network_weight(net, i, j) >= 0);
                assert(*network_weight(net, i, j) ==
                       *network_weight(net, j, i));
            }
        }
    }
#endif

    for (int32_t i = 0; i < n; i++) {
        ctx->dist[i] = INFINITY;
        ctx->depth[i] = -1;
        ctx->pred[i] = -1;
    }

    ctx->dist[source_vertex] = 0;
    ctx->depth[source_vertex] = 0;

    // Find shortest path for all vertices
    for (int32_t count = 0; count < n - 1; count++) {
        // Find vertex u achieving minimum distance cost
        int32_t u = -1;
        {
            double min = INFINITY;
            int32_t min_index = -1;
            for (int32_t v = 0; v < n; v++) {
                if (ctx->depth[v] < 0 && ctx->dist[v] < min) {
                    min_index = v;
                    min = ctx->dist[v];
                }
            }
            u = min_index;
        }

        assert(u >= 0);

        // Update dist[v] only if is not visited, there is an edge from u to v,
        // and the total weight of path from src to v through u is smaller
        // than current value of dist
        for (int32_t v = 0; v < n; v++) {
            if (ctx->depth[v] < 0) {
                double w = *network_weight(net, u, v);
                double d = ctx->dist[v] + w;
                if (d < ctx->dist[v]) {
                    ctx->dist[v] = ctx->dist[u] + w;
                    ctx->pred[v] = u;
                    ctx->depth[v] = ctx->depth[u] + 1;
                }
            }
        }

        assert(u >= 0 && u < n);
    }

#ifndef NDEBUG
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

    if (result) {
        assert(result->nnodes == net->nnodes);

        result->nnodes = net->nnodes;
        result->source = source_vertex;
    }
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
