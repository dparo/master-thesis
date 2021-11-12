
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

#include <unity.h>

#include "parser.h"
#include "core.h"
#include "core-utils.h"

static void test_tour_create(void) {
    const char *filepath = "data/ESPPRC - Test Instances/E-n101-k14_a.vrp";
    Instance instance = parse_test_instance(filepath);
    Tour tour = tour_create(&instance);
    tour_destroy(&tour);
    instance_destroy(&instance);
}

static void test_sxpos(void) {
    for (int32_t n = 0; n < 200; n++) {
        int32_t pos = 0;
        for (int32_t i = 0; i < n; i++) {
            for (int32_t j = i + 1; j < n; j++) {
                TEST_ASSERT(sxpos(n, i, j) == pos);
                pos++;
            }
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tour_create);
    RUN_TEST(test_sxpos);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
