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

#if 0
const char *SMALL_TEST_INSTANCE = "data/my.vrp";
#else
const char *SMALL_TEST_INSTANCE = "data/ESPPRC - Test Instances/F-n45-k4_a.vrp";
#endif

static struct {
    char *filepath;
    int32_t expected_num_customers;
    int32_t expected_num_vehicles;
} const G_TEST_INSTANCES[] = {
    {"data/ESPPRC - Test Instances/E-n76-k7_a.vrp", 75, 7},
    {"data/ESPPRC - Test Instances/E-n76-k7_b.vrp", 75, 7},
    {"data/ESPPRC - Test Instances/E-n76-k8_a.vrp", 75, 8},
    {"data/ESPPRC - Test Instances/E-n76-k8_b.vrp", 75, 8},
    {"data/ESPPRC - Test Instances/E-n76-k10_a.vrp", 75, 10},
    {"data/ESPPRC - Test Instances/E-n76-k10_b.vrp", 75, 10},
    {"data/ESPPRC - Test Instances/E-n76-k14_a.vrp", 75, 14},
    {"data/ESPPRC - Test Instances/E-n76-k14_b.vrp", 75, 14},
    {"data/ESPPRC - Test Instances/E-n101-k8_a.vrp", 100, 8},
    {"data/ESPPRC - Test Instances/E-n101-k8_b.vrp", 100, 8},
    {"data/ESPPRC - Test Instances/E-n101-k14_a.vrp", 100, 14},
    {"data/ESPPRC - Test Instances/E-n101-k14_b.vrp", 100, 14},
    {"data/ESPPRC - Test Instances/F-n45-k4_a.vrp", 44, 4},
    {"data/ESPPRC - Test Instances/F-n72-k4_a.vrp", 71, 4},
    {"data/ESPPRC - Test Instances/F-n135-k7_a.vrp", 134, 7},
    {"data/ESPPRC - Test Instances/M-n121-k7_a.vrp", 120, 7},
    {"data/ESPPRC - Test Instances/M-n121-k7_b.vrp", 120, 7},
    {"data/ESPPRC - Test Instances/M-n151-k12_a.vrp", 150, 12},
    {"data/ESPPRC - Test Instances/M-n151-k12_b.vrp", 150, 12},
    {"data/ESPPRC - Test Instances/M-n200-k16_a.vrp", 199, 16},
    {"data/ESPPRC - Test Instances/M-n200-k16_b.vrp", 199, 16},
    {"data/ESPPRC - Test Instances/M-n200-k17_a.vrp", 199, 17},
    {"data/ESPPRC - Test Instances/M-n200-k17_b.vrp", 199, 17},
    {"data/ESPPRC - Test Instances/P-n70-k10_a.vrp", 69, 10},
    {"data/ESPPRC - Test Instances/P-n70-k10_b.vrp", 69, 10},
    {"data/ESPPRC - Test Instances/P-n76-k4_a.vrp", 75, 4},
    {"data/ESPPRC - Test Instances/P-n76-k4_b.vrp", 75, 4},
    {"data/ESPPRC - Test Instances/P-n76-k5_a.vrp", 75, 5},
    {"data/ESPPRC - Test Instances/P-n76-k5_b.vrp", 75, 5},
    {"data/ESPPRC - Test Instances/P-n101-k4_a.vrp", 100, 4},
    {"data/ESPPRC - Test Instances/P-n101-k4_b.vrp", 100, 4},
};

#if __cplusplus
}
#endif
