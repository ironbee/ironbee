/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

#ifndef _IB_BUILD_H_
#define _IB_BUILD_H_

/**
 * @file
 * @brief IronBee --- Build Definitions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

/**
 * @defgroup IronBeeBuild Build
 * @ingroup IronBee
 *
 * Cross-platform build support.
 *
 * @{
 */

/* Definitions for exporting symbols */
#if (defined(_WIN32) || defined(__CYGWIN__))
  #ifdef BUILDING_DLL
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__((dllexport))
    #else
      #define DLL_PUBLIC __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__((dllimport))
    #else
      #define DLL_PUBLIC __declspec(dllimport)
    #endif
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define DLL_PUBLIC __attribute__ ((visibility("default")))
    #define DLL_LOCAL  __attribute__ ((visibility("hidden")))
  #else
    #define DLL_PUBLIC
    #define DLL_LOCAL
  #endif
#endif

/* For GNU C, tell the compiler to check printf like formatters */
#ifndef PRINTF_ATTRIBUTE
#if (defined(__GNUC__) && !defined(SOLARIS2))
#define PRINTF_ATTRIBUTE(a, b) __attribute__((__format__ (printf, a, b)))
#define VPRINTF_ATTRIBUTE(a) __attribute__((__format__ (printf, a, 0)))
#else
#define PRINTF_ATTRIBUTE(a, b)
#define VPRINTF_ATTRIBUTE(a)
#endif
#endif

/* Have compiler check for null pointer. */
#ifdef __clang__
#define ALL_NONNULL_ATTRIBUTE __attribute__((nonnull))
#define NONNULL_ATTRIBUTE(...) __attribute__((nonnull (__VA_ARGS__)))
#else
#define ALL_NONNULL_ATTRIBUTE
#define NONNULL_ATTRIBUTE(...)
#endif

/* Mark a function as deprecated, with a message. */
#if defined(__clang__)
#define IB_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif (defined(__GNUC__) && !defined(SOLARIS2))
#define IB_DEPRECATED(msg) __attribute__((deprecated))
#elif defined(_MSC_VER)
#define IB_DEPRECATED(msg) __declspec(deprecated)
#else
#pragma message("WARNING: IB_DEPRECATED(msg) is not implemented for this compiler!")
#define IB_DEPRECATED(msg)
#endif

/**
 * @} IronBeeBuild
 */

#endif /* _IB_BUILD_H_ */
