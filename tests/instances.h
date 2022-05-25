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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include "types.h"

static struct {
    char *filepath;
    int32_t expected_num_customers;
    int32_t expected_num_vehicles;
    double best_primal;
} const G_TEST_INSTANCES[] = {
    // Family F
    {"data/ESPPRC - Test Instances/vrps/F-n45-k4_a.vrp", 44, 4, -11.967},

    // Family E
    {"data/ESPPRC - Test Instances/vrps/E-n76-k7_a.vrp", 75, 7, -6.032},
    {"data/ESPPRC - Test Instances/vrps/E-n76-k8_a.vrp", 75, 8, -6.635},
    {"data/ESPPRC - Test Instances/vrps/E-n76-k10_a.vrp", 75, 10, -3.81},
    {"data/ESPPRC - Test Instances/vrps/E-n76-k14_a.vrp", 75, 14, -3.788},
    // {"data/ESPPRC - Test Instances/vrps/E-n101-k8_a.vrp", 100, 8, -23.977},
    // {"data/ESPPRC - Test Instances/vrps/E-n101-k14_a.vrp", 100, 14, -6.667},

    // Family P
    // {"data/ESPPRC - Test Instances/vrps/P-n70-k10_a.vrp", 69, 10, -2.852},
    {"data/ESPPRC - Test Instances/vrps/P-n76-k4_a.vrp", 75, 4, -2.903},
    {"data/ESPPRC - Test Instances/vrps/P-n76-k5_a.vrp", 75, 5, -3.96},
    {"data/ESPPRC - Test Instances/vrps/P-n101-k4_a.vrp", 100, 4, -7.219},
};

#if __cplusplus
}
#endif
