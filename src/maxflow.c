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
#include "maxflow/push-relabel.h"

void max_flow_result_create_v2(MaxFlowResult *result, int32_t nnodes) {
    result->nnodes = nnodes;
    result->colors = malloc(nnodes * sizeof(*result->colors));
}

void max_flow_result_destroy_v2(MaxFlowResult *m) {
    free(m->colors);
    memset(m, 0, sizeof(*m));
}

void flow_network_destroy_v2(FlowNetwork *network) {
    free(network->caps);
    memset(network, 0, sizeof(*network));
}

void flow_network_create_v2(FlowNetwork *network, int32_t nnodes) {
    if (network->nnodes) {
        flow_network_destroy_v2(network);
    }
    network->nnodes = nnodes;
    int32_t nsquared = nnodes * nnodes;
    network->caps = calloc(nsquared, sizeof(*network->caps));

    if (!network->caps) {
        flow_network_destroy_v2(network);
    }
}

void flow_network_clear_caps(FlowNetwork *net) {
    int32_t nsquared = net->nnodes * net->nnodes;
    memset(net->caps, 0, nsquared * sizeof(*net->caps));
}

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
        max_flow_result_destroy_v2(&mf->payload.temp_mf);
        break;
    case MAXFLOW_ALGO_PUSH_RELABEL:
        max_flow_destroy_push_relabel(mf);
        break;
    default:
        assert(!"Invalid code path");
        break;
    }

    memset(mf, 0, sizeof(*mf));
}

void max_flow_create(MaxFlow *mf, int32_t nnodes, MaxFlowAlgoKind kind) {
    if (mf->kind != 0) {
        max_flow_destroy(mf);
    }

    switch (kind) {

    case MAXFLOW_ALGO_BRUTEFORCE:
        max_flow_result_create_v2(&mf->payload.temp_mf, nnodes);
        break;
    case MAXFLOW_ALGO_PUSH_RELABEL:
        max_flow_create_push_relabel(mf, nnodes);
        break;
    default:
        assert(!"Invalid code path");
        break;
    }

    mf->kind = kind;
    mf->nnodes = nnodes;
}

flow_t maxflow_result_recompute_flow(const FlowNetwork *net,
                                     MaxFlowResult *result) {
    flow_t flow = 0;

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
    result->maxflow = flow;
    return flow;
}

static void max_flow_single_pair_bruteforce(const FlowNetwork *net, MaxFlow *mf,
                                            int32_t s, int32_t t,
                                            MaxFlowResult *result) {
    // NOTE:
    //     This implementation of bruteforce cannot work with arbitrary
    //     sized networks. For convenience we use an int32_t to encode
    //     bipartitions, therefore the maximum allowed network size
    //     is 31 (30 to be on the safe side)
    //
    assert(net->nnodes <= 30);

    flow_t maxflow = FLOW_MAX;
    int32_t mincolor1_amt = INT32_MAX;

    for (int32_t label_it = 0; label_it < 1 << net->nnodes; label_it++) {
        for (int32_t k = 0; k < net->nnodes; k++) {
            mf->payload.temp_mf.colors[k] = (label_it & (1 << k)) >> k;
        }

        mf->payload.temp_mf.colors[s] = 1;
        mf->payload.temp_mf.colors[t] = 0;

        flow_t flow = maxflow_result_recompute_flow(net, &mf->payload.temp_mf);

        int32_t color1_amt = 0;
        for (int32_t i = 0; i < net->nnodes; i++)
            if (mf->payload.temp_mf.colors[i] == mf->payload.temp_mf.colors[s])
                color1_amt += 1;

        bool improving = false;
        if (flow < maxflow) {
            improving = true;
        } else if (flow == maxflow && color1_amt < mincolor1_amt) {
            // In case of tie, prefer the min-cuts achieving
            // the least number of nodes in the source-vertex
            // side of the coloring
            improving = true;
        }

        mincolor1_amt = MIN(mincolor1_amt, color1_amt);
        maxflow = MIN(maxflow, flow);

        if (improving) {
            max_flow_result_copy(result, &mf->payload.temp_mf);
        }
    }
}

flow_t max_flow_single_pair(const FlowNetwork *net, MaxFlow *mf, int32_t s,
                            int32_t t, MaxFlowResult *result) {
    assert(net->caps);
    assert(net->nnodes >= 2);
    assert(mf->nnodes >= 2);
    assert(result->nnodes >= 2);
    assert(net->nnodes == mf->nnodes);
    assert(result->nnodes == net->nnodes);

    assert(s != t);
    assert(s >= 0 && s < net->nnodes);
    assert(t >= 0 && t < net->nnodes);

    assert(result->colors);

#ifndef NDEBUG
    for (int32_t i = 0; i < net->nnodes; i++) {
        assert(flow_net_get_cap(net, i, i) == 0);
    }
#endif

    result->s = s;
    result->t = t;

    mf->s = s;
    mf->t = t;

    switch (mf->kind) {
    case MAXFLOW_ALGO_BRUTEFORCE:
        max_flow_single_pair_bruteforce(net, mf, s, t, result);
        break;

    case MAXFLOW_ALGO_RANDOM:
        for (int32_t i = 0; i < net->nnodes; i++) {
            result->colors[i] = rand() % 2;
        }
        result->colors[s] = 1;
        result->colors[t] = 0;
        maxflow_result_recompute_flow(net, result);
        break;

    case MAXFLOW_ALGO_PUSH_RELABEL:
        max_flow_algo_push_relabel(net, mf, s, t, result);
        break;

    default:
        assert(!"Invalid code path");
        break;
    }

    result->s = s;
    result->t = t;

    return result->maxflow;
}

void gomory_hu_tree_create_v2(GomoryHuTree *tree, int32_t nnodes) {
    tree->nnodes = nnodes;
    tree->sink_candidate = malloc(nnodes * sizeof(*tree->sink_candidate));
    tree->record_flows = malloc(nnodes * sizeof(*tree->record_flows));
    tree->maxflows = malloc(nnodes * nnodes * sizeof(*tree->maxflows));
    tree->rows = malloc(nnodes * sizeof(tree->rows[0]) * nnodes *
                        sizeof(tree->rows[0].records[0]));

    tree->bfs_queue = malloc(nnodes * sizeof(*tree->bfs_queue));
    tree->parent = malloc(nnodes * sizeof(*tree->parent));
    tree->visited = malloc(nnodes * sizeof(*tree->visited));

    max_flow_result_create_v2(&tree->temp_result, nnodes);
}

void gomory_hu_tree_destroy_v2(GomoryHuTree *tree) {
    max_flow_result_destroy_v2(&tree->temp_result);

    free(tree->bfs_queue);
    free(tree->parent);
    free(tree->visited);
    free(tree->rows);
    free(tree->sink_candidate);
    free(tree->record_flows);
    free(tree->maxflows);

    memset(tree, 0, sizeof(*tree));
}

static GomoryHuTreeAdjRow *gomory_hu_tree_get_row(GomoryHuTree *tree,
                                                  int32_t node) {
    ATTRIB_MAYBE_UNUSED const int32_t n = tree->nnodes;
    return (GomoryHuTreeAdjRow *)((uint8_t *)tree->rows +
                                  node * sizeof(tree->rows[0]) * n *
                                      sizeof(tree->rows[0].records[0]));
}

void max_flow_all_pairs(const FlowNetwork *net, MaxFlow *mf,
                        GomoryHuTree *tree) {
    ATTRIB_MAYBE_UNUSED const int32_t n = net->nnodes;
    assert(tree->nnodes == net->nnodes);
    assert(tree->nnodes == mf->nnodes);
    assert(net->nnodes == mf->nnodes);

#ifndef NDEBUG

    // IMPORTANT:
    //     This implementation only works with undirected graphs.
    //     Since the FlowNetwork allows representations of directed graphs
    //     We are going to assert that the network is undirected here
    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (i != j) {
                assert(flow_net_get_cap(net, i, j) ==
                       flow_net_get_cap(net, j, i));
            }
        }
    }

#endif

    for (int32_t i = 0; i < n; i++) {
        tree->sink_candidate[i] = 0;
        tree->record_flows[i] = 0;
        GomoryHuTreeAdjRow *row = gomory_hu_tree_get_row(tree, i);
        row->num_records = 0;
    }

    for (int32_t s = 1; s < n; s++) {
        MaxFlowResult *result = &tree->temp_result;

        int32_t t = tree->sink_candidate[s];
        ATTRIB_MAYBE_UNUSED flow_t max_flow =
            max_flow_single_pair(net, mf, s, t, result);

        assert(max_flow == result->maxflow);

        assert(result->colors[s] == BLACK);
        assert(result->colors[t] == WHITE);

        tree->record_flows[s] = max_flow;

        // Setup the next sink candidate for each vertex according to their
        // bipartition (s, t) as valid max_flow candidates.
        for (int32_t i = 0; i < n; i++) {
            bool i_black = result->colors[i] == BLACK;
            bool i_white = result->colors[i] == WHITE;

            if (i != s && tree->sink_candidate[i] == t && i_black) {
                tree->sink_candidate[i] = s;
            } else if (i != t && tree->sink_candidate[i] == s && i_white) {
                tree->sink_candidate[i] = t;
            }
        }

        // If the next sink candidate for t is of BLACK COLOR (eg belongs to the
        // s bipartition), fix the candidates, and swap the flows
        if (result->colors[tree->sink_candidate[t]] == BLACK) {
            tree->sink_candidate[s] = tree->sink_candidate[t];
            tree->sink_candidate[t] = s;
            SWAP(flow_t, tree->record_flows[s], tree->record_flows[t]);
        }
    }

    // Setup the adjency matrix records
    for (int32_t s = 1; s < n; s++) {
        flow_t f = tree->record_flows[s];
        int32_t t = tree->sink_candidate[s];

        GomoryHuTreeAdjRow *s_row = gomory_hu_tree_get_row(tree, s);
        GomoryHuTreeAdjRow *t_row = gomory_hu_tree_get_row(tree, t);

        s_row->records[s_row->num_records].node = t;
        s_row->records[s_row->num_records].flow = f;
        ++s_row->num_records;

        t_row->records[t_row->num_records].node = s;
        t_row->records[t_row->num_records].flow = f;
        ++t_row->num_records;
    }

    // Build up all the maxflows.
    //       The same BFS traversal can be shared to compute N maxflows
    //       at the same time
    {
        int32_t *queue = tree->bfs_queue;

        for (int32_t s = 0; s < n; s++) {
            memset(tree->visited, 0, n * sizeof(*tree->visited));
            memset(tree->record_flows, 0, n * sizeof(*tree->record_flows));
            int32_t head = 0;
            int32_t tail = 0;
            queue[tail++] = s;
            tree->visited[s] = true;
            tree->parent[s] = -1;
            tree->maxflows[s * n + s] = FLOW_MAX;

            while (head != tail) {
                int32_t u = queue[head++];

                GomoryHuTreeAdjRow *row = gomory_hu_tree_get_row(tree, u);
                for (int32_t i = 0; i < row->num_records; i++) {
                    if (i >= row->num_records) {
                        break;
                    }

                    int32_t v = row->records[i].node;
                    flow_t flow = row->records[i].flow;
                    bool explore_v = !tree->visited[v];

                    if (explore_v) {
                        queue[tail++] = v;
                        tree->parent[v] = u;
                        tree->maxflows[s * n + v] =
                            MIN(tree->maxflows[s * n + u], flow);
                        tree->visited[v] = true;
                    }
                }
            }
        }
    }
}

flow_t gomory_hu_tree_query_v2(GomoryHuTree *tree, MaxFlowResult *result,
                               int32_t s, int32_t t) {
    const int32_t n = tree->nnodes;

    assert(s != t);
    assert(s >= 0 && s < tree->nnodes);
    assert(t >= 0 && t < tree->nnodes);

    flow_t max_flow = tree->maxflows[s * n + t];
    result->maxflow = max_flow;

    for (int32_t i = 0; i < n; i++) {
        result->colors[i] = WHITE;
    }

    // BFS to determine the bipartition
    {
        int32_t *queue = tree->bfs_queue;

        int32_t head = 0;
        int32_t tail = 0;
        queue[tail++] = s;
        result->colors[s] = BLACK;

        while (head != tail) {
            int32_t u = queue[head++];

            GomoryHuTreeAdjRow *row = gomory_hu_tree_get_row(tree, u);
            for (int32_t i = 0; i < row->num_records; i++) {
                int32_t v = row->records[i].node;
                flow_t flow = row->records[i].flow;
                bool explore_v = result->colors[v] == WHITE && flow > max_flow;

                if (explore_v) {
                    queue[tail++] = v;
                    result->colors[v] = BLACK;
                }
            }
        }
    }
    assert(result->colors[s] == BLACK);
    assert(result->colors[t] == WHITE);
    result->s = s;
    result->t = t;
    return max_flow;
}
