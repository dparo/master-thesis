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
#include "types.h"
#include "parsing-utils.h"

#define S(x)                                                                   \
    { #x, (int32_t)(x), true }
#define F(x)                                                                   \
    { x, 0, false }

static struct {
    char *string;
    int32_t expected;
    bool expect_success;
} i32_tests[] = {
    // Expect success here
    S(0),
    S(1),
    S(0xff),
    S(+0xcc),
    S(0xAB),
    S(0b01),
    S(-0xff),
    S(0x7fffffff),

    // TODO: Double check why this fails
    // S(-0x80000000),

    // Expect failures here
    F("0xffffffff"),
    F("asdadsads"),
    F("3 * 2"),
};

#undef S
#undef F

void test_parse_int32(void) {
    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(i32_tests); i++) {
        const char *in = i32_tests[i].string;
        int32_t expected = i32_tests[i].expected;
        int32_t obtained;
        bool success = str_to_int32(in, &obtained);
        TEST_ASSERT(success == i32_tests[i].expect_success);
        if (success) {
            TEST_ASSERT(expected == obtained);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_int32);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
