#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unity.h>

static void test_example(void) { TEST_ASSERT_EQUAL(1 + 2, 3); }

static void test_read(void) {
    FILE *f = fopen("./CMakeLists.txt", "r");
    char buffer[16 * 1024];
    size_t sizeRead = fread(buffer, 1, sizeof(buffer), f);
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    RUN_TEST(test_read);
    RUN_TEST(test_calloc_0_0);
    RUN_TEST(test_malloc_0);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
