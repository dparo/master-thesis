#include "stub.h"

static Solution solve(ATTRIB_MAYBE_UNUSED Solver *self,
                      ATTRIB_MAYBE_UNUSED const Instance *instance) {
    Solution result = {};
    return result;
}

static void destroy(ATTRIB_MAYBE_UNUSED Solver *self) {}

Solver stub_solver_create(ATTRIB_MAYBE_UNUSED const Instance *instance) {
    Solver solver = {0};
    solver.solve = solve;
    solver.destroy = destroy;
    return solver;
}
