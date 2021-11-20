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

TEST CLRS_network(void) {
    int32_t nnodes = 6;
    FlowNetwork net = flow_network_create(nnodes);
    MaxFlowResult max_flow_result = max_flow_result_create(nnodes);
    net.source_vertex = 0;
    net.sink_vertex = nnodes - 1;

    *network_cap(&net, 0, 1) = 16;
    *network_cap(&net, 0, 2) = 13;
    *network_cap(&net, 1, 2) = 10;
    *network_cap(&net, 2, 1) = 4;
    *network_cap(&net, 1, 3) = 12;
    *network_cap(&net, 3, 2) = 9;
    *network_cap(&net, 2, 4) = 14;
    *network_cap(&net, 4, 3) = 7;
    *network_cap(&net, 3, 5) = 20;
    *network_cap(&net, 4, 5) = 4;

    double max_flow = push_relabel_max_flow(&net, &max_flow_result);

    ASSERT_IN_RANGE(23, max_flow, 1e-4);
    ASSERT_EQ(1, max_flow_result.bipartition.data[0]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[2]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[3]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[4]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[5]);

    flow_network_destroy(&net);
    max_flow_result_destroy(&max_flow_result);
    PASS();
}

// From Matteo Fischetti "Lezioni di Ricerca Operativa 1" 4th edition , example
// at page 175-179
TEST non_trivial_network1(void) {
    int32_t nnodes = 7;
    FlowNetwork net = flow_network_create(nnodes);
    MaxFlowResult max_flow_result = max_flow_result_create(nnodes);
    net.source_vertex = 0;
    net.sink_vertex = 7 - 1;

    *network_cap(&net, 1 - 1, 4 - 1) = 2;
    *network_cap(&net, 1 - 1, 2 - 1) = 2;
    *network_cap(&net, 1 - 1, 5 - 1) = 2;
    *network_cap(&net, 5 - 1, 2 - 1) = 1;
    *network_cap(&net, 2 - 1, 4 - 1) = 1;
    *network_cap(&net, 5 - 1, 6 - 1) = 2;
    *network_cap(&net, 2 - 1, 6 - 1) = 2;
    *network_cap(&net, 2 - 1, 3 - 1) = 2;
    *network_cap(&net, 4 - 1, 3 - 1) = 1;
    *network_cap(&net, 6 - 1, 3 - 1) = 1;
    *network_cap(&net, 3 - 1, 7 - 1) = 2;
    *network_cap(&net, 6 - 1, 7 - 1) = 4;

    double max_flow = push_relabel_max_flow(&net, &max_flow_result);
    ASSERT_IN_RANGE(5, max_flow, 1e-4);
    ASSERT_EQ(1, max_flow_result.bipartition.data[1 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[2 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[3 - 1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[4 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[5 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[6 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[7 - 1]);

    flow_network_destroy(&net);
    max_flow_result_destroy(&max_flow_result);
    PASS();
}

// From Matteo Fischetti "Lezioni di Ricerca Operativa 1" 4th edition , exercise
// 9-23 at page 197-198
TEST non_trivial_network2(void) {
    int32_t nnodes = 7;
    FlowNetwork net = flow_network_create(nnodes);
    MaxFlowResult max_flow_result = max_flow_result_create(nnodes);
    net.source_vertex = 0;
    net.sink_vertex = 7 - 1;

    *network_cap(&net, 1 - 1, 2 - 1) = 6;
    *network_cap(&net, 1 - 1, 4 - 1) = 5;
    *network_cap(&net, 1 - 1, 5 - 1) = 10;
    *network_cap(&net, 5 - 1, 4 - 1) = 12;
    *network_cap(&net, 4 - 1, 2 - 1) = 5;
    *network_cap(&net, 4 - 1, 3 - 1) = 7;
    *network_cap(&net, 2 - 1, 3 - 1) = 5;
    *network_cap(&net, 2 - 1, 6 - 1) = 12;
    *network_cap(&net, 2 - 1, 6 - 1) = 12;
    *network_cap(&net, 6 - 1, 3 - 1) = 3;
    *network_cap(&net, 6 - 1, 7 - 1) = 15;
    *network_cap(&net, 3 - 1, 7 - 1) = 4;

    double max_flow = push_relabel_max_flow(&net, &max_flow_result);
    ASSERT_IN_RANGE(15, max_flow, 1e-4);
    ASSERT_EQ(1, max_flow_result.bipartition.data[1 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[2 - 1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[3 - 1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[4 - 1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[5 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[6 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[7 - 1]);

    flow_network_destroy(&net);
    max_flow_result_destroy(&max_flow_result);
    PASS();
}

// From Matteo Fischetti "Lezioni di Ricerca Operativa 1" 4th edition , exercise
// 9-24 at page 198
TEST non_trivial_network3(void) {
    int32_t nnodes = 8;
    FlowNetwork net = flow_network_create(nnodes);
    MaxFlowResult max_flow_result = max_flow_result_create(nnodes);
    net.source_vertex = 0;
    net.sink_vertex = 8 - 1;

    *network_cap(&net, 1 - 1, 5 - 1) = 7;
    *network_cap(&net, 1 - 1, 2 - 1) = 3;
    *network_cap(&net, 1 - 1, 6 - 1) = 3;
    *network_cap(&net, 6 - 1, 2 - 1) = 5;
    *network_cap(&net, 5 - 1, 2 - 1) = 1;
    *network_cap(&net, 2 - 1, 3 - 1) = 2;
    *network_cap(&net, 5 - 1, 4 - 1) = 7;
    *network_cap(&net, 4 - 1, 2 - 1) = 2;
    *network_cap(&net, 6 - 1, 3 - 1) = 3;
    *network_cap(&net, 6 - 1, 7 - 1) = 3;
    *network_cap(&net, 3 - 1, 7 - 1) = 5;
    *network_cap(&net, 3 - 1, 4 - 1) = 2;
    *network_cap(&net, 7 - 1, 8 - 1) = 6;
    *network_cap(&net, 4 - 1, 8 - 1) = 5;

    double max_flow = push_relabel_max_flow(&net, &max_flow_result);
    ASSERT_IN_RANGE(10, max_flow, 1e-4);
    ASSERT_EQ(1, max_flow_result.bipartition.data[1 - 1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[2 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[3 - 1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[4 - 1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[5 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[6 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[7 - 1]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[8 - 1]);

    flow_network_destroy(&net);
    max_flow_result_destroy(&max_flow_result);
    PASS();
}

TEST no_path_flow(void) {

    int32_t nnodes = 4;
    FlowNetwork net = flow_network_create(nnodes);
    MaxFlowResult max_flow_result = max_flow_result_create(nnodes);
    net.source_vertex = 0;
    net.sink_vertex = 3;

    // 3 Nodes are circularly linked with the source node with max capacity 2 in
    // any direction, while the sink is completely detached (capacity 0)
    *network_cap(&net, 0, 1) = 2;
    *network_cap(&net, 1, 0) = 2;
    *network_cap(&net, 1, 2) = 2;
    *network_cap(&net, 2, 1) = 2;
    *network_cap(&net, 2, 0) = 2;
    *network_cap(&net, 0, 2) = 2;

    double max_flow = push_relabel_max_flow(&net, &max_flow_result);
    ASSERT_IN_RANGE(0, max_flow, 1e-4);
    ASSERT_EQ(1, max_flow_result.bipartition.data[0]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[1]);
    ASSERT_EQ(1, max_flow_result.bipartition.data[2]);
    ASSERT_EQ(0, max_flow_result.bipartition.data[3]);
    flow_network_destroy(&net);
    max_flow_result_destroy(&max_flow_result);
    PASS();
}

TEST single_path_flow(void) {
    for (int32_t nnodes = 2; nnodes < MAX_NUM_NODES_TO_TEST; nnodes++) {
        FlowNetwork net = flow_network_create(nnodes);
        MaxFlowResult max_flow_result = max_flow_result_create(nnodes);
        net.source_vertex = 0;
        net.sink_vertex = nnodes - 1;

        double min_cap = INFINITY;

        for (int32_t i = 0; i < nnodes - 1; i++) {
            double r = rand() % (nnodes / 2);
            *network_cap(&net, i, i + 1) = r;
            *network_cap(&net, i + 1, i) = rand() % 256;
            min_cap = MIN(min_cap, r);
        }

        double max_flow = push_relabel_max_flow(&net, &max_flow_result);
        ASSERT_IN_RANGE(min_cap, max_flow, 1e-4);

        flow_network_destroy(&net);
        max_flow_result_destroy(&max_flow_result);
    }

    PASS();
}

TEST two_path_flow(void) {
    for (int32_t blen = 2; blen < MAX_NUM_NODES_TO_TEST / 2; blen++) {
        int32_t nnodes = blen * 2 + 2;
        FlowNetwork net = flow_network_create(nnodes);

        MaxFlowResult max_flow_result = max_flow_result_create(nnodes);
        net.source_vertex = 0;
        net.sink_vertex = nnodes - 1;

        double min_cap1 = INFINITY;
        double min_cap2 = INFINITY;

        *network_cap(&net, 0, 1) = 99999;
        *network_cap(&net, 0, 2) = 99999;

        *network_cap(&net, blen * 2 - 1, nnodes - 1) = 99999;
        *network_cap(&net, blen * 2, nnodes - 1) = 99999;

        for (int32_t i = 1; i < nnodes; i += 2) {
            if (i + 2 >= nnodes) {
                break;
            }
            if (i == nnodes - 1) {
                continue;
            }
            double r = rand() % (nnodes / 2);
            *network_cap(&net, i, i + 2) = r;
            *network_cap(&net, i + 2, i) = rand() % 256;
            min_cap1 = MIN(min_cap1, r);
        }

        for (int32_t i = 2; i < nnodes; i += 2) {
            if (i + 2 >= nnodes) {
                break;
            }
            if (i == nnodes - 1) {
                continue;
            }
            double r = rand() % (nnodes / 2);
            *network_cap(&net, i, i + 2) = r;
            *network_cap(&net, i + 2, i) = rand() % 256;
            min_cap2 = MIN(min_cap2, r);
        }

        double max_flow = push_relabel_max_flow(&net, &max_flow_result);
        ASSERT_IN_RANGE(min_cap1 + min_cap2, max_flow, 1e-4);

        flow_network_destroy(&net);
        max_flow_result_destroy(&max_flow_result);
    }

    PASS();
}

/* Add all the definitions that need to be in the test runner's main file.
 */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    srand(time(NULL));
    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */

    /* If tests are run outside of a suite, a default suite is used. */
    RUN_TEST(CLRS_network);
    RUN_TEST(non_trivial_network1);
    RUN_TEST(non_trivial_network2);
    RUN_TEST(non_trivial_network3);
    RUN_TEST(no_path_flow);
    RUN_TEST(single_path_flow);
    RUN_TEST(two_path_flow);

    GREATEST_MAIN_END(); /* display results */
}
