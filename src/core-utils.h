#pragma once

#if __cplusplus
extern "C" {
#endif

#include "core.h"

static inline int32_t *tour_succ(Tour *tour, int32_t vehicle_idx,
                                 int32_t customer_idx) {
    return mati32_access(tour->succ, vehicle_idx, customer_idx,
                         tour->num_customers + 1, tour->num_vehicles);
}

static inline int32_t *tour_comp(Tour *tour, int32_t vehicle_idx,
                                 int32_t customer_idx) {

    return mati32_access(tour->comp, vehicle_idx, customer_idx,
                         tour->num_customers + 1, tour->num_vehicles);
}

static inline bool tour_is_customer_served_from_vehicle(Tour *tour,
                                                        int32_t vehicle_idx,
                                                        int32_t customer_idx) {
    return *tour_comp(tour, vehicle_idx, customer_idx) != -1;
}

static inline bool
tour_is_customer_served_from_any_vehicle(Tour *tour, int32_t customer_idx) {
    for (int32_t i = 0; i < tour->num_vehicles; i++) {
        if (tour_is_customer_served_from_vehicle(tour, i, customer_idx)) {
            return true;
        }
    }
    return false;
}

static inline bool tour_are_all_customers_served(Tour *tour) {
    for (int32_t i = 1; i < tour->num_customers + 1; i++) {
        if (tour_is_customer_served_from_any_vehicle(tour, i)) {
            return true;
        }
    }
    return false;
}

#if __cplusplus
}
#endif
