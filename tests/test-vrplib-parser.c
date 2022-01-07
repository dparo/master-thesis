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
    ASSERT(is_valid_instance(instance));
    ASSERT_EQ(expected_num_customers, instance->num_customers);
    ASSERT_EQ(expected_num_vehicles, instance->num_vehicles);
    ASSERT(instance->vehicle_cap > 0);

    ASSERT(instance->positions);
    ASSERT(instance->demands);
    ASSERT(instance->profits || instance->edge_weight);

    ASSERT(instance->demands[0] == 0.0);
    for (int32_t i = 1; i < instance->num_customers + 1; i++)
        ASSERT(instance->demands[i] > 0.0);

    if (instance->profits) {
        ASSERT(instance->profits[0] == 0.0);
        for (int32_t i = 1; i < instance->num_customers + 1; i++)
            ASSERT(instance->profits[i] >= 0.0);

        // TODO: __EMAIL the professor__
        //       Double check these assertions. For some reason there are
        //       instances where the depot doesn't have a dual = 0, and some
        //       customers have duals which are negative. Therefore not all
        //       instances pass these checks. For this reason these checks are
        //       currently disabled
        if (0) {
            ASSERT(instance->profits[0] == 0.0);
            for (int32_t i = 1; i < instance->num_customers + 1; i++)
                ASSERT(instance->profits[i] >= 0.0);
        }
    } else if (instance->edge_weight) {
        // TODO: If there's anything todo here
    }
    PASS();
}

TEST parsing_single_instance(void) {
    Instance instance = parse("./data/CVRP/toy.vrp");
    CHECK_CALL(validate_instance(&instance, 5, 1));
    instance_destroy(&instance);
    PASS();
}

#define EPS ((double)1e-2)
/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */

    /* If tests are run outside of a suite, a default suite is used. */
    RUN_TEST(parsing_single_instance);

    GREATEST_MAIN_END(); /* display results */
}
