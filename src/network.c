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
static inline double *flow(Network *net, int32_t i, int32_t j) {
    return network_flow(net, i, j);
}

/// \brief Just a shorter alias for network_cap
static inline double *cap(Network *net, int32_t i, int32_t j) {
    return network_cap(net, i, j);
}

// See:
// 1. https://en.wikipedia.org/wiki/Push%E2%80%93relabel_maximum_flow_algorithm
// 2. Goldberg, A.V., 1997. An efficient implementation of a scaling
//    minimum-cost flow algorithm. Journal of algorithms, 22(1), pp.1-29.
static void push_relabel_max_flow(void) {}

__attribute__((alias("network_flow")));

static bool is_sink_node_reachable(Network *net, int32_t *parent, bool *visited,
                                   int32_t *queue) {
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
double edmond_karp_max_flow(Network *net) {
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

MaxFlowResult ford_fulkerson_max_flow(Network *net, double initial_flow) {
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
