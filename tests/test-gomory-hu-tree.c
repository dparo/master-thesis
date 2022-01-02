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
#include "network.h"

#define MAX_NUM_NODES_TO_TEST 50

typedef struct NodePair {
    int32_t u, v;
} NodePair;

TEST validate_with_slow_max_flow(FlowNetwork *net, int32_t source_vertex,
                                 int32_t sink_vertex, MaxFlowResult *result) {
    BruteforceMaxFlowResult bf =
        max_flow_bruteforce(net, source_vertex, sink_vertex);

    // printf("found_max_flow = %g, true_max_flow = %g\n", result->maxflow,
    //        bf.maxflow);
    // printf("LABELS. Found %d max flow sections:\n", bf.num_sections);

    for (int32_t i = 0; i < net->nnodes; i++) {
        // printf("computed_bipartition[%d] = %d\n", i,
        //        result->bipartition.data[i]);
    }

    for (int32_t secidx = 0; secidx < bf.num_sections; secidx++) {
        for (int32_t i = 0; i < net->nnodes; i++) {
            // printf("section[%d][%d] = %d\n", secidx, i,
            //        bf.sections[secidx].bipartition.data[i]);
        }
    }
    // printf("\n");

    bool is_valid_section = false;

    for (int32_t secidx = 0; secidx < bf.num_sections; secidx++) {
        bool found = true;
        for (int32_t i = 0; i < net->nnodes; i++) {
            if (result->colors[i] != bf.sections[secidx].colors[i]) {
                found = false;
                break;
            }
        }

        if (found) {
            is_valid_section = true;
            break;
        }
    }

    ASSERT_IN_RANGE(bf.maxflow, result->maxflow, 1e-4);
    ASSERT(is_valid_section);

    // Cleanup
    {
        for (int32_t secidx = 0; secidx < bf.num_sections; secidx++) {
            max_flow_result_destroy(&bf.sections[secidx]);
        }
        free(bf.sections);
    }

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

    const double RAND_VALS[] = {0.0, 1e-3, 1e-2, 1e-1, 0.5, 0.8, 1.0};
    for (int32_t i = 0; i < net->nnodes; i++) {
        for (int32_t j = i + 1; j < net->nnodes; j++) {
            if (i != j) {
                double r = RAND_VALS[rand() % ARRAY_LEN(RAND_VALS)];
                *network_cap(net, i, j) = r;
                *network_cap(net, j, i) = r;
            }
        }
    }
}

TEST random_symm_networks(void) {
    for (int32_t nnodes = 2; nnodes <= 10; nnodes++) {
        for (int32_t try_it = 0; try_it < 2048; try_it++) {
            FlowNetwork network = flow_network_create(nnodes);
            MaxFlowResult max_flow_result1 = max_flow_result_create(nnodes);
            MaxFlowResult max_flow_result2 = max_flow_result_create(nnodes);

            NodePair st_pair = make_random_node_pair(nnodes);
            init_symm_random_flownet(&network);
            int32_t source_vertex = st_pair.u;
            int32_t sink_vertex = st_pair.v;

            double max_flow1;
            double max_flow2;
            {
                max_flow1 = push_relabel_max_flow(
                    &network, source_vertex, sink_vertex, &max_flow_result1);
                ASSERT_IN_RANGE(max_flow1, max_flow_result1.maxflow, 1e-5);
                CHECK_CALL(validate_with_slow_max_flow(
                    &network, source_vertex, sink_vertex, &max_flow_result1));
            }

            {
                max_flow2 = push_relabel_max_flow(
                    &network, sink_vertex, source_vertex, &max_flow_result2);
                ASSERT_IN_RANGE(max_flow2, max_flow_result2.maxflow, 1e-5);
                CHECK_CALL(validate_with_slow_max_flow(
                    &network, sink_vertex, source_vertex, &max_flow_result2));
            }

            ASSERT_IN_RANGE(max_flow1, max_flow2, 1e-5);

            // NOTE:
            //       A simple byte by byte comparison of is not possible to
            //       assert whether, 2 min_cuts are complementary. This is
            //       because a solution to a max_flow problem can have multiple
            //       min cut solutions.
#if 0
            for (int32_t i = 0; i < nnodes; i++) {
                ASSERT_EQ(max_flow_result1.bipartition.data[i],
                          !max_flow_result2.bipartition.data[i]);
            }
#endif

            flow_network_destroy(&network);
            max_flow_result_destroy(&max_flow_result1);
            max_flow_result_destroy(&max_flow_result2);
        }
    }

    PASS();
}

TEST random_gomory_hu(void) {
    for (int32_t nnodes = 2; nnodes <= 10; nnodes++) {
        for (int32_t try_it = 0; try_it < 1024; try_it++) {
            FlowNetwork network = flow_network_create(nnodes);
            init_symm_random_flownet(&network);

            MaxFlowResult max_flow_result1 = max_flow_result_create(nnodes);
            MaxFlowResult max_flow_result2 = max_flow_result_create(nnodes);

            GomoryHuTree tree = gomory_hu_tree_create(nnodes);
            GomoryHuTreeCtx gh_ctx = {0};

            double max_flow1 = INFINITY, max_flow2 = INFINITY;

            gomory_hu_tree_ctx_create(&gh_ctx, nnodes);
            gomory_hu_tree2(&network, &tree, &gh_ctx);

            for (int32_t source = 0; source < nnodes; source++) {
                for (int32_t sink = 0; sink < nnodes; sink++) {
                    if (source == sink) {
                        continue;
                    }
                    max_flow1 = push_relabel_max_flow(&network, source, sink,
                                                      &max_flow_result1);
                    max_flow2 = gomory_hu_query(&tree, source, sink,
                                                &max_flow_result2, &gh_ctx);

                    ASSERT_IN_RANGE(max_flow1, max_flow_result1.maxflow, 1e-5);
                    ASSERT_IN_RANGE(max_flow2, max_flow_result2.maxflow, 1e-5);
                    ASSERT_IN_RANGE(max_flow1, max_flow2, 1e-5);
                }
            }

            flow_network_destroy(&network);
            max_flow_result_destroy(&max_flow_result1);
            max_flow_result_destroy(&max_flow_result2);
            gomory_hu_tree_ctx_destroy(&gh_ctx);
            gomory_hu_tree_destroy(&tree);
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
