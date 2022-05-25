#pragma once

#if __cplusplus
extern "C" {
#endif

#include "common.h"
#include <cJSON.h>

cJSON *load_json(char *filepath);
void parse_cptp_solver_json_dump(PerfProfRun *run, cJSON *root);
void parse_bapcod_solver_json_dump(PerfProfRun *run, cJSON *root);

#if __cplusplus
}
#endif
