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

void max_flow_create_push_relabel(MaxFlow *mf, int32_t nnodes) {
    mf->payload.height = malloc(nnodes * sizeof(*mf->payload.height));
    mf->payload.excess_flow = malloc(nnodes * sizeof(*mf->payload.excess_flow));
    mf->payload.curr_neigh = malloc(nnodes * sizeof(*mf->payload.curr_neigh));
    mf->payload.list = malloc((nnodes - 2) * sizeof(*mf->payload.list));

#ifndef NDEBUG
    // randomly initialize the array to make accumulation errors apparent
    for (int32_t i = 0; i < nnodes; i++) {
        mf->payload.height[i] = rand();
        mf->payload.excess_flow[i] = (double)rand() / RAND_MAX;
        mf->payload.curr_neigh[i] = rand();
    }

    for (int32_t i = 0; i < nnodes - 2; i++) {
        mf->payload.list[i] = rand();
    }
#endif
}

void max_flow_algo_push_relabel(const FlowNetwork *net, MaxFlow *mf, int32_t s,
                                int32_t t, MaxFlowResult *result) {}
