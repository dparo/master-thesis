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

#include "push-relabel.h"
#include "maxflow/utils.h"

void max_flow_create_push_relabel(MaxFlow *mf, int32_t nnodes) {
    mf->payload.height = malloc(nnodes * sizeof(*mf->payload.height));
    mf->payload.excess_flow = malloc(nnodes * sizeof(*mf->payload.excess_flow));
    mf->payload.curr_neigh = malloc(nnodes * sizeof(*mf->payload.curr_neigh));
    mf->payload.list = malloc((nnodes - 2) * sizeof(*mf->payload.list));

#ifndef NDEBUG
    // randomly initialize the array to make accumulation errors apparent
    for (int32_t i = 0; i < nnodes; i++) {
        mf->payload.height[i] = rand();
        mf->payload.excess_flow[i] = (flow_t)rand() % 40;
        mf->payload.curr_neigh[i] = rand();
    }

    for (int32_t i = 0; i < nnodes - 2; i++) {
        mf->payload.list[i] = rand();
    }
#endif
}

static inline bool can_push_flow(const FlowNetwork *net, const MaxFlow *mf,
                                 int32_t u, int32_t v) {
    if ((mf->payload.height[u] == (mf->payload.height[v] + 1)) &&
        (residual_cap(net, mf, u, v) > 0.0)) {
        return true;
    } else {
        return false;
    }
}

static void push(const FlowNetwork *net, MaxFlow *mf, int32_t u, int32_t v) {
    assert(mf->payload.excess_flow[u] > 0);
    assert(u != v);
    assert(mf->payload.height[u] == mf->payload.height[v] + 1);

    flow_t rescap = residual_cap(net, mf, u, v);
    assert(rescap > 0);
    flow_t delta = MIN(mf->payload.excess_flow[u], rescap);

    assert(MAXFLOW_FLOW(mf, u, v) == -MAXFLOW_FLOW(mf, v, u));
    assert(MAXFLOW_FLOW(mf, u, v) <= flow_net_get_cap(net, u, v));
    assert(MAXFLOW_FLOW(mf, v, u) <= flow_net_get_cap(net, v, u));

    MAXFLOW_FLOW(mf, u, v) += delta;
    MAXFLOW_FLOW(mf, v, u) -= delta;

    assert(MAXFLOW_FLOW(mf, u, v) == -MAXFLOW_FLOW(mf, v, u));
    assert(MAXFLOW_FLOW(mf, u, v) <= flow_net_get_cap(net, u, v));
    assert(MAXFLOW_FLOW(mf, v, u) <= flow_net_get_cap(net, v, u));
}

static void relabel(const FlowNetwork *net, MaxFlow *mf, int32_t u) {
    assert(mf->payload.excess_flow[u] > 0);

#ifndef NDEBUG
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (u != v && residual_cap(net, mf, u, v) > 0) {
            assert(mf->payload.height[u] <= mf->payload.height[v]);
        }
    }
#endif

    assert(u != mf->s && u != mf->t);

    int32_t min_height = INT32_MAX;
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (residual_cap(net, mf, u, v) > 0) {
            min_height = MIN(min_height, mf->payload.height[v]);
        }
    }

    assert(min_height != INT32_MAX);
    int32_t new_height = 1 + min_height;
    assert(new_height >= mf->payload.height[u] + 1);
    mf->payload.height[u] = new_height;
    assert(mf->payload.height[u] < 2 * net->nnodes - 1);
}

static void discharge(const FlowNetwork *net, MaxFlow *mf, int32_t u) {
    assert(u != mf->s && u != mf->t);

    while (mf->payload.excess_flow[0] > 0) {
        int32_t v = mf->payload.curr_neigh[u];
        if (v >= net->nnodes) {
            relabel(net, mf, u);
            mf->payload.curr_neigh[u] = 0;
        } else if (can_push_flow(net, mf, u, v)) {
            push(net, mf, u, v);
        } else {
            mf->payload.curr_neigh[u] += 1;
        }
    }
}

static void greedy_preflow(const FlowNetwork *net, MaxFlow *mf) {
    int32_t s = mf->s;

    for (int32_t i = 0; i < net->nnodes; i++) {
        mf->payload.excess_flow[i] = 0;
        mf->payload.height[i] = 0;
    }
    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = 0; j < net->nnodes; j++) {
            MAXFLOW_FLOW(mf, i, j) = 0;
        }
    }

    // For each edge leaving the source s, saturate all out-arcs of s
    for (int32_t v = 0; v < net->nnodes; v++) {
        if (v == s) {
            continue;
        }

        flow_t c = flow_net_get_cap(net, s, v);
        assert(c >= 0);

        MAXFLOW_FLOW(mf, s, v) = c;
        MAXFLOW_FLOW(mf, v, s) = -c;

        mf->payload.excess_flow[v] = c;
        mf->payload.excess_flow[s] -= c;
    }

    mf->payload.height[s] = net->nnodes;
}

static void compute_bipartition_from_height(const MaxFlow *mf,
                                            MaxFlowResult *result) {

    for (int32_t h = mf->nnodes; h >= 0; h--) {
        bool found = false;
        for (int32_t i = 0; i < mf->nnodes; i++) {
            if (mf->payload.height[i] == h) {
                found = true;
                break;
            }
        }
        if (!found) {
            for (int32_t i = 0; i < mf->nnodes; i++) {
                result->colors[i] = (mf->payload.height[i] > h) ? BLACK : WHITE;
            }
            break;
        }
    }
}

static flow_t get_flow_from_source_node(const MaxFlow *mf) {
    int32_t s = mf->s;
    flow_t max_flow = 0;

    for (int32_t i = 0; i < mf->nnodes; i++) {
        if (i == s) {
            continue;
        }
        max_flow += MAXFLOW_FLOW(mf, s, i);
    }

    assert(max_flow >= 0);
    return max_flow;
}

void max_flow_algo_push_relabel(const FlowNetwork *net, MaxFlow *mf, int32_t s,
                                int32_t t, MaxFlowResult *result) {
    maxflow_clear_flow(mf);
    greedy_preflow(net, mf);

    for (int32_t i = 0; i < net->nnodes; i++) {
        mf->payload.curr_neigh[i] = 0;
    }

    mf->payload.list_len = 0;
    for (int32_t i = 0; i < net->nnodes; i++) {
        if (i != s && i != t) {
            mf->payload.list[mf->payload.list_len++] = i;
        }
    }
    // MAIN LOOP
    {
        int32_t curr_node = 0;
        while (curr_node < mf->payload.list_len) {
            int32_t u = mf->payload.list[curr_node];
            int32_t prev_height = mf->payload.height[u];
            discharge(net, mf, u);

            if (mf->payload.height[u] > prev_height) {
                // Make space at the start of the list to move u at the front
                memmove(mf->payload.list + 1, mf->payload.list,
                        curr_node * sizeof(*mf->payload.list));
                mf->payload.list[0] = u;
                assert(mf->payload.excess_flow[u] == 0);
                curr_node = 1;
            } else {
                curr_node += 1;
            }
        }
    }

    // COMPUTE maxflow: Sum the flow of outgoing edges from s
    flow_t max_flow = get_flow_from_source_node(mf);

    validate_flow(net, mf, max_flow);
#ifndef NDEBUG
    for (int32_t i = 0; i < net->nnodes; i++) {
        if (i != s && i != t) {
            // This assertion is only valid for all vertices except {s, t}.
            // This is verified in the CLRS (Introduction to algorithms) book
            assert(mf->payload.excess_flow[i] == 0);
        }
    }
#endif

    result->maxflow = max_flow;
    compute_bipartition_from_height(mf, result);

    // Assert that the cross section induced from the bipartition is
    // consistent with the computed maxflow
    validate_min_cut(net, mf, result, max_flow);
}
