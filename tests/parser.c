#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "parser.h"

static void test_parser(void) {
    const char *filepath = "res/ESPPRC - Test Instances/E-n101-k14_a.vrp";
    Instance instance = parse(filepath);
    TEST_ASSERT_EQUAL(instance.num_customers, 100);
    TEST_ASSERT_EQUAL(instance.num_vehicles, 14);
    instance_destroy(&instance);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser);
    return UNITY_END();
}

/// Ran before each test
void setUp(void) {}

/// Ran after each test
void tearDown(void) {}
