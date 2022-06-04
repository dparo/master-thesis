/*
 * Copyright (c) 2022 Davide Paro
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

    for (int32_t i = 0; i < ARRAY_LEN_i32(bytes); i++) {
        snprintf_safe(hash->cstr + 2 * i, 65 - 2 * i, "%02x", bytes[i]);
    }

    hash->cstr[ARRAY_LEN(hash->cstr) - 1] = 0;
}

#if __cplusplus
}
#endif
