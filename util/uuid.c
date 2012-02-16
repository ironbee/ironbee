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

#include <ironbee/uuid.h>

#include "ironbee_config_auto.h"

#include <ironbee/debug.h>

#include <string.h>

ib_status_t ib_uuid_ascii_to_bin(ib_uuid_t *uuid,
                                 const char *str)
{
    IB_FTRACE_INIT();
    int i;
    int j;

    // Some format checks
    if (uuid == NULL ||
        strlen(str) != 36 ||
        str[8] != '-' ||
        str[13] != '-' ||
        str[18] != '-' ||
        str[23] != '-' )
    {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Store the sensor id as ib_uuid_t */
    for (i = 0, j = 0; i < 36; ++i) {
        int byteval;

        /* Skip the following positions ('-'). */
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            i++;
        }

        /* First hex char in the two digit byte. */
        if (str[i] >= '0' && str[i] <= '9') {
            byteval = (str[i] - '0') << 4;
        }
        else if (str[i] >= 'A' && str[i] <= 'F') {
            byteval = (str[i] - 'A' + 10) << 4;
        }
        else if (str[i] >= 'a' && str[i] <= 'f') {
            byteval = (str[i] - 'a' + 10) << 4;
        }
        else {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Second hex char in the two digit byte. */
        ++i;
        if (str[i] >= '0' && str[i] <= '9') {
            byteval += str[i] - '0';
        }
        else if (str[i] >= 'A' && str[i] <= 'F') {
            byteval += str[i] - 'A' + 10;
        }
        else if (str[i] >= 'a' && str[i] <= 'f') {
            byteval += str[i] - 'a' + 10;
        }
        else {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        uuid->byte[j++] = byteval & 0xff;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

