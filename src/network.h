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
    double *flow;
    double *cap;
} FlowNetwork;

typedef struct {
    int32_t nnodes;
    bool *data;
} NetworkBipartition;

typedef struct {
    double maxflow;
    NetworkBipartition bipartition;
} MaxFlowResult;

/// For debug only
typedef struct {
    double maxflow;
    int32_t num_sections;
    MaxFlowResult *sections;
} BruteforceMaxFlowResult;

typedef struct {
    int32_t source_vertex;
    int32_t sink_vertex;
    int32_t *height;
    double *excess_flow;
    int32_t *curr_neigh;
    int32_t list_len;
    int32_t *list;
} PushRelabelCtx;

typedef struct {
    int32_t u, v;
} GomoryHuEdge;

typedef struct {
    int32_t nedges;
    GomoryHuEdge *edges;
} GomoryHuTree;

typedef struct GomoryHuTreeFordFulkersonCtx {
    int32_t *p;
    double *flows;
    uint8_t *colors;
    int32_t *pred;
    int32_t *bfs_queue;
    FlowNetwork reduced_net;
} GomoryHuTreeFordFulkersonCtx;

typedef struct {
    GomoryHuTreeFordFulkersonCtx ff;
    FlowNetwork contracted_net;
    MaxFlowResult mfr;
    PushRelabelCtx pr_ctx;
    int32_t num_sets;
    int32_t *sets;
    int32_t *sets_size;
    int32_t num_edges;
    GomoryHuEdge *edges;
} GomoryHuTreeCtx;

static inline bool push_relabel_ctx_is_valid(PushRelabelCtx *ctx) {
    bool result =
        ctx->height && ctx->excess_flow && ctx->curr_neigh && ctx->list;
    return result;
}

FlowNetwork flow_network_create(int32_t nnodes);
void flow_network_clear(FlowNetwork *net, bool clear_cap);
void flow_network_destroy(FlowNetwork *net);

MaxFlowResult max_flow_result_create(int32_t nnodes);
void max_flow_result_destroy(MaxFlowResult *m);

PushRelabelCtx push_relabel_ctx_create(int32_t nnodes);
void push_relabel_ctx_destroy(PushRelabelCtx *ctx);

double push_relabel_max_flow(FlowNetwork *net, int32_t source_vertex,
                             int32_t sink_vertex, MaxFlowResult *result);
double push_relabel_max_flow2(FlowNetwork *net, int32_t source_vertex,
                              int32_t sink_vertex, MaxFlowResult *result,
                              PushRelabelCtx *ctx);

BruteforceMaxFlowResult max_flow_bruteforce(FlowNetwork *net,
                                            int32_t source_vertex,
                                            int32_t sink_vertex);

bool gomory_hu_tree_ctx_create(GomoryHuTreeCtx *ctx, int32_t nnodes);
void gomory_hu_tree_ctx_destroy(GomoryHuTreeCtx *ctx);
void gomory_hu_tree2(FlowNetwork *net, GomoryHuTree *output,
                     GomoryHuTreeCtx *ctx);

bool gomory_hu_tree(FlowNetwork *net, GomoryHuTree *output);

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
