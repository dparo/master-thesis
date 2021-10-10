#pragma once

#if __cplusplus
extern "C" {
#endif

#include "core.h"

Solver mip_solver_create(Instance *instance);

#if __cplusplus
}
#endif
