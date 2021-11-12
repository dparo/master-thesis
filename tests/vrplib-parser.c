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

static void validate_instance(Instance *instance,
                              int32_t expected_num_customers,
                              int32_t expected_num_vehicles) {
    TEST_ASSERT_EQUAL(instance->num_customers, expected_num_customers);
    TEST_ASSERT_EQUAL(instance->num_vehicles, expected_num_vehicles);
    TEST_ASSERT(instance->vehicle_cap > 0);

    TEST_ASSERT_NOT_NULL(instance->positions);
    TEST_ASSERT_NOT_NULL(instance->demands);
    TEST_ASSERT(instance->duals || instance->edge_weight);

    TEST_ASSERT(instance->demands[0] == 0.0);
    for (int32_t i = 1; i < instance->num_customers + 1; i++)
        TEST_ASSERT(instance->demands[i] > 0.0);

    if (instance->duals) {
        TEST_ASSERT(instance->duals[0] == 0.0);
        for (int32_t i = 1; i < instance->num_customers + 1; i++)
            TEST_ASSERT(instance->duals[i] >= 0.0);

        // TODO: __EMAIL the professor__
        //       Double check these assertions. For some reason there are
        //       instances where the depot doesn't have a dual = 0, and some
        //       customers have duals which are negative. Therefore not all
        //       instances pass these checks. For this reason these checks are
        //       currently disabled
        if (0) {
            TEST_ASSERT(instance->duals[0] == 0.0);
            for (int32_t i = 1; i < instance->num_customers + 1; i++)
                TEST_ASSERT(instance->duals[i] >= 0.0);
        }
    } else if (instance->edge_weight) {
        // TODO: If there's anything todo here
    }
}

static void test_parser_on_single_instance(void) {
    Instance instance = parse_vrplib_instance("./data/CVRP/toy.vrp");
    validate_instance(&instance, 5, 0);
    instance_destroy(&instance);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_on_single_instance);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
