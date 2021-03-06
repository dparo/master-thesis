/*
 * Copyright (c) 2022 Davide Paro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "hashing.h"
#include "core-utils.h"

#define SHA256_UPDATE_WITH_VAR(shactx, var)                                    \
    do {                                                                       \
        sha256_update((shactx), (const BYTE *)(&(var)), sizeof(var));          \
    } while (0)

#define SHA256_UPDATE_WITH_ARRAY(shactx, array, num_elems)                     \
    do {                                                                       \
        sha256_update((shactx), (const BYTE *)(array),                         \
                      (num_elems) * sizeof(*(array)));                         \
    } while (0)

Hash hash_instance(const Instance *instance) {
    SHA256_CTX shactx;
    sha256_init(&shactx);

    SHA256_UPDATE_WITH_VAR(&shactx, instance->num_customers);
    SHA256_UPDATE_WITH_VAR(&shactx, instance->num_vehicles);
    SHA256_UPDATE_WITH_VAR(&shactx, instance->vehicle_cap);

    int32_t n = instance->num_customers + 1;

    if (instance->positions) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->positions, n);
    }

    if (instance->demands) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->demands, n);
    }

    if (instance->demands) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->demands, n);
    }

    if (instance->profits) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->profits, n);
    }

    if (instance->edge_weight) {
        SHA256_UPDATE_WITH_ARRAY(&shactx, instance->edge_weight,
                                 hm_nentries(n));
    }

    Hash result = {0};
    sha256_finalize_to_cstr(&shactx, &result);
    return result;
}

void sha256_hash_file_contents(const char *fpath, Hash *hash) {

    SHA256_CTX shactx;
    sha256_init(&shactx);
    size_t len = 0;
    char *contents = fread_all_into_cstr(fpath, &len);
    if (contents) {
        sha256_update(&shactx, (const BYTE *)contents, len);
    } else {
        log_fatal("%s: Failed to hash (sha256) file contents\n", fpath);
        abort();
    }
    sha256_finalize_to_cstr(&shactx, hash);
    free(contents);
}

Hash compute_run_hash(const Hash *exe_hash, const PerfProfInput *input,
                      char *args[PROC_MAX_ARGS], int32_t num_args) {
    assert(input);

    SHA256_CTX shactx;
    sha256_init(&shactx);

    for (int32_t i = 0; i < num_args; i++) {
        sha256_update(&shactx, (const BYTE *)(&args[i][0]), strlen(args[i]));
    }

    if (exe_hash) {
        sha256_update(&shactx, (const BYTE *)(&exe_hash->cstr[0]), 64);
    }

    sha256_update(&shactx, (const BYTE *)(&input->uid.seedidx),
                  sizeof(input->uid.seedidx));
    sha256_update(&shactx, (const BYTE *)(&input->uid.hash.cstr[0]), 64);

    Hash result = {0};
    sha256_finalize_to_cstr(&shactx, &result);
    return result;
}
