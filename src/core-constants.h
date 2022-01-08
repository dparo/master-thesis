#pragma once

#if __cplusplus
extern "C" {
#endif

#define INT32_DEAD_VAL (INT32_MIN >> 1)

// See BapCodReducedCostTolerance, ColGenMipSolverReducedCostTolerance,
// RCSP_PRECISION from BapCod
#define COST_TOLERANCE ((double)1e-6)

#if __cplusplus
}
#endif
