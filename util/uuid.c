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
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @todo Add bin to ascii
 */

#include "ironbee_config_auto.h"

#include <ironbee/uuid.h>

#include <ironbee/debug.h>

#include <string.h>
#include <ossp/uuid.h>
#include <assert.h>

ib_status_t ib_uuid_ascii_to_bin(
     ib_uuid_t *uuid,
     const char *str
)
{
    IB_FTRACE_INIT();

    uuid_t *ossp_uuid;
    uuid_rc_t uuid_rc;
    size_t uuid_len = UUID_LEN_BIN;
    size_t str_len;

    if (uuid == NULL || str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    str_len = strlen(str);
    if (str_len != UUID_LEN_STR) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    uuid_rc = uuid_create(&ossp_uuid);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    assert(str_len == UUID_LEN_STR);
    uuid_rc = uuid_import(ossp_uuid, UUID_FMT_STR, str, str_len);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    uuid_rc = uuid_export(ossp_uuid, UUID_FMT_BIN, &uuid, &uuid_len);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK || uuid_len != UUID_LEN_BIN) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    uuid_destroy(ossp_uuid);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_uuid_bin_to_ascii(
    char *str,
    const ib_uuid_t *uuid
)
{
    IB_FTRACE_INIT();

    uuid_t *ossp_uuid;
    uuid_rc_t uuid_rc;
    size_t uuid_len = UUID_LEN_STR+1;

    if (uuid == NULL || str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    uuid_rc = uuid_create(&ossp_uuid);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    uuid_rc = uuid_import(ossp_uuid, UUID_FMT_BIN, uuid, UUID_LEN_BIN);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    uuid_rc = uuid_export(ossp_uuid, UUID_FMT_STR, &str, &uuid_len);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK || uuid_len != UUID_LEN_STR+1) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    uuid_destroy(ossp_uuid);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_uuid_create_v4(ib_uuid_t *uuid)
{
    IB_FTRACE_INIT();

    uuid_t *ossp_uuid;
    uuid_rc_t uuid_rc;
    size_t uuid_len = UUID_LEN_BIN;

    uuid_rc = uuid_create(&ossp_uuid);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    uuid_rc = uuid_make(ossp_uuid, UUID_MAKE_V4);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    uuid_rc = uuid_export(ossp_uuid, UUID_FMT_BIN, &uuid, &uuid_len);
    if (uuid_rc == UUID_RC_MEM) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    } else if (uuid_rc != UUID_RC_OK || uuid_len != UUID_LEN_BIN) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    uuid_destroy(ossp_uuid);

    IB_FTRACE_RET_STATUS(IB_OK);
}
