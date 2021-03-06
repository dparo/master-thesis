/*
 * Copyright (c) 2022 Davide Paro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#if __cplusplus
extern "C" {
#endif

#include "maxflow.h"

#define MAXFLOW_FLOW(mf, i, j) (mf->payload.flows[i * mf->nnodes + j])

static inline void maxflow_clear_flow(MaxFlow *mf) {
    memset(mf->payload.flows, 0,
           mf->nnodes * mf->nnodes * sizeof(*mf->payload.flows));
}

static inline flow_t residual_cap(const FlowNetwork *net, const MaxFlow *mf,
                                  int32_t i, int32_t j) {
    assert(MAXFLOW_FLOW(mf, i, j) == -MAXFLOW_FLOW(mf, j, i));
    flow_t result = flow_net_get_cap(net, i, j) - MAXFLOW_FLOW(mf, i, j);
    return result;
}

static inline flow_t flow_entering(const MaxFlow *mf, int32_t i) {
    flow_t sum = 0;
    for (int32_t j = 0; j < mf->nnodes; j++) {
        if (i != j) {
            flow_t f = MAXFLOW_FLOW(mf, j, i);
            if (f >= 0) {
                sum += f;
            }
        }
    }
    return sum;
}

static inline flow_t flow_exiting(const MaxFlow *mf, int32_t i) {
    flow_t sum = 0;
    for (int32_t j = 0; j < mf->nnodes; j++) {
        if (i != j) {
            flow_t f = MAXFLOW_FLOW(mf, i, j);
            if (f >= 0) {
                sum += f;
            }
        }
    }
    return sum;
}

static void validate_flow(const FlowNetwork *net, const MaxFlow *mf,
                          double max_flow) {
#ifndef NDEBUG
    int32_t s = mf->s;
    int32_t t = mf->t;

    for (int32_t i = 0; i < net->nnodes; i++) {
        flow_t fenter = flow_entering(mf, i);
        flow_t fexit = flow_exiting(mf, i);

        if (i == s) {
            assert(fexit - fenter == max_flow);
        } else if (i == t) {
            assert(fenter - fexit == max_flow);
        } else {
            // Verify flow entering node i is equal to flow exiting node i
            assert(fenter == fexit);
        }
    }

    // Assert flow on edge (i, j) does not exceed the capacity of edge (i, j)
    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            assert(MAXFLOW_FLOW(mf, i, j) <= flow_net_get_cap(net, i, j));
        }
    }

    // Assert forward flow is equal to backward flow
    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            assert(MAXFLOW_FLOW(mf, i, j) == -MAXFLOW_FLOW(mf, j, i));
        }
    }

#else
    UNUSED_PARAM(net);
    UNUSED_PARAM(mf);
    UNUSED_PARAM(max_flow);
#endif
}

static inline void validate_min_cut(const FlowNetwork *net, const MaxFlow *mf,
                                    const MaxFlowResult *result,
                                    double max_flow) {
#ifndef NDEBUG
    flow_t section_flow = 0;

    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {

            int32_t li = result->colors[i];
            int32_t lj = result->colors[j];
            assert(MAXFLOW_FLOW(mf, i, j) == -MAXFLOW_FLOW(mf, j, i));

            flow_t f = MAXFLOW_FLOW(mf, i, j);
            flow_t c = flow_net_get_cap(net, i, j);

            assert(c >= 0);
            assert(f <= c);

            if (f >= 0) {
                if (li == BLACK && lj == WHITE) {
                    // All edges should be saturated
                    flow_t r = residual_cap(net, mf, i, j);
                    assert(r == 0);
                    section_flow += f;
                } else if (li == WHITE && lj == BLACK) {
                    // All edges should be drained
                    assert(f == 0);
                    section_flow -= f;
                }
            }
        }
    }
    assert(max_flow == section_flow);
#else
    UNUSED_PARAM(net);
    UNUSED_PARAM(result);
    UNUSED_PARAM(mf);
    UNUSED_PARAM(result);
    UNUSED_PARAM(max_flow);
#endif
}

#if __cplusplus
}
#endif
