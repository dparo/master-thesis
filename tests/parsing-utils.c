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
#include <stdbool.h>

static void test_parse_int32(void) {

#define SS(str, x)                                                             \
    { str, ((int32_t)(x)), true }

#define S(x)                                                                   \
    { #x, ((int32_t)(x)), true }

#define F(x)                                                                   \
    { x, 0, false }

    static struct {
        char *string;
        int32_t expected;
        bool expect_success;
    } i32_tests[] = {
        S(0),
        S(1),
        S(0xff),
        S(+0xcc),
        S(0xAB),
        S(0b01),
        S(-0xff),
        S(0x7fffffff),
        S(-0x80000000),
        S(-0b1010101),
        S(+0b1010101),
        S(0b1010101),

        F("0x"),
        F("-"),
        F("-0x"),

        F("0b"),
        F("-0b"),
        F("-0xasd"),
        F("-0bx10"),
        F("-0xffu"),
        F("-0xfful"),
        F("-1.0"),
        F("+1.0"),
        // OUT of range
        F("0xffffffff"),
        // Text string
        F("asdadsads"),

        // Computations are not allowed
        F("3 * 2"),
    };

#undef SS
#undef S
#undef F

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

static void test_parse_usize(void) {

#define SS(str, x)                                                             \
    { str, ((size_t)(UINT64_C(x))), true }

#define S(x)                                                                   \
    { #x, ((size_t)(UINT64_C(x))), true }

#define F(x)                                                                   \
    { x, 0, false }

    static struct {
        char *string;
        size_t expected;
        bool expect_success;
    } usize_tests[] = {
        S(0),
        S(1),
        S(0xff),
        S(+0xcc),
        S(0xAB),
        S(0b01),
        S(0x7fffffff),
        S(+0b1010101),
        S(0b1010101),
        S(0xffffffff),

        F("-0xff"),
        F("-0b1010101"),
        F("0x"),
        F("-"),
        F("-0x"),
        F("-0x80000000"),

        F("0b"),
        F("-0b"),
        F("-0xasd"),
        F("-0bx10"),
        F("-0xffu"),
        F("-0xfful"),
        F("-1.0"),
        F("+1.0"),
        // OUT of range
        F("0xffffffffffffffffffffffffffff"),
        // Text string
        F("asdadsads"),

        // Computations are not allowed
        F("3 * 2"),
    };

#undef SS
#undef S
#undef F

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(usize_tests); i++) {
        const char *in = usize_tests[i].string;
        size_t expected = usize_tests[i].expected;
        size_t obtained;
        bool success = str_to_usize(in, &obtained);
        TEST_ASSERT(success == usize_tests[i].expect_success);
        if (success) {
            TEST_ASSERT(expected == obtained);
        }
    }
}

static void test_parse_float(void) {

#define SS(str, x)                                                             \
    { str, ((float)(x)), true }

#define S(x)                                                                   \
    { #x, ((float)(x)), true }

#define F(x)                                                                   \
    { x, 0, false }

    static struct {
        char *string;
        float expected;
        bool expect_success;
    } float_tests[] = {

        S(0.0),
        S(1.0),
        S(2.32),
        S(0.04),
        S(.001),
        S(1.),
        S(1e-3),
        S(1E-3),

        S(+0.0),
        S(+1.0),
        S(+2.32),
        S(+0.04),
        S(+.001),
        S(+1.),
        S(+1e-3),
        S(+1E-3),

        S(-0.0),
        S(-1.0),
        S(-2.32),
        S(-0.04),
        S(-.001),
        S(-1.),
        S(-1e-3),
        S(-1E-3),

        S(10.0),
        S(10e4),
        S(.10),
        S(10.0f),
        S(10e4f),
        S(.10f),

        S(-10.0),
        S(-10e4),
        S(-.10),
        S(-10.0f),
        S(-10e4f),
        S(-.10f),

        SS("10f", 10.0f),
        SS("-10f", -10.0f),
        SS("inf", INFINITY),
        SS("infinity", INFINITY),
        SS("-inf", -INFINITY),
        SS("-infinity", -INFINITY),
        SS("nan", NAN),
        SS("-nan", -NAN),

        F("0x"),
        F("-"),
        F("-0x"),

        F("0b"),
        F("-0b"),
        F("-0xasd"),
        F("-0bx10"),
        F("-0xffu"),
        F("-0xfful"),
        // Text string
        F("asdadsads"),

        // Computations are not allowed
        F("3 * 2"),
    };

#undef SS
#undef S
#undef F
    const float EPSILON = 0.0001;
    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(float_tests); i++) {
        const char *in = float_tests[i].string;
        float expected = float_tests[i].expected;
        float obtained;
        bool success = str_to_float(in, &obtained);
        TEST_ASSERT(success == float_tests[i].expect_success);
        if (success) {
            TEST_ASSERT_FLOAT_WITHIN(EPSILON, expected, obtained);
        }
    }
}

static void test_parse_double(void) {

#define SS(str, x)                                                             \
    { str, ((double)(x)), true }

#define S(x)                                                                   \
    { #x, ((double)(x)), true }

#define F(x)                                                                   \
    { x, 0, false }

    static struct {
        char *string;
        double expected;
        bool expect_success;
    } double_tests[] = {

        S(0.0),
        S(1.0),
        S(2.32),
        S(0.04),
        S(.001),
        S(1.),
        S(1e-3),
        S(1E-3),

        S(+0.0),
        S(+1.0),
        S(+2.32),
        S(+0.04),
        S(+.001),
        S(+1.),
        S(+1e-3),
        S(+1E-3),

        S(-0.0),
        S(-1.0),
        S(-2.32),
        S(-0.04),
        S(-.001),
        S(-1.),
        S(-1e-3),
        S(-1E-3),

        S(10.0),
        S(10e4),
        S(.10),
        S(10.0f),
        S(10e4f),
        S(.10f),

        S(-10.0),
        S(-10e4),
        S(-.10),
        S(-10.0f),
        S(-10e4f),
        S(-.10f),

        SS("10f", 10.0f),
        SS("-10f", -10.0f),
        SS("inf", INFINITY),
        SS("infinity", INFINITY),
        SS("-inf", -INFINITY),
        SS("-infinity", -INFINITY),
        SS("nan", NAN),
        SS("-nan", -NAN),

        F("0x"),
        F("-"),
        F("-0x"),

        F("0b"),
        F("-0b"),
        F("-0xasd"),
        F("-0bx10"),
        F("-0xffu"),
        F("-0xfful"),
        // Text string
        F("asdadsads"),

        // Computations are not allowed
        F("3 * 2"),
    };

#undef SS
#undef S
#undef F
    const float EPSILON = 0.0001;
    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(double_tests); i++) {
        const char *in = double_tests[i].string;
        double expected = double_tests[i].expected;
        double obtained;
        bool success = str_to_double(in, &obtained);
        TEST_ASSERT(success == double_tests[i].expect_success);
        if (success) {
            TEST_ASSERT_DOUBLE_WITHIN(EPSILON, expected, obtained);
        }
    }
}

static void test_parse_bool(void) {
#define SS(str, x)                                                             \
    { str, ((bool)(x)), true }

#define S(x)                                                                   \
    { #x, ((bool)(x)), true }

#define F(x)                                                                   \
    { x, 0, false }

    static struct {
        char *string;
        bool expected;
        bool expect_success;
    } bool_tests[] = {
        S(true),
        S(false),
        S(1),
        S(0),
        SS("True", true),
        SS("False", false),

        SS("TRUE", true),
        SS("FALSE", false),

        F("0x"),
        F("-"),
        F("-0x"),

        F("0b"),
        F("-0b"),
        F("-0xasd"),
        F("-0bx10"),
        F("-0xffu"),
        F("-0xfful"),
        // Text string
        F("asdadsads"),

        // Computations are not allowed
        F("3 * 2"),
    };

#undef SS
#undef S
#undef F

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(bool_tests); i++) {
        const char *in = bool_tests[i].string;
        bool expected = bool_tests[i].expected;
        bool obtained;
        bool success = str_to_bool(in, &obtained);
        TEST_ASSERT(success == bool_tests[i].expect_success);
        if (success) {
            TEST_ASSERT(expected == obtained);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_int32);
    RUN_TEST(test_parse_usize);
    RUN_TEST(test_parse_float);
    RUN_TEST(test_parse_double);
    RUN_TEST(test_parse_bool);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
