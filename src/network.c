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

// See:
// 1. https://en.wikipedia.org/wiki/Push%E2%80%93relabel_maximum_flow_algorithm
// 2. Goldberg, A.V., 1997. An efficient implementation of a scaling
//    minimum-cost flow algorithm. Journal of algorithms, 22(1), pp.1-29.
static void push_relabel_max_flow(void) {}

static inline double *flow(Network *net, int32_t i, int32_t j) {
    assert(i >= 0 && i < net->nnodes);
    assert(j >= 0 && j < net->nnodes);
    return &net->flow[i * net->nnodes + j];
}

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

    bool path_exists = false;

    while (
        (path_exists = is_sink_node_reachable(net, parent, visited, queue))) {
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

FlowBipartition ford_fulkerson_max_flow(Network *net) {
    int32_t s = net->source_vertex;
    int32_t t = net->sink_vertex;
}
