#pragma once

#if __cplusplus
extern "C" {
#endif

#include "types.h"

static struct {
    char *filepath;
    int32_t expected_num_customers;
    int32_t expected_num_vehicles;
} const G_TEST_INSTANCES[] = {
    {"res/ESPPRC - Test Instances/E-n76-k7_a.vrp", 75, 7},
    {"res/ESPPRC - Test Instances/E-n76-k7_b.vrp", 75, 7},
    {"res/ESPPRC - Test Instances/E-n76-k8_a.vrp", 75, 8},
    {"res/ESPPRC - Test Instances/E-n76-k8_b.vrp", 75, 8},
    {"res/ESPPRC - Test Instances/E-n76-k10_a.vrp", 75, 10},
    {"res/ESPPRC - Test Instances/E-n76-k10_b.vrp", 75, 10},
    {"res/ESPPRC - Test Instances/E-n76-k14_a.vrp", 75, 14},
    {"res/ESPPRC - Test Instances/E-n76-k14_b.vrp", 75, 14},
    {"res/ESPPRC - Test Instances/E-n101-k8_a.vrp", 100, 8},
    {"res/ESPPRC - Test Instances/E-n101-k8_b.vrp", 100, 8},
    {"res/ESPPRC - Test Instances/E-n101-k14_a.vrp", 100, 14},
    {"res/ESPPRC - Test Instances/E-n101-k14_b.vrp", 100, 14},
    {"res/ESPPRC - Test Instances/F-n45-k4_a.vrp", 44, 4},
    {"res/ESPPRC - Test Instances/F-n72-k4_a.vrp", 71, 4},
    {"res/ESPPRC - Test Instances/F-n135-k7_a.vrp", 134, 7},
    {"res/ESPPRC - Test Instances/M-n121-k7_a.vrp", 120, 7},
    {"res/ESPPRC - Test Instances/M-n121-k7_b.vrp", 120, 7},
    {"res/ESPPRC - Test Instances/M-n151-k12_a.vrp", 150, 12},
    {"res/ESPPRC - Test Instances/M-n151-k12_b.vrp", 150, 12},
    {"res/ESPPRC - Test Instances/M-n200-k16_a.vrp", 199, 16},
    {"res/ESPPRC - Test Instances/M-n200-k16_b.vrp", 199, 16},
    {"res/ESPPRC - Test Instances/M-n200-k17_a.vrp", 199, 17},
    {"res/ESPPRC - Test Instances/M-n200-k17_b.vrp", 199, 17},
    {"res/ESPPRC - Test Instances/P-n70-k10_a.vrp", 69, 10},
    {"res/ESPPRC - Test Instances/P-n70-k10_b.vrp", 69, 10},
    {"res/ESPPRC - Test Instances/P-n76-k4_a.vrp", 75, 4},
    {"res/ESPPRC - Test Instances/P-n76-k4_b.vrp", 75, 4},
    {"res/ESPPRC - Test Instances/P-n76-k5_a.vrp", 75, 5},
    {"res/ESPPRC - Test Instances/P-n76-k5_b.vrp", 75, 5},
    {"res/ESPPRC - Test Instances/P-n101-k4_a.vrp", 100, 4},
    {"res/ESPPRC - Test Instances/P-n101-k4_b.vrp", 100, 4},
};

#if __cplusplus
}
#endif
