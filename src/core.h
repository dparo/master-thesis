#pragma once

#if __cplusplus
extern "C" {
#endif

#include "types.h"

typedef struct Instance {
    int32_t num_customers;
    int32_t num_vehicles;
    double vehicle_cap;

    struct {
        Vec2d *positions;
        double *demands;
        double *duals;
    };
} Instance;

void instance_destroy(Instance *instance);

#if __cplusplus
}
#endif
