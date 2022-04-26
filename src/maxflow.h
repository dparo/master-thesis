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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include "types.h"

typedef int32_t flow_t;
#define FLOW_MAX INT32_MAX

typedef enum {
    BLACK = 0,
    WHITE = 1,
} MaxFlowBipartitionColor;

typedef struct {
    int32_t nnodes;
    flow_t *caps;
} FlowNetwork;

typedef enum MaxFlowAlgoKind {
    MAXFLOW_ALGO_INVALID,
    MAXFLOW_ALGO_PUSH_RELABEL,
    MAXFLOW_ALGO_BRUTEFORCE,
    MAXFLOW_ALGO_RANDOM,
    MAXFLOW_ALGO_2OPT,
    MAXFLOW_ALGO_LIN_KERNIGHAN,
} MaxFlowAlgoKind;

typedef struct MaxFlowResult {
    int32_t nnodes;
    int32_t s, t;
    flow_t maxflow;
    int32_t *colors;
} MaxFlowResult;

typedef struct MaxFlow {
    int32_t nnodes;

    int32_t s;
    int32_t t;

    MaxFlowAlgoKind kind;
    union {
        // bruteforce
        struct {
            MaxFlowResult temp_mf;
        };

        // Push relabel context
        struct {
            flow_t *flows;
            int32_t *height;
            flow_t *excess_flow;
            int32_t *curr_neigh;
            int32_t list_len;
            int32_t *list;
        };
    } payload;

} MaxFlow;

typedef struct GomoryHuTree {
    int32_t nnodes;
    int32_t num_results;
    MaxFlowResult *results;
    int32_t *indices;
    struct {
        int32_t *sink_candidate;
    };
} GomoryHuTree;

static inline void flow_net_set_cap(FlowNetwork *net, int32_t i, int32_t j,
                                    flow_t val) {
    assert(i >= 0 && i < net->nnodes);
    assert(j >= 0 && j < net->nnodes);
    net->caps[i * net->nnodes + j] = val;
}
static inline flow_t flow_net_get_cap(const FlowNetwork *net, int32_t i,
                                      int32_t j) {
    assert(i >= 0 && i < net->nnodes);
    assert(j >= 0 && j < net->nnodes);
    return net->caps[i * net->nnodes + j];
}

void flow_network_create_v2(FlowNetwork *network, int32_t nnodes);
void flow_network_destroy_v2(FlowNetwork *network);
void flow_network_clear_caps(FlowNetwork *net);

void max_flow_destroy(MaxFlow *mf);
void max_flow_create(MaxFlow *mf, int32_t nnodes, MaxFlowAlgoKind kind);

void max_flow_result_create_v2(MaxFlowResult *result, int32_t nnodes);
flow_t maxflow_result_recompute_flow(const FlowNetwork *net,
                                     MaxFlowResult *result);
void max_flow_result_destroy_v2(MaxFlowResult *result);

void max_flow_result_copy(MaxFlowResult *dest, const MaxFlowResult *src);

void gomory_hu_tree_create_v2(GomoryHuTree *tree, int32_t nnodes);
void gomory_hu_tree_destroy_v2(GomoryHuTree *tree);
MaxFlowResult *gomory_hu_tree_query_v2(GomoryHuTree *tree, int32_t s,
                                       int32_t t);

flow_t max_flow_single_pair(const FlowNetwork *net, MaxFlow *mf, int32_t s,
                            int32_t t, MaxFlowResult *result);

void max_flow_all_pairs(const FlowNetwork *net, MaxFlow *mf,
                        GomoryHuTree *tree);

#if __cplusplus
}
#endif
