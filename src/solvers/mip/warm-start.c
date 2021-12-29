#include "warm-start.h"
#include "validation.h"

typedef struct InsHeurNodePair {
    int32_t u, v;
} InsHeurNodePair;

bool ins_heur_find_starting_pair(const Instance *instance,
                                 InsHeurNodePair *start_pair) {
    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    int32_t s = -1;
    int32_t e = -1;

    // Start is always the depot node
    s = 0;
    if (instance->demands[s] > Q) {
        return false;
    }

    const double rel_Q = Q - instance->demands[s];

    bool any_e = false;
    for (int32_t i = 1; i < n; i++) {
        if (instance->demands[i] <= rel_Q) {
            any_e = true;
            break;
        }
    }

    if (!any_e) {
        return false;
    }

    e = -1;
    do {
        e = rand() % n;
    } while (e == s || instance->demands[e] > rel_Q);

    start_pair->u = s;
    start_pair->v = e;
    return true;
}

bool mip_ins_heur_warm_start(Solver *solver, const Instance *instance) {
    bool result = true;
    const int32_t n = instance->num_customers + 1;
    const double Q = instance->vehicle_cap;

    Solution solution = solution_create(instance);
    Tour *const tour = &solution.tour;
    tour->num_comps = 1;

    double cost = 0.0;
    double sum_demands = 0.0;

    InsHeurNodePair starting_pair;
    if (!ins_heur_find_starting_pair(instance, &starting_pair)) {
        log_fatal("%s :: Unable to find starting pair", __func__);
        goto terminate;
    }

    const int32_t start = starting_pair.u;
    const int32_t end = starting_pair.v;
    fprintf(stderr, "start = %d, end = %d, n = %d\n", start, end, n);

    assert(start >= 0 && start < n);
    assert(end >= 0 && end < n);
    assert(start != end);

    tour->comp[start] = 0;
    tour->comp[end] = 0;

    cost += cptp_dist(instance, start, end);
    cost += cptp_dist(instance, end, start);
    sum_demands += instance->demands[start];
    sum_demands += instance->demands[end];

    while (true) {
        break;
    }

    solution.upper_bound = cost - sum_demands;
    validate_solution(instance, &solution);

    // Convert the solution to something cplex can use

terminate:
    solution_destroy(&solution);
    return result;
}
