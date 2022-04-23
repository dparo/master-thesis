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

TEST weird_network(void) {
    int32_t nnodes = 4;
    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 1;
    int32_t sink_vertex = 0;

    flow_net_set_cap(&net, 1, 2, 40);
    flow_net_set_cap(&net, 2, 1, 60);
    flow_net_set_cap(&net, 1, 0, 80);
    flow_net_set_cap(&net, 0, 1, 40);
    flow_net_set_cap(&net, 1, 3, 40);
    flow_net_set_cap(&net, 3, 1, 20);

    flow_net_set_cap(&net, 2, 0, 20);
    flow_net_set_cap(&net, 0, 2, 20);
    flow_net_set_cap(&net, 3, 0, 60);
    flow_net_set_cap(&net, 0, 3, 80);

    max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

TEST weird_network2(void) {
    /* net->nnodes = 4, source = 2, sink = 3
    EDGES:
        flow(0, 0)/cap(0, 0) = 0 / 0
        flow(0, 1)/cap(0, 1) = 0 / 0.8
        flow(0, 2)/cap(0, 2) = 0 / 0.8
        flow(0, 3)/cap(0, 3) = 0 / 0
        flow(1, 0)/cap(1, 0) = 0 / 0
        flow(1, 1)/cap(1, 1) = 0 / 0
        flow(1, 2)/cap(1, 2) = 0 / 0
        flow(1, 3)/cap(1, 3) = 0 / 0
        flow(2, 0)/cap(2, 0) = 0 / 0.2
        flow(2, 1)/cap(2, 1) = 0 / 0.6
        flow(2, 2)/cap(2, 2) = 0 / 0
        flow(2, 3)/cap(2, 3) = 0 / 0.6
        flow(3, 0)/cap(3, 0) = 0 / 0
        flow(3, 1)/cap(3, 1) = 0 / 0.2
        flow(3, 2)/cap(3, 2) = 0 / 0.8
        flow(3, 3)/cap(3, 3) = 0 / 0 */

    int32_t nnodes = 4;
    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 2;
    int32_t sink_vertex = 3;

    flow_net_set_cap(&net, 0, 0, 0);
    flow_net_set_cap(&net, 0, 1, 80);
    flow_net_set_cap(&net, 0, 2, 80);
    flow_net_set_cap(&net, 0, 3, 0);
    flow_net_set_cap(&net, 1, 0, 0);
    flow_net_set_cap(&net, 1, 1, 0);
    flow_net_set_cap(&net, 1, 2, 0);
    flow_net_set_cap(&net, 1, 3, 0);
    flow_net_set_cap(&net, 2, 0, 20);
    flow_net_set_cap(&net, 2, 1, 60);
    flow_net_set_cap(&net, 2, 2, 0);
    flow_net_set_cap(&net, 2, 3, 60);
    flow_net_set_cap(&net, 3, 0, 0);
    flow_net_set_cap(&net, 3, 1, 20);
    flow_net_set_cap(&net, 3, 2, 80);
    flow_net_set_cap(&net, 3, 3, 0);

    max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

TEST weird_network3(void) {
    /*
    net->nnodes = 5, source = 0, sink = 2
    EDGES:
        flow(0, 0)/cap(0, 0) = 0 / 0
        flow(0, 1)/cap(0, 1) = 0 / 0.4
        flow(0, 2)/cap(0, 2) = 0 / 0.8
        flow(0, 3)/cap(0, 3) = 0 / 0.6
        flow(0, 4)/cap(0, 4) = 0 / 0.8
        flow(1, 0)/cap(1, 0) = 0 / 0.8
        flow(1, 1)/cap(1, 1) = 0 / 0
        flow(1, 2)/cap(1, 2) = 0 / 0.2
        flow(1, 3)/cap(1, 3) = 0 / 0.4
        flow(1, 4)/cap(1, 4) = 0 / 0.8
        flow(2, 0)/cap(2, 0) = 0 / 0.8
        flow(2, 1)/cap(2, 1) = 0 / 0.6
        flow(2, 2)/cap(2, 2) = 0 / 0
        flow(2, 3)/cap(2, 3) = 0 / 0.6
        flow(2, 4)/cap(2, 4) = 0 / 0.8
        flow(3, 0)/cap(3, 0) = 0 / 0.4
        flow(3, 1)/cap(3, 1) = 0 / 0.6
        flow(3, 2)/cap(3, 2) = 0 / 0.2
        flow(3, 3)/cap(3, 3) = 0 / 0
        flow(3, 4)/cap(3, 4) = 0 / 0.2
        flow(4, 0)/cap(4, 0) = 0 / 0
        flow(4, 1)/cap(4, 1) = 0 / 0.4
        flow(4, 2)/cap(4, 2) = 0 / 0.8
        flow(4, 3)/cap(4, 3) = 0 / 0.8
        flow(4, 4)/cap(4, 4) = 0 / 0

    */

    int32_t nnodes = 5;
    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 0;
    int32_t sink_vertex = 2;

    flow_net_set_cap(&net, 0, 0, 0);
    flow_net_set_cap(&net, 0, 1, 40);
    flow_net_set_cap(&net, 0, 2, 80);
    flow_net_set_cap(&net, 0, 3, 60);
    flow_net_set_cap(&net, 0, 4, 80);
    flow_net_set_cap(&net, 1, 0, 80);
    flow_net_set_cap(&net, 1, 1, 0);
    flow_net_set_cap(&net, 1, 2, 20);
    flow_net_set_cap(&net, 1, 3, 40);
    flow_net_set_cap(&net, 1, 4, 80);
    flow_net_set_cap(&net, 2, 0, 80);
    flow_net_set_cap(&net, 2, 1, 60);
    flow_net_set_cap(&net, 2, 2, 0);
    flow_net_set_cap(&net, 2, 3, 60);
    flow_net_set_cap(&net, 2, 4, 80);
    flow_net_set_cap(&net, 3, 0, 40);
    flow_net_set_cap(&net, 3, 1, 60);
    flow_net_set_cap(&net, 3, 2, 20);
    flow_net_set_cap(&net, 3, 3, 0);
    flow_net_set_cap(&net, 3, 4, 20);
    flow_net_set_cap(&net, 4, 0, 0);
    flow_net_set_cap(&net, 4, 1, 40);
    flow_net_set_cap(&net, 4, 2, 80);
    flow_net_set_cap(&net, 4, 3, 80);
    flow_net_set_cap(&net, 4, 4, 0);

    max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

TEST CLRS_network(void) {
    int32_t nnodes = 6;

    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 0;
    int32_t sink_vertex = 5;

    flow_net_set_cap(&net, 0, 1, 16);
    flow_net_set_cap(&net, 0, 2, 13);
    flow_net_set_cap(&net, 1, 2, 10);
    flow_net_set_cap(&net, 2, 1, 40);
    flow_net_set_cap(&net, 1, 3, 12);
    flow_net_set_cap(&net, 3, 2, 9);
    flow_net_set_cap(&net, 2, 4, 14);
    flow_net_set_cap(&net, 4, 3, 7);
    flow_net_set_cap(&net, 3, 5, 20);
    flow_net_set_cap(&net, 4, 5, 4);

    double max_flow =
        max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);

    ASSERT_EQ(23, max_flow);
    ASSERT_EQ(BLACK, result.colors[0]);
    ASSERT_EQ(BLACK, result.colors[1]);
    ASSERT_EQ(BLACK, result.colors[2]);
    ASSERT_EQ(WHITE, result.colors[3]);
    ASSERT_EQ(BLACK, result.colors[4]);
    ASSERT_EQ(WHITE, result.colors[5]);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

// From Matteo Fischetti "Lezioni di Ricerca Operativa 1" 4th edition , example
// at page 175-179
TEST non_trivial_network1(void) {
    int32_t nnodes = 7;
    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 0;
    int32_t sink_vertex = 7 - 1;

    flow_net_set_cap(&net, 1 - 1, 4 - 1, 2);
    flow_net_set_cap(&net, 1 - 1, 2 - 1, 2);
    flow_net_set_cap(&net, 1 - 1, 5 - 1, 2);
    flow_net_set_cap(&net, 5 - 1, 2 - 1, 1);
    flow_net_set_cap(&net, 2 - 1, 4 - 1, 1);
    flow_net_set_cap(&net, 5 - 1, 6 - 1, 2);
    flow_net_set_cap(&net, 2 - 1, 6 - 1, 2);
    flow_net_set_cap(&net, 2 - 1, 3 - 1, 2);
    flow_net_set_cap(&net, 4 - 1, 3 - 1, 1);
    flow_net_set_cap(&net, 6 - 1, 3 - 1, 1);
    flow_net_set_cap(&net, 3 - 1, 7 - 1, 2);
    flow_net_set_cap(&net, 6 - 1, 7 - 1, 4);

    double max_flow =
        max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);

    ASSERT_EQ(5, max_flow);
    ASSERT_EQ(BLACK, result.colors[1 - 1]);
    ASSERT_EQ(WHITE, result.colors[2 - 1]);
    ASSERT_EQ(WHITE, result.colors[3 - 1]);
    ASSERT_EQ(BLACK, result.colors[4 - 1]);
    ASSERT_EQ(WHITE, result.colors[5 - 1]);
    ASSERT_EQ(WHITE, result.colors[6 - 1]);
    ASSERT_EQ(WHITE, result.colors[7 - 1]);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

// From Matteo Fischetti "Lezioni di Ricerca Operativa 1" 4th edition , exercise
// 9-23 at page 197-198
TEST non_trivial_network2(void) {
    int32_t nnodes = 7;
    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 0;
    int32_t sink_vertex = 7 - 1;

    flow_net_set_cap(&net, 1 - 1, 2 - 1, 6);
    flow_net_set_cap(&net, 1 - 1, 4 - 1, 5);
    flow_net_set_cap(&net, 1 - 1, 5 - 1, 10);
    flow_net_set_cap(&net, 5 - 1, 4 - 1, 12);
    flow_net_set_cap(&net, 4 - 1, 2 - 1, 5);
    flow_net_set_cap(&net, 4 - 1, 3 - 1, 7);
    flow_net_set_cap(&net, 2 - 1, 3 - 1, 5);
    flow_net_set_cap(&net, 2 - 1, 6 - 1, 12);
    flow_net_set_cap(&net, 2 - 1, 6 - 1, 12);
    flow_net_set_cap(&net, 6 - 1, 3 - 1, 3);
    flow_net_set_cap(&net, 6 - 1, 7 - 1, 15);
    flow_net_set_cap(&net, 3 - 1, 7 - 1, 4);

    double max_flow =
        max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);

    ASSERT_EQ(15, max_flow);
    ASSERT_EQ(BLACK, result.colors[1 - 1]);
    ASSERT_EQ(WHITE, result.colors[2 - 1]);
    ASSERT_EQ(BLACK, result.colors[3 - 1]);
    ASSERT_EQ(BLACK, result.colors[4 - 1]);
    ASSERT_EQ(BLACK, result.colors[5 - 1]);
    ASSERT_EQ(WHITE, result.colors[6 - 1]);
    ASSERT_EQ(WHITE, result.colors[7 - 1]);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

// From Matteo Fischetti "Lezioni di Ricerca Operativa 1" 4th edition , exercise
// 9-24 at page 198
TEST non_trivial_network3(void) {
    int32_t nnodes = 8;
    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 0;
    int32_t sink_vertex = 8 - 1;

    flow_net_set_cap(&net, 1 - 1, 5 - 1, 7);
    flow_net_set_cap(&net, 1 - 1, 2 - 1, 3);
    flow_net_set_cap(&net, 1 - 1, 6 - 1, 3);
    flow_net_set_cap(&net, 6 - 1, 2 - 1, 5);
    flow_net_set_cap(&net, 5 - 1, 2 - 1, 1);
    flow_net_set_cap(&net, 2 - 1, 3 - 1, 2);
    flow_net_set_cap(&net, 5 - 1, 4 - 1, 7);
    flow_net_set_cap(&net, 4 - 1, 2 - 1, 2);
    flow_net_set_cap(&net, 6 - 1, 3 - 1, 3);
    flow_net_set_cap(&net, 6 - 1, 7 - 1, 3);
    flow_net_set_cap(&net, 3 - 1, 7 - 1, 5);
    flow_net_set_cap(&net, 3 - 1, 4 - 1, 2);
    flow_net_set_cap(&net, 7 - 1, 8 - 1, 6);
    flow_net_set_cap(&net, 4 - 1, 8 - 1, 5);

    double max_flow =
        max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);
    ASSERT_EQ(10, max_flow);
    ASSERT_EQ(BLACK, result.colors[1 - 1]);
    ASSERT_EQ(BLACK, result.colors[2 - 1]);
    ASSERT_EQ(WHITE, result.colors[3 - 1]);
    ASSERT_EQ(BLACK, result.colors[4 - 1]);
    ASSERT_EQ(BLACK, result.colors[5 - 1]);
    ASSERT_EQ(WHITE, result.colors[6 - 1]);
    ASSERT_EQ(WHITE, result.colors[7 - 1]);
    ASSERT_EQ(WHITE, result.colors[8 - 1]);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

TEST no_path_flow(void) {
    int32_t nnodes = 4;
    FlowNetwork net = {0};
    MaxFlow mf = {0};
    MaxFlowResult result = {0};

    flow_network_create_v2(&net, nnodes);
    max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
    max_flow_result_create_v2(&result, nnodes);

    int32_t source_vertex = 0;
    int32_t sink_vertex = 3;

    // 3 Nodes are circulary linked with the source node with max capacity 2 in
    // any direction, while the sink is completely detached (capacity 0)
    flow_net_set_cap(&net, 0, 1, 2);
    flow_net_set_cap(&net, 1, 0, 2);
    flow_net_set_cap(&net, 1, 2, 2);
    flow_net_set_cap(&net, 2, 1, 2);
    flow_net_set_cap(&net, 2, 0, 2);
    flow_net_set_cap(&net, 0, 2, 2);

    double max_flow =
        max_flow_single_pair(&net, &mf, source_vertex, sink_vertex, &result);

    ASSERT_EQ(0, max_flow);
    ASSERT_EQ(BLACK, result.colors[0]);
    ASSERT_EQ(BLACK, result.colors[1]);
    ASSERT_EQ(BLACK, result.colors[2]);
    ASSERT_EQ(WHITE, result.colors[3]);

    CHECK_CALL(
        validate_with_slow_max_flow(&net, source_vertex, sink_vertex, &result));

    max_flow_result_destroy_v2(&result);
    max_flow_destroy(&mf);
    flow_network_destroy_v2(&net);

    PASS();
}

TEST single_path_flow(void) {
    for (int32_t nnodes = 2; nnodes < MAX_NUM_NODES_TO_TEST; nnodes++) {
        FlowNetwork net = {0};
        MaxFlow mf = {0};
        MaxFlowResult result = {0};

        flow_network_create_v2(&net, nnodes);
        max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
        max_flow_result_create_v2(&result, nnodes);

        int32_t source_vertex = 0;
        int32_t sink_vertex = nnodes - 1;

        flow_t min_cap = INFINITY;

        for (int32_t i = 0; i < nnodes - 1; i++) {
            flow_t c = rand() % 10;
            flow_t rc = rand() % 10;
            flow_net_set_cap(&net, i, i + 1, c);
            flow_net_set_cap(&net, i + 1, i, rc);
            min_cap = MIN(min_cap, c);
        }

        double max_flow = max_flow_single_pair(&net, &mf, source_vertex,
                                               sink_vertex, &result);

        ASSERT_EQ(min_cap, max_flow);

        CHECK_CALL(validate_with_slow_max_flow(&net, source_vertex, sink_vertex,
                                               &result));

        max_flow_result_destroy_v2(&result);
        max_flow_destroy(&mf);
        flow_network_destroy_v2(&net);
    }

    PASS();
}

TEST two_path_flow(void) {
    for (int32_t blen = 2; blen < MAX_NUM_NODES_TO_TEST / 2; blen++) {
        int32_t nnodes = blen * 2 + 2;
        FlowNetwork net = {0};
        MaxFlow mf = {0};
        MaxFlowResult result = {0};

        flow_network_create_v2(&net, nnodes);
        max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
        max_flow_result_create_v2(&result, nnodes);

        int32_t source_vertex = 0;
        int32_t sink_vertex = nnodes - 1;

        flow_t min_cap1 = INFINITY;
        flow_t min_cap2 = INFINITY;

        flow_net_set_cap(&net, 0, 1, 99999);
        flow_net_set_cap(&net, 0, 2, 99999);

        flow_net_set_cap(&net, blen * 2 - 1, nnodes - 1, 99999);
        flow_net_set_cap(&net, blen * 2, nnodes - 1, 99999);

        for (int32_t i = 1; i < nnodes; i += 2) {
            if (i + 2 >= nnodes) {
                break;
            }
            if (i == nnodes - 1) {
                continue;
            }
            flow_t c = rand() % 10;
            flow_t rc = rand() % 10;
            flow_net_set_cap(&net, i, i + 2, c);
            flow_net_set_cap(&net, i + 2, i, rc);
            min_cap1 = MIN(min_cap1, c);
        }

        for (int32_t i = 2; i < nnodes; i += 2) {
            if (i + 2 >= nnodes) {
                break;
            }
            if (i == nnodes - 1) {
                continue;
            }
            flow_t c = rand() % 10;
            flow_t rc = rand() % 10;
            flow_net_set_cap(&net, i, i + 2, c);
            flow_net_set_cap(&net, i + 2, i, rc);
            min_cap2 = MIN(min_cap2, c);
        }

        double max_flow = max_flow_single_pair(&net, &mf, source_vertex,
                                               sink_vertex, &result);

        ASSERT_EQ(min_cap1 + min_cap2, max_flow);

        CHECK_CALL(validate_with_slow_max_flow(&net, source_vertex, sink_vertex,
                                               &result));

        max_flow_result_destroy_v2(&result);
        max_flow_destroy(&mf);
        flow_network_destroy_v2(&net);
    }

    PASS();
}

TEST random_networks(void) {
    const flow_t RAND_VALS[] = {0, 1, 2, 5, 7, 0, 3};

    for (int32_t nnodes = 2; nnodes <= 10; nnodes++) {
        for (int32_t try_it = 0; try_it < 2048; try_it++) {
            FlowNetwork net = {0};
            MaxFlow mf = {0};
            MaxFlowResult result = {0};

            flow_network_create_v2(&net, nnodes);
            max_flow_create(&mf, nnodes, MAXFLOW_ALGO_PUSH_RELABEL);
            max_flow_result_create_v2(&result, nnodes);

            int32_t source_vertex = 0;
            int32_t sink_vertex = nnodes - 1;

            for (int32_t i = 0; i < nnodes; i++) {
                for (int32_t j = 0; j < nnodes; j++) {
                    if (i != j) {
                        flow_t c = RAND_VALS[rand() % ARRAY_LEN(RAND_VALS)];
                        flow_net_set_cap(&net, i, j, c);
                    }
                }
            }

            double max_flow = max_flow_single_pair(&net, &mf, source_vertex,
                                                   sink_vertex, &result);

            ASSERT_EQ(max_flow, result.maxflow);
            CHECK_CALL(validate_with_slow_max_flow(&net, source_vertex,
                                                   sink_vertex, &result));

            CHECK_CALL(validate_with_slow_max_flow(&net, source_vertex,
                                                   sink_vertex, &result));

            max_flow_result_destroy_v2(&result);
            max_flow_destroy(&mf);
            flow_network_destroy_v2(&net);
        }
    }

    PASS();
}

/* Add all the definitions that need to be in the test runner's main file.
 */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */

    /* If tests are run outside of a suite, a default suite is used. */
    RUN_TEST(weird_network);
    RUN_TEST(weird_network2);
    RUN_TEST(weird_network3);
    RUN_TEST(CLRS_network);
    RUN_TEST(non_trivial_network1);
    RUN_TEST(non_trivial_network2);
    RUN_TEST(non_trivial_network3);
    RUN_TEST(no_path_flow);
    RUN_TEST(single_path_flow);
    RUN_TEST(two_path_flow);
    RUN_TEST(random_networks);

    GREATEST_MAIN_END(); /* display results */
}
