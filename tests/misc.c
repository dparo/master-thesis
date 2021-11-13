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
#include "types.h"

TEST test_example(void) {
    ASSERT_EQ(1 + 2, 3);
    PASS();
}

TEST calling_fread(void) {
    FILE *f = fopen("./CMakeLists.txt", "r");
    char buffer[16 * 1024];
    size_t sizeRead = fread(buffer, 1, ARRAY_LEN(buffer), f);
    ASSERT_GTE(sizeRead, 64);
    fclose(f);
    PASS();
}

TEST calling_calloc_0_0(void) {
    char *p = calloc(0, 0);
    ASSERT(p);
    free(p);
    PASS();
}

TEST calling_malloc_0(void) {
    char *p = malloc(0);
    ASSERT(p);
    free(p);
    PASS();
}

TEST calling_enum_lookup(void) {

    typedef enum DummyEnum {
        RED = -1,
        GREEN = 0xff2,
    } DummyEnum;

    static ENUM_TO_STR_TABLE_DECL(DummyEnum) = {
        ENUM_TO_STR_TABLE_FIELD(RED),
        ENUM_TO_STR_TABLE_FIELD(GREEN),
    };

    ASSERT(0 == strcmp("RED", ENUM_TO_STR(DummyEnum, RED)));
    ASSERT(0 == strcmp("GREEN", ENUM_TO_STR(DummyEnum, GREEN)));

    ASSERT(RED == *STR_TO_ENUM(DummyEnum, "RED"));
    ASSERT(GREEN == *STR_TO_ENUM(DummyEnum, "GREEN"));

    ASSERT(!STR_TO_ENUM(DummyEnum, "NON_EXISTENT"));
    ASSERT(RED == STR_TO_ENUM_DEFAULT(DummyEnum, "NON_EXISTENT", RED));
    PASS();
}

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN(); /* command-line arguments, initialization. */

    /* If tests are run outside of a suite, a default suite is used. */
    RUN_TEST(test_example);
    RUN_TEST(calling_fread);
    RUN_TEST(calling_calloc_0_0);
    RUN_TEST(calling_malloc_0);
    RUN_TEST(calling_enum_lookup);

    GREATEST_MAIN_END(); /* display results */
}
