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
#include <unity.h>

#include "parser.h"
#include "misc.h"
#include "instances.h"

static void test_parser_on_single_instance(void) {
    const char *filepath = "res/ESPPRC - Test Instances/E-n101-k14_a.vrp";
    Instance instance = parse(filepath);
    TEST_ASSERT_NOT_NULL(instance.positions);
    TEST_ASSERT_NOT_NULL(instance.demands);
    TEST_ASSERT_NOT_NULL(instance.duals);
    TEST_ASSERT_EQUAL(instance.num_customers, 100);
    TEST_ASSERT_EQUAL(instance.num_vehicles, 14);
    instance_destroy(&instance);
}

static void test_parser_on_all_instances(void) {

    STATIC_ASSERT(ARRAY_LEN(G_TEST_INSTANCES) == 31,
                  "This is the amount of test instances that we have");

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(G_TEST_INSTANCES); i++) {
        Instance instance = parse(G_TEST_INSTANCES[i].filepath);
        TEST_ASSERT_NOT_NULL(instance.positions);
        TEST_ASSERT_NOT_NULL(instance.demands);
        TEST_ASSERT_NOT_NULL(instance.duals);
        TEST_ASSERT_EQUAL(instance.num_customers,
                          G_TEST_INSTANCES[i].expected_num_customers);
        TEST_ASSERT_EQUAL(instance.num_vehicles,
                          G_TEST_INSTANCES[i].expected_num_vehicles);
        instance_destroy(&instance);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_on_single_instance);
    RUN_TEST(test_parser_on_all_instances);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
