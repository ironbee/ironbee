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

#ifndef _IB_UUID_H_
#define _IB_UUID_H_

/**
 * @file
 * @brief IronBee --- UUID Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilUUID UUID
 * @ingroup IronBeeUtil
 *
 * Code to generate, convert, and manipulate UUIDs.
 *
 * @{
 */

/**
 * Universal Unique ID Structure.
 *
 * This is a UUID.  UUIDs are generated via version 4 (random).
 */
typedef union {
    uint8_t  byte[16];
    uint16_t uint16[8];
    uint32_t uint32[4];
    uint64_t uint64[2];
} ib_uuid_t;

/**
 * Initialize UUID library.
 *
 * ib_util_initialize() will call this.
 */
ib_status_t DLL_PUBLIC ib_uuid_initialize(void);

/**
 * Shutdown UUID library.
 *
 * ib_util_shutdown() will call this.
 */
ib_status_t DLL_PUBLIC ib_uuid_shutdown(void);

/**
 * Parses an ASCII UUID (with the format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 * where x are hex chars) into a ib_uuid_t
 *
 * @param ibuuid pointer to an already allocated ib_uuid_t buffer
 * @param uuid pointer to the ascii string of the uuid (no blank spaces allowed)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_uuid_ascii_to_bin(
    ib_uuid_t  *ibuuid,
    const char *uuid
);

/**
 * Outputs a UUID to a string.
 *
 * @param str Pointer to already allocated buffer to hold string (37 bytes)
 * @param uuid UUID to write to @a str.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_uuid_bin_to_ascii(
    char            *str,
    const ib_uuid_t *uuid
);

/**
 * Creates a new, random, v4 uuid.
 *
 * @param uuid Pointer to allocated ib_uuid_t to store result in.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_uuid_create_v4(ib_uuid_t *uuid);

/**
 * Creates a new, random, v4 uuid string.
 *
 * @param str Pre-allocated buffer to hold string (37 bytes)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_uuid_create_v4_str(char *str);

/**
 * Creates a new, sha1, v5 uuid.
 *
 * @warning This routine does not use the caching that the other routines
 * do.  As such, it has a significant runtime cost and is not suitable for
 * frequent calling.  Use ib_uuid_create_v4() for cheap UUID creation.
 *
 * @sa ib_uuid_create_v4()
 *
 * @param[out] uuid_str The 37 byte UUID is written to this buffer.
 *             If this is NULL then it will be malloced by ossp-uuid.
 * @param[in,out] uuid_str_len This should be 37. If, for some reason,
 *                that is not long enough (36 bytes and one \0 character)
 *                then an out-of-memory error is reported by ossp-uuid
 *                and IB_EOTHER is returned.
 *                If @a uuid_str is NULL, then this is ignored
 *                and is used as an output variable for the
 *                length of @a uuid_str allocated by malloc.
 * @param[in] key The null-terminated string of the key we want to hash.
 *
 * @returns
 *   - IB_OK
 *   - IB_EOTHER if exporting the UUID or some other unexpected error occurs.
 *   - IB_EALLOC if a uuid object cannot be made.
 */
ib_status_t DLL_PUBLIC ib_uuid_create_v5_str(
    char       **uuid_str,
    size_t      *uuid_str_len,
    const char  *key
);

/** @} IronBeeUtilUUID */


#ifdef __cplusplus
}
#endif

#endif /* _IB_UUID_H_ */
