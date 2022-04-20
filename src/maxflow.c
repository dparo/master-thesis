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

#include "maxflow.h"

void max_flow_result_copy(MaxFlowResult *dest, const MaxFlowResult *src) {
    assert(dest->nnodes == src->nnodes);
    dest->s = src->s;
    dest->t = src->t;
    dest->maxflow = src->maxflow;
    memcpy(dest->colors, src->colors, dest->nnodes * sizeof(*dest->colors));
}

void max_flow_destroy(MaxFlow *mf) {

    switch (mf->kind) {
    case MAXFLOW_ALGO_BRUTEFORCE:
        max_flow_result_destroy(&mf->payload.temp_mf);
        break;
    default:
        assert(!"Invalid code path");
        break;
    }

    memset(mf, 0, sizeof(*mf));
}

void max_flow_init(MaxFlow *mf, int32_t nnodes, MaxFlowAlgoKind kind) {
    if (mf->kind != 0) {
        max_flow_destroy(mf);
    }

    switch (kind) {

    case MAXFLOW_ALGO_BRUTEFORCE:
        max_flow_result_create(&mf->payload.temp_mf, nnodes);
        break;
    default:
        assert(!"Invalid code path");
        break;
    }

    mf->kind = kind;
    mf->nnodes = nnodes;
}

static flow_t maxflow_result_recompute_flow(const FlowNetwork *net,
                                            MaxFlowResult *result) {
    flow_t flow = 0.0;

    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            if (i == j) {
                continue;
            }
            if (result->colors[i] == 1 && result->colors[j] == 0) {
                flow += flow_net_get_cap(net, i, j);
            }
        }
    }
    return flow;
}

static void max_flow_single_pair_bruteforce(const FlowNetwork *net, MaxFlow *mf,
                                            int32_t s, int32_t t,
                                            MaxFlowResult *result) {
    flow_t maxflow = INFINITY;

    for (int32_t label_it = 0; label_it < 1 << net->nnodes; label_it++) {
        for (int32_t k = 0; k < net->nnodes; k++) {
            mf->payload.temp_mf.colors[k] = (label_it & (1 << k)) >> k;
        }

        mf->payload.temp_mf.colors[s] = 1;
        mf->payload.temp_mf.colors[t] = 0;

        flow_t flow = maxflow_result_recompute_flow(net, &mf->payload.temp_mf);
        if (flow < maxflow) {
            maxflow = flow;
        }
    }

    max_flow_result_copy(result, &mf->payload.temp_mf);
}

void max_flow_single_pair(const FlowNetwork *net, MaxFlow *mf, int32_t s,
                          int32_t t, MaxFlowResult *result) {
    assert(net->nnodes >= 2);
    assert(mf->nnodes >= 2);
    assert(result->nnodes >= 2);

    result->s = s;
    result->t = t;

    switch (mf->kind) {
    case MAXFLOW_ALGO_BRUTEFORCE:
        max_flow_single_pair_bruteforce(net, mf, s, t, result);
        break;
    default:
        assert(!"Invalid code path");
        break;
    }

    result->s = s;
    result->t = t;
}

void max_flow_all_pairs(const FlowNetwork *net, MaxFlow *mf,
                        GomoryHuTree *tree) {
    int32_t s, t;
    MaxFlowResult result = {0};

    max_flow_single_pair(net, mf, s, t, &result);
}
