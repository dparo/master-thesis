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

void flow_network_clear(FlowNetwork *net) {
    int32_t nsquared = net->nnodes * net->nnodes;
    memset(net->flow, 0, nsquared * sizeof(*net->flow));
    if (0) {
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
    assert(fcmp(*flow(net, i, j), -*flow(net, j, i), 1e-4));
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

static bool can_push(FlowNetwork *net, double *excess_flow, int32_t *height,
                     int32_t u, int32_t v) {
    if ((height[u] == (height[v] + 1)) && (residual_cap(net, u, v) > 0.0)) {
        return true;
    } else {
        return false;
    }
}

// Push flow
static void push(FlowNetwork *net, int32_t *height, double *excess_flow,
                 int32_t u, int32_t v) {
    UNUSED_PARAM(height);
    assert(excess_flow[u] > 0.0);

    assert(u != v);

    assert(height[u] == height[v] + 1);
    double rescap = residual_cap(net, u, v);
    assert(rescap > 0.0);
    double delta = MIN(excess_flow[u], rescap);

    assert(fcmp(*flow(net, u, v), -*flow(net, v, u), 1e-4));
    assert(flte(*flow(net, u, v), *cap(net, u, v), 1e-4));
    assert(flte(*flow(net, v, u), *cap(net, v, u), 1e-4));
    *flow(net, u, v) += delta;
    *flow(net, v, u) -= delta;
    assert(flte(*flow(net, u, v), *cap(net, u, v), 1e-4));
    assert(flte(*flow(net, v, u), *cap(net, v, u), 1e-4));
    assert(fcmp(*flow(net, u, v), -*flow(net, v, u), 1e-4));

    excess_flow[u] -= delta;
    excess_flow[v] += delta;
}

// Increase the node height
static void relabel(FlowNetwork *net, int32_t *height, double *excess_flow,
                    int32_t u) {
    assert(excess_flow[u] > 0.0);

#ifndef NDEBUG
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (u != v && residual_cap(net, u, v) > 0.0) {
            assert(height[u] <= height[v]);
        }
    }
#endif
    assert(u != net->source_vertex && u != net->sink_vertex);

    bool found = false;
    int32_t min_height = INT32_MAX;
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (residual_cap(net, u, v) > 0) {
            min_height = MIN(min_height, height[v]);
            found = true;
        }
    }

    assert(found);
    assert(min_height != INT32_MAX);
    int32_t new_height = 1 + min_height;
    assert(new_height >= height[u] + 1);
    height[u] = new_height;
    assert(height[u] < 2 * net->nnodes - 1);
}

static void discharge(FlowNetwork *net, int32_t *height, double *excess_flow,
                      int32_t u, int32_t *curr_neigh) {
    assert(u != net->source_vertex && u != net->sink_vertex);
    while (fgt(excess_flow[u], 0.0, 1e-5)) {
        int32_t v = curr_neigh[u];
        if (v >= net->nnodes) {
            relabel(net, height, excess_flow, u);
            curr_neigh[u] = 0;
        } else if (can_push(net, excess_flow, height, u, v)) {
            push(net, height, excess_flow, u, v);
        } else {
            curr_neigh[u] += 1;
        }
    }
}

static void greedy_preflow(FlowNetwork *net, double *excess_flow,
                           int32_t *height) {
    int32_t s = net->source_vertex;

    for (int32_t i = 0; i < net->nnodes; i++) {
        excess_flow[i] = 0.0;
        height[i] = 0;
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
        excess_flow[v] = c;
        excess_flow[s] -= c;
    }

    height[s] = net->nnodes;
}

static void compute_bipartition_from_height(FlowNetwork *net,
                                            MaxFlowResult *result,
                                            int32_t *height) {

    for (int32_t h = net->nnodes; h >= 0; h--) {
        bool found = false;
        for (int32_t i = 0; i < net->nnodes; i++) {
            if (height[i] == h) {
                found = true;
                break;
            }
        }
        if (!found) {
            for (int32_t i = 0; i < net->nnodes; i++) {
                result->bipartition.data[i] = height[i] > h;
            }
            break;
        }
    }
}

static double get_flow_from_s_node(FlowNetwork *net) {
    int32_t s = net->source_vertex;
    double max_flow = 0.0;
    for (int32_t i = 0; i < net->nnodes; i++) {
        if (i == s) {
            continue;
        }
        max_flow += *flow(net, s, i);
    }

    // Round to 0.0 if close
    if (fcmp(max_flow, 0.0, 1e-5)) {
        max_flow = 0.0;
    }

    assert(max_flow >= 0.0);
    return max_flow;
}

static void validate_flow(FlowNetwork *net, double max_flow,
                          double *excess_flow) {
    int32_t s = net->source_vertex;
    int32_t t = net->sink_vertex;

    for (int32_t i = 0; i < net->nnodes; i++) {
        double fenter = flow_entering(net, i);
        double fexit = flow_exiting(net, i);

        if (i == s) {
            assert(fcmp(fexit - fenter, max_flow, 1e-4));
        } else if (i == t) {
            assert(fcmp(fenter - fexit, max_flow, 1e-4));
        } else {
            // This assertion is only valid for all vertices except {s, t}.
            // This is verified in the CLRS (Introduction to algorithms) book
            assert(fcmp(excess_flow[i], 0.0, 1e-5));
            // Verify flow entering node i is equal to flow exiting node i
            assert(fcmp(fenter, fexit, 1e-4));
        }
    }

    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            // Assert flow on edge (i, j) does not exceed the capacity of edge
            // (i, j)
            assert(flte(*flow(net, i, j), *cap(net, i, j), 1e-4));
            assert(fcmp(*flow(net, i, j), -*flow(net, j, i), 1e-4));
        }
    }
}

static void validate_min_cut(FlowNetwork *net, MaxFlowResult *result,
                             double max_flow) {
    double section_flow = 0.0;
    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            int32_t li = (int32_t)result->bipartition.data[i];
            int32_t lj = (int32_t)result->bipartition.data[j];
            assert(fcmp(*flow(net, i, j), -*flow(net, j, i), 1e-4));
            double f = *flow(net, i, j);
            double c = *cap(net, i, j);
            assert(c >= 0.0);
            assert(flte(f, c, 1e-4));
            if (f >= 0) {
                if (li == 1 && lj == 0) {
                    // All edges should be saturated
                    double r = residual_cap(net, i, j);
                    assert(fcmp(0.0, r, 1e-4));
                    section_flow += f;
                } else if (li == 0 && lj == 1) {
                    // All edges should be drained
                    assert(fcmp(f, 0, 1e-4));
                    section_flow -= f;
                }
            }
        }
    }
    assert(fcmp(section_flow, max_flow, 1e-4));
}

// This implementation uses the relabel-to-front max flow algorithm version
// See:
// 1. https://en.wikipedia.org/wiki/Push%E2%80%93relabel_maximum_flow_algorithm
// 2. Goldberg, A.V., 1997. An efficient implementation of a scaling
//    minimum-cost flow algorithm. Journal of algorithms, 22(1), pp.1-29.
double push_relabel_max_flow(FlowNetwork *net, MaxFlowResult *result) {
    assert(net->cap);
    assert(net->flow);
    assert(net->nnodes >= 2);
    assert(net->sink_vertex != net->source_vertex);

    int32_t s = net->source_vertex;
    int32_t t = net->sink_vertex;

#ifndef NDEBUG
    for (int32_t i = 0; i < net->nnodes; i++) {
        assert(*cap(net, i, i) == 0.0);
    }
#endif

    int32_t *height = malloc(net->nnodes * sizeof(*height));
    double *excess_flow = malloc(net->nnodes * sizeof(*excess_flow));

    // PREFLOW
    greedy_preflow(net, excess_flow, height);

    int32_t *curr_neigh = malloc(net->nnodes * sizeof(*curr_neigh));
    int32_t *list = malloc((net->nnodes - 2) * sizeof(*list));
    int32_t list_len = 0;

    for (int32_t i = 0; i < net->nnodes; i++) {
        curr_neigh[i] = 0;
    }

    for (int32_t i = 0; i < net->nnodes; i++) {
        if (i != s && i != t) {
            list[list_len++] = i;
        }
    }

    // MAIN LOOP
    int32_t curr_node = 0;
    while (curr_node < list_len) {
        int32_t u = list[curr_node];
        int32_t old_height = height[u];
        discharge(net, height, excess_flow, u, curr_neigh);
        if (height[u] > old_height) {
            // Make space at the start of the list to move u at the front
            memmove(list + 1, list, curr_node * sizeof(*list));
            list[0] = u;
            assert(fcmp(excess_flow[u], 0.0, 1e-5));
            curr_node = 1;
        } else {
            curr_node += 1;
        }
    }

    // COMPUTE maxflow: Sum the flow of outgoing edges from s
    double max_flow = get_flow_from_s_node(net);

#ifndef NDEBUG
    validate_flow(net, max_flow, excess_flow);
#endif

    if (result) {
        assert(result->bipartition.data);

        result->maxflow = max_flow;
        result->bipartition.nnodes = net->nnodes;
        compute_bipartition_from_height(net, result, height);

#ifndef NDEBUG
        // Assert that the cross section induced from the bipartition is
        // consistent with the computed maxflow
        validate_min_cut(net, result, max_flow);
#endif
    }

    free(curr_neigh);
    free(list);
    free(excess_flow);
    free(height);

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

BruteforceMaxFlowResult max_flow_bruteforce(FlowNetwork *net) {
    assert(net->nnodes >= 2 && net->nnodes <= 10);
    int32_t *labels = calloc(net->nnodes, sizeof(*labels));

    int32_t num_sections = 0;
    double max_flow = INFINITY;

    for (int32_t label_it = 0; label_it < 1 << net->nnodes; label_it++) {
        for (int32_t k = 0; k < net->nnodes; k++) {
            labels[k] = (label_it & (1 << k)) >> k;
        }
        if (labels[net->source_vertex] != 1 || labels[net->sink_vertex] != 0) {
            continue;
        }

        labels[net->source_vertex] = 1;
        labels[net->sink_vertex] = 0;

        double flow = compute_flow_from_labels(net, labels);
        if (flte(flow, max_flow, 1e-6)) {
            if (fcmp(flow, max_flow, 1e-6)) {
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
        if (labels[net->source_vertex] != 1 || labels[net->sink_vertex] != 0) {
            continue;
        }

        labels[net->source_vertex] = 1;
        labels[net->sink_vertex] = 0;

        double flow = compute_flow_from_labels(net, labels);

        if (fcmp(flow, max_flow, 1e-6)) {
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
