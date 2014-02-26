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

#ifndef _IB_STRING_ASSEMBLY_H_
#define _IB_STRING_ASSEMBLY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ironbee/mm.h>
#include <ironbee/release.h>

/**
 * @file
 * @brief IronBee --- String Assembly
 *
 * @author Christopher Alfeld <calfeld@calfeld.net>
 */

/**
 * @defgroup IronBeeUtilStringAssembly String Assembly
 * @ingroup IronBeeUtil
 *
 * Routines to assemble strings.
 *
 * Begin assembly via ib_sa_begin(), append via ib_sa_append(), and then
 * convert to a string buffer via ib_sa_finish().
 *
 * @{
 */

/**
 * String assembly state.
 **/
typedef struct ib_sa_t ib_sa_t;

/**
 * Begin string assembly.
 *
 * @param[out] sa        String assembly state.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_sa_begin(
    ib_sa_t **sa
)
NONNULL_ATTRIBUTE(1);

/**
 * Append data to a string under assembly.
 *
 * @param[in] sa          String assembly state.
 * @param[in] data        Data to append.  Lifetime must be until
 *                        ib_sa_finish() but does not need to be longer.
 * @param[in] data_length Length of @a data.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_sa_append(
    ib_sa_t    *sa,
    const char *data,
    size_t      data_length
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Complete assembly, converting to string.
 *
 * Once called, any further use of the assembly state is undefined.  To
 * reflect this, `*sa` will be set to NULL.
 *
 * @param[in, out] sa         String assembly state.  Will be set to NULL on success.
 * @param[out]     dst        Where to store assembled string.  Lifetime will
 *                            equal that of @a mp.
 * @param[out]     dst_length Length of @a dst.
 * @param[in]      mm         Memory manager to allocate @a dst from.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_sa_finish(
    ib_sa_t    **sa,
    const char **dst,
    size_t      *dst_length,
    ib_mm_t      mm
)
NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Abort assembly.
 *
 *
 * Once called, any further use of the assembly state is undefined.  To
 * reflect this, `*sa` will be set to NULL.
 *
 * @param[in, out] sa String assembly state.  Will be set to NULL.
 **/
void DLL_PUBLIC ib_sa_abort(ib_sa_t **sa);


/** @} */

/**
 * @} IronBeeUtilStringAssembly
 */

#ifdef __cplusplus
}
#endif

#endif
