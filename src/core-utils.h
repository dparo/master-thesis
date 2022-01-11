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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include "core.h"
#include <math.h>

bool solver_params_contains(SolverTypedParams *params, char *key);
bool solver_params_get_bool(SolverTypedParams *params, char *key);
int32_t solver_params_get_int32(SolverTypedParams *params, char *key);
double solver_params_get_double(SolverTypedParams *params, char *key);

static inline int64_t hm_nentries(int32_t n) { return ((n * n) - n) / 2; }

// Number of entries in a full matrix of size `N x N`
// Eg N**2 - num_entries(diagonal)
static inline int64_t fm_nentries(int32_t n) { return (n * n) - n; }

static inline int64_t sxpos(int32_t n, int32_t i, int32_t j) {
    assert(i != j);

    int32_t l = MIN(i, j);
    int32_t u = MAX(i, j);

    int64_t result = l * n + u - ((l + 1) * (l + 2)) / 2;
    return result;
}

static inline int64_t asxpos(int32_t n, int32_t i, int32_t j) {
    if (i <= j)
        return sxpos(n, i, j);
    else
        return hm_nentries(n) + sxpos(n, j, i);
}

static inline double cptp_dist(const Instance *instance, int32_t i, int32_t j) {
    assert(i >= 0 && i < instance->num_customers + 1);
    assert(j >= 0 && j < instance->num_customers + 1);

    if (instance->edge_weight) {
        return instance->edge_weight[sxpos(instance->num_customers + 1, i, j)];
    } else {
        double distance =
            vec2d_dist(&instance->positions[i], &instance->positions[j]);

        switch (instance->rounding_strat) {
        case CPTP_DIST_ROUND: /// Default
            return round(distance);
        case CPTP_DIST_NO_ROUND:
            return distance;
        case CPTP_DIST_CEIL:
            return ceil(distance);
        case CPTP_DIST_FLOOR:
            return floor(distance);
        default:
            assert(!"Invalid code path!");
            return INFINITY;
        }
    }
    return INFINITY;
}

static inline double cptp_reduced_cost_arc_val(const Instance *instance,
                                               int32_t i, int32_t j) {
    double dist = cptp_dist(instance, i, j);
    double pi = instance->profits[i];
    double pj = instance->profits[j];
    return dist - (pi + pj) / 2.0;
}

static inline int32_t *tsucc(Tour *tour, int32_t customer_idx) {
    return veci32_access(tour->succ, customer_idx, tour->num_customers + 1);
}

static inline int32_t *tcomp(Tour *tour, int32_t customer_idx) {
    return veci32_access(tour->comp, customer_idx, tour->num_customers + 1);
}

static inline double tour_eval(const Instance *instance, Tour *tour) {
    if (tour->num_comps != 1) {
        return INFINITY;
    }

    for (int32_t i = 0; i < tour->num_customers + 1; i++) {
        int32_t succ = *tsucc(tour, i);
        if (succ >= 0) {
            if (*tcomp(tour, i) != 0 || succ >= tour->num_customers + 1) {
                return INFINITY;
            }
        }
    }

    double cost = 0.0;
    double profit = 0.0;
    double demands = 0.0;

    int32_t curr_vertex = 0;
    int32_t next_vertex = -1;

    profit += instance->profits[0];
    demands += instance->demands[0];

    while ((next_vertex = *tsucc(tour, curr_vertex)) != 0) {
        assert(next_vertex != curr_vertex);
        cost += cptp_dist(instance, curr_vertex, next_vertex);
        profit += instance->profits[next_vertex];
        demands += instance->demands[next_vertex];
        curr_vertex = next_vertex;
    }

    cost += cptp_dist(instance, curr_vertex, 0);

    if (demands > instance->vehicle_cap) {
        return INFINITY;
    } else {
        return cost - profit;
    }
}

static inline double solution_relgap(Solution *solution) {
    // Taken from:
    // https://www.ibm.com/docs/en/icos/12.10.0?topic=g-cpxxgetmiprelgap-cpxgetmiprelgap

    double ub = solution->upper_bound;
    double lb = solution->lower_bound;
    return (ub - lb) / (1e-10 + fabs(ub));
}

static inline void solver_params_push(SolverParams *params, char *name,
                                      char *value) {
    assert(params->num_params < MAX_NUM_SOLVER_PARAMS);
    params->params[params->num_params].name = name;
    params->params[params->num_params].value = value;
    params->num_params++;
}

#if __cplusplus
}
#endif
