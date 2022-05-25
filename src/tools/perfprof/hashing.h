#pragma once

#if __cplusplus
extern "C" {
#endif

#include <sha256.h>
#include <string.h>

#include "types.h"
#include "utils.h"
#include "misc.h"
#include "common.h"
#include "core.h"

Hash hash_instance(const Instance *instance);
void sha256_hash_file_contents(const char *fpath, Hash *hash);
Hash compute_run_hash(const Hash *exe_hash, const PerfProfInput *input,
                      char *args[PROC_MAX_ARGS], int32_t num_args);

static void sha256_finalize_to_cstr(SHA256_CTX *shactx, Hash *hash) {

    BYTE bytes[32];

    sha256_final(shactx, bytes);

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(bytes); i++) {
        snprintf_safe(hash->cstr + 2 * i, 65 - 2 * i, "%02x", bytes[i]);
    }

    hash->cstr[ARRAY_LEN(hash->cstr) - 1] = 0;
}

#if __cplusplus
}
#endif
