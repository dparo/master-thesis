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

#define STD_C11_VERSION 201112L

#if __GNUC__ || __clang__
#define THREAD_LOCAL __thread
#elif _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#endif

#define UNUSED_PARAM(x) (void)(x)

#ifdef __GNUC__
#define ATTRIB_NORETURN __attribute__((noreturn))
#define ATTRIB_ALWAYS_INLINE __attribute__((always_inline))
#define ATTRIB_CONSTRUCT(func) __attribute__((constructor))
#define ATTRIB_DESTRUCT(func) __attribute__((destructor))
#define ATTRIB_DEPRECATED __attribute__((deprecated))
#define ATTRIB_PURE __attribute__((pure))
#define ATTRIB_CONST __attribute__((const))
#define ATTRIB_WEAK __attribute__((weak))
#define ATTRIB_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define ATTRIB_MAYBE_UNUSED __attribute__((unused))
#define ATTRIB_NODISCARD __attribute__((warn_unused_result))
#define ATTRIB_PRINTF(STRING_INDEX, FIRST_TO_CHECK)                            \
    __attribute__((format(printf, (STRING_INDEX), (FIRST_TO_CHECK))))

#elif defined _MSC_VER
#if __cplusplus > 201703L
#define ATTRIB_NORETURN [[noreturn]]
#define ATTRIB_MAYBE_UNUSED [[maybe_unused]]
#define ATTRIB_NODISCARD [[nodiscard]]
#endif
#define ATTRIB_NORETURN __declspec(noreturn)
#define ATTRIB_MAYBE_UNUSED
#define ATTRIB_NODISCARD _Check_return_
#endif

/* ===================================
   Static Assertion
   =================================== */
#ifndef STATIC_ASSERT
#if __cplusplus > 201703L
#define STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#if __STDC_VERSION__ >= STD_C11_VERSION
#define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
#define STATIC_ASSERT(cond, msg)                                               \
    typedef char CONCAT(__dpcrtasrt__, __COUNTER__)[!(cond) ? -1 : 0]
#endif
#endif
#endif

#if defined __GNUC__ || defined __GNUG__ || defined __clang__
#define ARRAY_LEN(arr)                                                         \
    (sizeof(arr) / sizeof((arr)[0]) +                                          \
     sizeof(typeof(int[1 - 2 * !!__builtin_types_compatible_p(                 \
                                   typeof(arr), typeof(&(arr)[0]))])) *        \
         0)
#else
#define ARRAY_LEN(A)                                                           \
    ((sizeof(A) / sizeof((A)[0])) /                                            \
     ((size_t) !(                                                              \
         sizeof(A) %                                                           \
         sizeof((A)[0])))) /* Make sure that the sizeof(A) is a multiple of    \
                              sizeof(A[0]), if this does not hold divide by    \
                              zero to trigger a warning */
#endif

#define ARRAY_LEN_i32(arr) ((int32_t)ARRAY_LEN(arr))

/* Compute the length of a c-string literal known at compile time */
#define STRLIT_LEN(S) (ARRAY_LEN(S) - 1)

/**************** offsetof ************/
#ifndef offsetof
#if __GNUC__ || __clang__
#define offsetof(type, member) __builtin_offsetof(type, member)
#else
#define offsetof(type, member) ((size_t) & (((type *)0)->member))
#endif
#endif

#define todo(...) assert(!"!!!TODO!!!")
#define todo_msg(msg) assert(!(msg))

#include <debugbreak.h>

#if __cplusplus
}
#endif
