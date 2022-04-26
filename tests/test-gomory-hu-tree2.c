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

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <greatest.h>
#include "types.h"
#include "maxflow.h"

#define MAX_NUM_NODES_TO_TEST 50

typedef struct NodePair {
    int32_t u, v;
} NodePair;

TEST validate_with_slow_max_flow(FlowNetwork *net, int32_t s, int32_t t,
                                 MaxFlowResult *result) {
    MaxFlowResult bf_result;
    max_flow_result_create_v2(&bf_result, net->nnodes);

    MaxFlow bf_maxflow = {0};
    max_flow_create(&bf_maxflow, net->nnodes, MAXFLOW_ALGO_BRUTEFORCE);

    max_flow_single_pair(net, &bf_maxflow, s, t, &bf_result);
    ASSERT_EQ(bf_result.maxflow, result->maxflow);

    max_flow_result_destroy_v2(&bf_result);
    max_flow_destroy(&bf_maxflow);

    PASS();
}

static NodePair make_random_node_pair(int32_t n) {
    NodePair result = {0};
    result.u = rand() % n;

    do {
        result.v = rand() % n;
    } while (result.v == result.u);

    return result;
}

static void init_symm_random_flownet(FlowNetwork *net) {
    const flow_t RAND_VALS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = i + 1; j < net->nnodes; j++) {
            if (i != j) {
                flow_t r = RAND_VALS[rand() % ARRAY_LEN(RAND_VALS)];
                flow_net_set_cap(net, i, j, r);
                flow_net_set_cap(net, j, i, r);
            }
        }
    }
}

TEST random_symm_networks(void) {
    for (int32_t nnodes = 2; nnodes <= 10; nnodes++) {
        for (int32_t try_it = 0; try_it < 2048; try_it++) {
            FlowNetwork net = {0};
            MaxFlow mf = {0};
            MaxFlowResult result1 = {0};
            MaxFlowResult result2 = {0};

            flow_network_create_v2(&net, nnodes);
            max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
            max_flow_result_create_v2(&result1, nnodes);
            max_flow_result_create_v2(&result2, nnodes);

            NodePair st_pair = make_random_node_pair(nnodes);
            init_symm_random_flownet(&net);

            int32_t source_vertex = st_pair.u;
            int32_t sink_vertex = st_pair.v;

            flow_t max_flow1 = 0;
            flow_t max_flow2 = 0;

            // Validate flow symmetry between max flows (s, t) vs (t, s)
            {
                max_flow1 = max_flow_single_pair(&net, &mf, source_vertex,
                                                 sink_vertex, &result1);

                ASSERT_EQ(max_flow1, result1.maxflow);
                CHECK_CALL(validate_with_slow_max_flow(&net, source_vertex,
                                                       sink_vertex, &result1));
            }

            {
                max_flow2 = max_flow_single_pair(&net, &mf, sink_vertex,
                                                 source_vertex, &result2);
                ASSERT_EQ(max_flow2, result2.maxflow);
                CHECK_CALL(validate_with_slow_max_flow(
                    &net, sink_vertex, source_vertex, &result2));
            }

            ASSERT_EQ(max_flow1, max_flow2);

            flow_network_destroy_v2(&net);
            max_flow_result_destroy_v2(&result1);
            max_flow_result_destroy_v2(&result2);
        }
    }

    PASS();
}

TEST random_gomory_hu(void) {
    for (int32_t nnodes = 2; nnodes <= 10; nnodes++) {
        for (int32_t try_it = 0; try_it < 1024; try_it++) {
            MaxFlow mf = {0};
            FlowNetwork net = {0};
            MaxFlowResult result1 = {0};
            GomoryHuTree tree = {0};

            max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
            max_flow_result_create_v2(&result1, nnodes);
            flow_network_create_v2(&net, nnodes);
            init_symm_random_flownet(&net);
            gomory_hu_tree_create_v2(&tree, nnodes);

            flow_t max_flow1 = FLOW_MAX;

            max_flow_all_pairs(&net, &mf, &tree);

            for (int32_t source = 0; source < nnodes; source++) {
                for (int32_t sink = 0; sink < nnodes; sink++) {
                    if (source == sink) {
                        continue;
                    }
                    max_flow1 =
                        max_flow_single_pair(&net, &mf, source, sink, &result1);

                    MaxFlowResult *result2 =
                        gomory_hu_tree_query_v2(&tree, source, sink);
                    flow_t max_flow2 = result2->maxflow;

                    assert(result1.colors[source] == BLACK);
                    assert(result1.colors[sink] == WHITE);
                    assert(result2->colors[source] == BLACK);
                    assert(result2->colors[sink] == WHITE);

                    ASSERT_EQ(max_flow1, result1.maxflow);
                    ASSERT_EQ(max_flow2, result2->maxflow);

                    ASSERT_EQ(max_flow1, max_flow2);
                }
            }

            flow_network_destroy_v2(&net);
            max_flow_destroy(&mf);
            max_flow_result_destroy_v2(&result1);
            gomory_hu_tree_destroy_v2(&tree);
        }
    }
    PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */
    RUN_TEST(random_symm_networks);
    RUN_TEST(random_gomory_hu);
    GREATEST_MAIN_END(); /* display results */
}
