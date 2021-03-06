/*
 * Copyright (c) 2021 Davide Paro
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

extern const char *PROJECT_NAME;
extern const char *PROJECT_DESCRIPTION;
extern const char *PROJECT_VERSION;
extern const char *PROJECT_VERSION_MAJOR;
extern const char *PROJECT_VERSION_MINOR;
extern const char *PROJECT_VERSION_PATCH;

extern const char *BUILD_TYPE;
extern const char *C_COMPILER_ID;
extern const char *C_COMPILER_ABI;
extern const char *C_COMPILER_VERSION;
extern const char *GIT_SHA1;
extern const char *GIT_DATE;
extern const char *GIT_COMMIT_SUBJECT;

#if __cplusplus
}
#endif
