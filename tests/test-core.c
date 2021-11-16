
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

#include "parser.h"
#include "core.h"
#include "core-utils.h"

TEST tour_creation(void) {
    const char *filepath = "data/ESPPRC - Test Instances/vrps/E-n101-k14_a.vrp";
    Instance instance = parse(filepath);
    Tour tour = tour_create(&instance);
    ASSERT(tour.comp);
    ASSERT_EQ(instance.num_customers, tour.num_customers);
    tour_destroy(&tour);
    instance_destroy(&instance);
    PASS();
}

TEST calling_sxpos(void) {
    for (int32_t n = 0; n < 200; n++) {
        int32_t pos = 0;
        for (int32_t i = 0; i < n; i++) {
            for (int32_t j = i + 1; j < n; j++) {
                ASSERT(sxpos(n, i, j) == pos);
                pos++;
            }
        }
    }
    PASS();
}

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */

    /* If tests are run outside of a suite, a default suite is used. */
    RUN_TEST(tour_creation);
    RUN_TEST(calling_sxpos);

    GREATEST_MAIN_END(); /* display results */
}
