#include "mip.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef COMPILED_WITH_CPLEX
Solver mip_solver_create(ATTRIB_MAYBE_UNUSED Instance *instance) {
    fprintf(stderr,
            "%s: Cannot use mip solver as the program was not compiled with "
            "CPLEX\n",
            __FILE__);
    fflush(stderr);
    abort();
    return (Solver){};
}
#else

Solver mip_solver_create(ATTRIB_MAYBE_UNUSED Instance *instance) {
    // TODO: Implement me
    return (Solver){};
}

#endif
