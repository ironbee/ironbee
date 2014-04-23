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

#ifndef _IB_DSO_H_
#define _IB_DSO_H_

/**
 * @file
 * @brief IronBee --- DSO Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilDso Dynamic Shared Object (DSO)
 * @ingroup IronBeeUtil
 *
 * Code to load and interact with DSOs.  Used for module loading.
 *
 * @{
 */

/**
 * A dso file.
 */
typedef struct ib_dso_t ib_dso_t;

/**
 * Generic type for a DSO symbol.
 *
 * @sa ib_dso_sym_find().
 */
typedef void ib_dso_sym_t;

/**
 * Open a dynamic shared object (DSO) from a file.
 *
 * @param[out] pdso DSO handle is stored in @a *dso.
 * @param[in]  file DSO filename.
 * @param[in]  mm Memory manager to use.
 *
 * @returns
 * - IB_EINVAL -- Unable to open DSO file.
 * - IB_EALLOC -- Allocation error.
 * - IB_OK     -- Success.
 */
ib_status_t DLL_PUBLIC ib_dso_open(
    ib_dso_t   **pdso,
    const char  *file,
    ib_mm_t      mm
)
NONNULL_ATTRIBUTE(1,2);


/**
 * Close a dynamic shared object (DSO).
 *
 * @param[in] dso DSO to close.
 *
 * @returns
 * - IB_EINVAL   -- @a dso is null.
 * - IB_EUNKNOWN -- Failed to close DSO.
 * - IB_OK       -- Success.
 */
ib_status_t DLL_PUBLIC ib_dso_close(
    ib_dso_t *dso
)
ALL_NONNULL_ATTRIBUTE;


/**
 * Find a given symbol in a dynamic shared object (DSO).
 *
 * @param[out] psym DSO symbol handle is stored in @a *sym.
 * @param[in]  dso  DSO to search in.
 * @param[in]  name DSO symbol name.
 *
 * @returns
 * - IB_EINVAL -- dso or psym is null.
 * - IB_ENOENT -- No symbol in @a dso named @a name.
 * - IB_OK     -- Success.
 */
ib_status_t DLL_PUBLIC ib_dso_sym_find(
    ib_dso_sym_t **psym,
    ib_dso_t      *dso,
    const char    *name
)
ALL_NONNULL_ATTRIBUTE;

/**
 * Given @a addr, look up the symbol name and file name of the dynamic library.
 *
 * @param[out] fname File name of the dynamic library.
 * @param[out] sname The name of the symbol. If the given address does not
 *             point to a symbol, the closest symbol with an address less than
 *             @a addr is returned.
 * @param[in] mm Copy the file name and symbol name assigned to
 *            @a fname and @a sname.
 * @parma[in] addr The address to look up a symbol for.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - IB_EOTHER On system call failure.
 */
ib_status_t DLL_PUBLIC ib_dso_sym_name_find(
    const char **fname,
    const char **sname,
    ib_mm_t      mm,
    void        *addr
)
NONNULL_ATTRIBUTE(1, 2, 4);

/** @} IronBeeUtilDso */

#ifdef __cplusplus
}
#endif

#endif /* _IB_DSO_H_ */
