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
    int32_t nnodes;
    double *weights;
} Network;

typedef struct {
    int32_t nnodes;
    double *flow;
    double *cap;
} FlowNetwork;

typedef enum {
    BLACK = 0,
    WHITE = 1,
} MaxFlowColor;

typedef struct {
    bool *data;
} NetworkBipartition;

typedef struct {
    int32_t nnodes;
    int32_t source;
    int32_t sink;
    double maxflow;
    int32_t *colors;
} MaxFlowResult;

typedef struct {
    int32_t nnodes;
    int32_t source;
    int32_t succ;
} ShortestPath;

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
    FlowNetwork reduced_net;
} GomoryHuTree;

typedef struct {
    bool *visited;
    int32_t *parent;
    int32_t *bfs_queue;
} FordFulkersonCtx;

typedef struct {
    int32_t nnodes;
    int32_t *p;
    double *flows;
    FordFulkersonCtx ff;
    PushRelabelCtx pr;
    MaxFlowResult mf;
} GomoryHuTreeCtx;

typedef struct {
    int32_t nnodes;
    int32_t *depth;
    int32_t *pred;
    double *dist;
} DijkstraCtx;

static inline bool push_relabel_ctx_is_valid(PushRelabelCtx *ctx) {
    bool result =
        ctx->height && ctx->excess_flow && ctx->curr_neigh && ctx->list;
    return result;
}

Network network_create(int32_t nnodes, bool clear_weights);
void network_destroy(Network *network);

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

GomoryHuTree gomory_hu_tree_create(int32_t nnodes);
void gomory_hu_tree_destroy(GomoryHuTree *tree);
double gomory_hu_query(GomoryHuTree *tree, int32_t source, int32_t sink,
                       MaxFlowResult *result, GomoryHuTreeCtx *ctx);

DijkstraCtx dijkstra_ctx_create(int32_t nnodes);
void dijkstra_ctx_destroy(DijkstraCtx *ctx);

void dijkstra(Network *net, int32_t source_vertex, ShortestPath *result,
              DijkstraCtx *ctx);

static inline double *network_weight(Network *net, int32_t i, int32_t j) {
    assert(i >= 0 && i < net->nnodes);
    assert(j >= 0 && j < net->nnodes);
    return &net->weights[i * net->nnodes + j];
}

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
