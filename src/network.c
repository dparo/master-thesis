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

/// \brief Just a shorter alias for network_flow
static inline double *flow(FlowNetwork *net, int32_t i, int32_t j) {
    return network_flow(net, i, j);
}

/// \brief Just a shorter alias for network_cap
static inline double *cap(FlowNetwork *net, int32_t i, int32_t j) {
    return network_cap(net, i, j);
}

static inline double residual_cap(FlowNetwork *net, int32_t i, int32_t j) {
    if (j > i) {
        return *flow(net, j, i);
    } else if (i < j) {
        return *cap(net, i, j) - *flow(net, i, j);
    } else {
        return 0.0;
    }
}

static inline double compute_flow_entering_vertex(FlowNetwork *net, int32_t j) {
    double sum = 0.0;
    for (int32_t i = 0; i < j; i++) {
        sum += *flow(net, i, j);
    }
    return sum;
}

static inline double compute_flow_exiting_vertex(FlowNetwork *net, int32_t i) {
    double sum = 0.0;

    for (int32_t j = i + 1; j < net->nnodes; j++) {
        sum += *flow(net, i, j);
    }

    return sum;
}

static inline double compute_excess_flow_of_vertex(FlowNetwork *net,
                                                   int32_t i) {
    return compute_flow_entering_vertex(net, i) -
           compute_flow_exiting_vertex(net, i);
}

static bool is_residual_edge(int32_t *height, int32_t i, int32_t j) {
    if (height[i] <= height[j] + 1) {
        return true;
    } else {
        return false;
    }
}

static void push(FlowNetwork *net, int32_t *height, double *excess_flow,
                 int32_t u, int32_t v) {
    UNUSED_PARAM(height);
    assert(excess_flow[u] > 0.0);
    assert(residual_cap(net, u, v) > 0.0);
    assert(height[u] == height[v] + 1);
    double delta = MIN(excess_flow[u], residual_cap(net, u, v));
    if (u < v) {
        *flow(net, u, v) += delta;
    } else {
        *flow(net, v, u) -= delta;
    }
    excess_flow[u] -= delta;
    excess_flow[v] += delta;

    // At the end of the push operation, the node u should no longer be
    // overflowing
    assert(excess_flow[u] == 0.0);
}

static void relabel(FlowNetwork *net, int32_t *height, double *excess_flow,
                    int32_t u) {
    assert(excess_flow[u] > 0.0);

#ifndef NDEBUG
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (u != v && is_residual_edge(height, u, v)) {
            assert(height[u] <= height[v]);
        }
    }
#endif

    int32_t min_height = INT32_MAX;
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (u != v && is_residual_edge(height, u, v)) {
            min_height = MIN(min_height, height[v]);
        }
    }

    assert(min_height != INT32_MAX);
    int32_t new_height = 1 + min_height;
    assert(new_height >= height[u] + 1);
    height[u] = new_height;
}

// See:
// 1. https://en.wikipedia.org/wiki/Push%E2%80%93relabel_maximum_flow_algorithm
// 2. Goldberg, A.V., 1997. An efficient implementation of a scaling
//    minimum-cost flow algorithm. Journal of algorithms, 22(1), pp.1-29.
static void push_relabel_max_flow(FlowNetwork *net) {
    validate_network_flow(net);
    int32_t s = net->source_vertex;
    int32_t t = net->sink_vertex;

    int32_t *height = malloc(net->nnodes * sizeof(*height));
    double *excess_flow = malloc(net->nnodes * sizeof(*excess_flow));

    // PREFLOW
    {
        for (int32_t i = 0; i < net->nnodes; i++) {
            excess_flow[i] = 0.0;
            height[i] = 0;
        }

        for (int32_t i = 0; i < net->nnodes; i++) {
            for (int32_t j = i + 1; j < net->nnodes; j++) {
                *flow(net, i, j) = 0.0;
            }
        }

        // For each vertex touching the source vertex s
        for (int32_t v = 0; v < s; v++) {
            double c = *cap(net, v, s);
            *flow(net, v, s) = c;
            excess_flow[v] = c;
            excess_flow[s] -= c;
        }

        for (int32_t v = s + 1; v < net->nnodes; v++) {
            double c = *cap(net, s, v);
            *flow(net, s, v) = c;
            excess_flow[v] = c;
            excess_flow[s] -= c;
        }

        height[s] = net->nnodes;
    }

    // While there exist push, or relabel op perform it
    bool done = false;
    while (!done) {
        int32_t cnt_overflowing = 0;
        for (int32_t i = 0; i < net->nnodes; i++) {
            if (excess_flow[i] > 0.0) {
                cnt_overflowing += 1;
                relabel(net, height, excess_flow, i);
            }
        }
        if (cnt_overflowing == 0) {
            done = true;
        }
    }
}

static bool is_sink_node_reachable(FlowNetwork *net, int32_t *parent,
                                   bool *visited, int32_t *queue) {
    int32_t s = net->source_vertex;
    int32_t t = net->sink_vertex;

    int32_t queue_begin = 0;
    int32_t queue_end = 0;

    // Enqueue
    queue[queue_end++] = s;
    visited[s] = s;

    while (queue_end - queue_begin > 0) {
        // Dequeue
        int32_t u = queue[queue_begin++];
        for (int32_t j = 0; j < net->nnodes; j++) {
            if (visited[j] == false && *flow(net, u, j) > 0) {
                // Enqueue
                queue[queue_end++] = j;
                visited[j] = true;
                parent[j] = u;
            }
        }
    }

    return visited[t];
}

// See: https://en.wikipedia.org/wiki/Ford%E2%80%93Fulkerson_algorithm
double edmond_karp_max_flow(FlowNetwork *net) {
    int32_t *parent = malloc(net->nnodes * sizeof(*parent));
    int32_t *queue = malloc(net->nnodes * sizeof(*queue));
    bool *visited = calloc(net->nnodes, sizeof(*visited));

    double max_flow = 0.0;

    for (int32_t i = 0; i < net->nnodes; i++) {
        parent[i] = -1;
    }

    while (is_sink_node_reachable(net, parent, visited, queue)) {
        double path_flow = INFINITY;
        int32_t i = net->sink_vertex;

        // Walk backwards
        while (i != net->source_vertex) {
            path_flow = MIN(path_flow, *flow(net, parent[i], i));
            i = parent[i];
        }

        max_flow += path_flow;

        // Update residual capacities of the edges and reverse edges
        int32_t v = net->sink_vertex;
        while (v != net->source_vertex) {
            int32_t u = parent[v];
            *flow(net, u, v) -= path_flow;
            *flow(net, v, u) += path_flow;
            v = parent[v];
        }

        memset(visited, 0, sizeof(*visited) * net->nnodes);
    }

    free(parent);
    free(visited);
    free(queue);
    return max_flow;
}

MaxFlowResult ford_fulkerson_max_flow(FlowNetwork *net, double initial_flow) {
    int32_t s = net->source_vertex;
    int32_t t = net->sink_vertex;

    double max_flow = initial_flow;
    if (max_flow < 0.0) {
        max_flow = 0.0;
        // Compute it ourselves
        for (int32_t i = 0; i < net->nnodes; i++) {
            for (int32_t j = 0; j < net->nnodes; j++) {
                max_flow += *flow(net, i, j);
            }
        }
    }
    int32_t *pred = malloc(net->nnodes * sizeof(*pred));
    double *eps = malloc(net->nnodes * sizeof(*eps));
    int32_t *queue = malloc(net->nnodes * sizeof(*queue));

    int32_t queue_begin = 0;
    int32_t queue_end = 0;

    for (int32_t i = 0; i < net->nnodes; i++) {
        pred[i] = -1;
    }

    do {
        eps[s] = INFINITY;
        pred[s] = s;
        // Enqueue s
        queue[queue_end++] = s;

        while ((queue_end - queue_begin > 0) && pred[t] < 0) {
            // Dequeue node
            int32_t h = queue[queue_begin++];
            for (int32_t j = 0; j < net->nnodes; j++) {
                double f = *flow(net, h, j);
                double c = *cap(net, h, j);
                if (f < c && pred[j] < 0) {
                    // Non saturated directed edges
                    eps[j] = MIN(eps[h], c - f);
                    pred[j] = h;
                    queue[queue_end++] = j;
                }
            }

            for (int32_t i = 0; i < net->nnodes; i++) {
                double f = *flow(net, i, h);
                if (f > 0 && pred[i] < 0) {
                    eps[i] = MIN(eps[h], f);
                    pred[i] = h;
                    queue[queue_end++] = i;
                }
            }
        }

        if (pred[t] >= 0) {
            double delta = eps[t];
            max_flow += delta;
            int32_t j = t;
            while (j != s) {
                int32_t i = pred[j];
                if (i > 0) {
                }
                j = i;
            }
        }
    } while (pred[t] < 0);

    free(pred);
    free(eps);
    free(queue);

    return (MaxFlowResult){0};
}
