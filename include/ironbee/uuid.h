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
 *****************************************************************************/

#ifndef _IB_UUID_H_
#define _IB_UUID_H_

/**
 * @file
 * @brief IronBee - UUID Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilUUID UUID Utility
 * @ingroup IronBeeUtil
 * @{
 */

/**
 * @internal
 * Universal Unique ID Structure.
 *
 * This is a modified UUIDv1 (RFC-4122) that uses fields as follows:
 *
 * time_low: 32-bit second accuracy time that tx started
 * time_mid: 16-bit counter
 * time_hi_and_ver: 4-bit version (0100) + 12-bit least sig usec
 * clk_seq_hi_res: 2-bit reserved (10) + 6-bit reserved (111111)
 * clk_seq_low: 8-bit reserved (11111111)
 * node(0-1): 16-bit process ID
 * node(2-5): 32-bit ID (system default IPv4 address by default)
 *
 * This is loosely based of of Apache mod_unique_id, but with
 * future expansion in mind.
 */
typedef struct ib_uuid_t ib_uuid_t;
struct ib_uuid_t {
    uint32_t  time_low;
    uint16_t  time_mid;
    uint16_t  time_hi_and_ver;
    uint8_t   clk_seq_hi_res;
    uint8_t   clk_seq_low;
    uint8_t   node[6];
};

/**
 * Parses an ASCII UUID (with the format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 * where x are hexa chars) into a ib_uuid_t
 *
 * @param ibuuid pointer to an already allocated ib_uuid_t buffer
 * @param uuid pointer to the ascii string of the uuid (no blank spaces allowed)
 * @param ib pointer to the engine (to log format errors)
 *
 * @returns Status code
 */
ib_status_t ib_uuid_ascii_to_bin(ib_uuid_t *ibuuid,
                                const char *uuid);
/** @} IronBeeUtilUUID */


#ifdef __cplusplus
}
#endif

#endif /* _IB_UUID_H_ */
