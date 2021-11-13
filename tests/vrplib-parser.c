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
#include "core-utils.h"
#include "misc.h"
#include "instances.h"

TEST validate_instance(Instance *instance, int32_t expected_num_customers,
                       int32_t expected_num_vehicles) {
    ASSERT_EQ(expected_num_customers, instance->num_customers);
    ASSERT_EQ(expected_num_vehicles, instance->num_vehicles);
    ASSERT(instance->vehicle_cap > 0);

    ASSERT(instance->positions);
    ASSERT(instance->demands);
    ASSERT(instance->duals || instance->edge_weight);

    ASSERT(instance->demands[0] == 0.0);
    for (int32_t i = 1; i < instance->num_customers + 1; i++)
        ASSERT(instance->demands[i] > 0.0);

    if (instance->duals) {
        ASSERT(instance->duals[0] == 0.0);
        for (int32_t i = 1; i < instance->num_customers + 1; i++)
            ASSERT(instance->duals[i] >= 0.0);

        // TODO: __EMAIL the professor__
        //       Double check these assertions. For some reason there are
        //       instances where the depot doesn't have a dual = 0, and some
        //       customers have duals which are negative. Therefore not all
        //       instances pass these checks. For this reason these checks are
        //       currently disabled
        if (0) {
            ASSERT(instance->duals[0] == 0.0);
            for (int32_t i = 1; i < instance->num_customers + 1; i++)
                ASSERT(instance->duals[i] >= 0.0);
        }
    } else if (instance->edge_weight) {
        // TODO: If there's anything todo here
    }
    PASS();
}

TEST parsing_single_instance(void) {
    Instance instance = parse_vrplib_instance("./data/CVRP/toy.vrp");
    CHECK_CALL(validate_instance(&instance, 5, 0));
    instance_destroy(&instance);
    PASS();
}

#define EPS ((double)0.000001)

struct BapCodGeneratedInstanceTest {
    int32_t column_generation_it;
    struct {
        int32_t i;
        double expected_x, expected_y;
        double expected_demand;
    } nodes[100];
    struct {
        int32_t i, j;
        double expected;
    } distances[100];
};

TEST parsing_bapcod_output_instances(void) {
    char filepath[4096];
    for (int32_t i = 0; i <= 62; i++) {
        snprintf(filepath, ARRAY_LEN(filepath),
                 "data/BaPCod generated - Test instances/A-n37-k5.cgit-%d.vrp",
                 i);
        Instance instance = parse_vrplib_instance(filepath);
        CHECK_CALL(validate_instance(&instance, 36, 5));

        if (i == 28) {
            // Some extra checks for this instance
            ASSERT_EQ(10, instance.positions[19 - 1].x);
            ASSERT_EQ(91, instance.positions[19 - 1].y);

            ASSERT_EQ(50, instance.positions[34 - 1].x);
            ASSERT_EQ(2, instance.positions[34 - 1].y);

            ASSERT_EQ(4, instance.demands[19 - 1]);
            ASSERT_EQ(19, instance.demands[33 - 1]);

            ASSERT_IN_RANGE(22.2962, cptp_dist(&instance, 1 - 1, 2 - 1), EPS);
            ASSERT_IN_RANGE(-6.33531, cptp_dist(&instance, 1 - 1, 4 - 1), EPS);
            ASSERT_IN_RANGE(28.2942, cptp_dist(&instance, 3 - 1, 32 - 1), EPS);
            ASSERT_IN_RANGE(40.6364, cptp_dist(&instance, 19 - 1, 34 - 1), EPS);
        }

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
    RUN_TEST(parsing_bapcod_output_instances);

    GREATEST_MAIN_END(); /* display results */
}
