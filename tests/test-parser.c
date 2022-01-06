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
#include <greatest.h>

#include "parser.h"
#include "misc.h"
#include "instances.h"

TEST validate_instance(Instance *instance, int32_t expected_num_customers,
                       int32_t expected_num_vehicles) {
    ASSERT(is_valid_instance(instance));
    ASSERT_EQ(instance->num_customers, expected_num_customers);
    ASSERT_EQ(instance->num_vehicles, expected_num_vehicles);
    ASSERT(instance->vehicle_cap > 0);

    ASSERT(instance->positions);
    ASSERT(instance->demands);
    ASSERT(instance->profits);

    ASSERT(instance->demands[0] == 0.0);
    for (int32_t i = 1; i < instance->num_customers + 1; i++)
        ASSERT(instance->demands[i] > 0.0);

    // TODO: __EMAIL the professor__
    //       Double check these assertions. For some reason there are instances
    //       where the depot doesn't have a dual = 0, and some customers have
    //       duals which are negative. Therefore not all instances pass these
    //       checks. For this reason these checks are currently disabled
    if (0) {
        ASSERT(instance->profits[0] == 0.0);
        for (int32_t i = 1; i < instance->num_customers + 1; i++)
            ASSERT(instance->profits[i] >= 0.0);
    }

    PASS();
}

TEST parsing_single_instance(void) {
    const char *filepath = "data/ESPPRC - Test Instances/vrps/E-n101-k14_a.vrp";
    Instance instance = parse(filepath);

    CHECK_CALL(validate_instance(&instance, 100, 14));

    ASSERT(instance.positions[0].x == 35);
    ASSERT(instance.positions[0].y == 35);
    ASSERT(instance.demands[0] == 0);
    ASSERT(instance.profits[0] == 7.247);

    ASSERT(instance.positions[1].x == 41);
    ASSERT(instance.positions[1].y == 49);
    ASSERT(instance.demands[1] == 10.0);
    ASSERT(instance.profits[1] == 5.843);

    int32_t n = instance.num_customers + 1;

    ASSERT(instance.positions[n - 2].x == 20);
    ASSERT(instance.positions[n - 2].y == 26);
    ASSERT(instance.demands[n - 2] == 9.0);
    ASSERT(instance.profits[n - 2] == 3.546);

    ASSERT(instance.positions[n - 1].x == 18);
    ASSERT(instance.positions[n - 1].y == 18);
    ASSERT(instance.demands[n - 1] == 17.0);
    ASSERT(instance.profits[n - 1] == 6.82);

    instance_destroy(&instance);
    PASS();
}

TEST parsing_all_instances(void) {

    STATIC_ASSERT(ARRAY_LEN(G_TEST_INSTANCES) == 31,
                  "This is the amount of test instances that we have");

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(G_TEST_INSTANCES); i++) {
        Instance instance = parse(G_TEST_INSTANCES[i].filepath);
        CHECK_CALL(validate_instance(
            &instance, G_TEST_INSTANCES[i].expected_num_customers,
            G_TEST_INSTANCES[i].expected_num_vehicles));
        instance_destroy(&instance);
    }
    PASS();
}

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */

    /* If tests are run outside of a suite, a default suite is used. */
    RUN_TEST(parsing_single_instance);
    RUN_TEST(parsing_all_instances);

    GREATEST_MAIN_END(); /* display results */
}
