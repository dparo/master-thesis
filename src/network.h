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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "core-utils.h"

typedef struct {
    int32_t w, h;
    int16_t *data;
} AdjMatrix;

typedef struct {
    int32_t nnodes;
    int32_t source_vertex;
    int32_t sink_vertex;
    double *flow;
    double *cap;
} FlowNetwork;

typedef struct {
    int32_t nnodes;
    int32_t *labels;
} NetworkBipartition;

typedef struct {
    double maxflow;
    NetworkBipartition bipartition;
} MaxFlowResult;

FlowNetwork flow_network_create(int32_t nnodes);
void flow_network_clear(FlowNetwork *net);
void flow_network_destroy(FlowNetwork *net);

double edmond_karp_max_flow(FlowNetwork *net);
double push_relabel_max_flow(FlowNetwork *net);

static inline double *network_flow(FlowNetwork *net, int32_t i, int32_t j) {
    assert(i >= 0 && i < net->nnodes);
    assert(j >= 0 && j < net->nnodes);
    return &net->flow[i * net->nnodes + j];
}

static inline double *network_cap(FlowNetwork *net, int32_t i, int32_t j) {
    assert(i >= 0 && i < net->nnodes);
    assert(j >= 0 && j < net->nnodes);
    return &net->cap[i * net->nnodes + j];
}

#if __cplusplus
}
#endif
