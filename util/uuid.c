/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.    See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.    You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 * @brief UUID helper functions
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 * @todo Add bin to ascii
 */

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>

#include "ironbee_util_private.h"

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
                                const char *uuid)
{
    IB_FTRACE_INIT(ib_uuid_ascii_to_bin);
    uint8_t *u = (uint8_t *)ibuuid;
    
    // Some format checks
    if (u == NULL ||
        strlen(uuid) != 36 ||
        uuid[8] != '-' ||
        uuid[13] != '-' ||
        uuid[18] != '-' ||
        uuid[23] != '-' ) 
    {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Store the sensor id as ib_uuid_t */
    uint8_t i = 0;
    uint8_t j = 0;
    for (i = 0; i < 36; i += 2) {
        /* Skip the following positions ('-') */
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            i++;
        }
        if ( !((uuid[i] >= '0' && uuid[i] <= '9') ||
               (uuid[i] >= 'A' && uuid[i] <= 'F') ||
               (uuid[i] >= 'a' && uuid[i] <= 'f')) ||
             !((uuid[i + 1] >= '0' && uuid[i + 1] <= '9') ||
               (uuid[i + 1] >= 'A' && uuid[i + 1] <= 'F') ||
               (uuid[i + 1] >= 'a' && uuid[i + 1] <= 'f'))
               )
        {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        unsigned int tmphex = 0;
        sscanf(&uuid[i], "%02x", &tmphex);
        *(u + j++) = (uint8_t) tmphex & 0xff;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

