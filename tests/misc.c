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

static void test_example(void) { TEST_ASSERT_EQUAL(1 + 2, 3); }

static void test_read(void) {
    FILE *f = fopen("./CMakeLists.txt", "r");
    char buffer[16 * 1024];
    size_t sizeRead = fread(buffer, 1, ARRAY_LEN(buffer), f);
    TEST_ASSERT_GREATER_OR_EQUAL(64, sizeRead);
    fclose(f);
}

static void test_calloc_0_0(void) {
    char *p = calloc(0, 0);
    TEST_ASSERT(p);
    free(p);
}

static void test_malloc_0(void) {
    char *p = malloc(0);
    TEST_ASSERT(p);
    free(p);
}

static void test_enum_lookup(void) {

    typedef enum DummyEnum {
        RED = -1,
        GREEN = 0xff2,
    } DummyEnum;

    static ENUM_TO_STR_TABLE_DECL(DummyEnum) = {
        ENUM_TO_STR_TABLE_FIELD(RED),
        ENUM_TO_STR_TABLE_FIELD(GREEN),
    };

    TEST_ASSERT(0 == strcmp("RED", ENUM_TO_STR(DummyEnum, RED)));
    TEST_ASSERT(0 == strcmp("GREEN", ENUM_TO_STR(DummyEnum, GREEN)));

    TEST_ASSERT(RED == *STR_TO_ENUM(DummyEnum, "RED"));
    TEST_ASSERT(GREEN == *STR_TO_ENUM(DummyEnum, "GREEN"));

    TEST_ASSERT_NULL(STR_TO_ENUM(DummyEnum, "NON_EXISTENT"));
    TEST_ASSERT(RED == STR_TO_ENUM_DEFAULT(DummyEnum, "NON_EXISTENT", RED));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    RUN_TEST(test_read);
    RUN_TEST(test_calloc_0_0);
    RUN_TEST(test_malloc_0);
    RUN_TEST(test_enum_lookup);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
