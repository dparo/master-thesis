#pragma once

#if __cplusplus
extern "C" {
#endif

#include "mip.h"

bool mip_ins_heur_warm_start(Solver *solver, const Instance *instance);

#if __cplusplus
}
#endif
