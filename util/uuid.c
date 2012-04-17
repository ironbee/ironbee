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
#include <ironbee/lock.h>

#include <string.h>
#include <uuid.h>
#include <assert.h>

/*
 * These are initialized by ib_uuid_init();
 * OSSP UUID is ... generous .. in what it creates for a UUID.  E.g., it will
 * do multiple allocations, check its MAC (it may have changed?), etc. for
 * every creation.  So we only keep one at reuse it.
 */
static ib_lock_t  g_uuid_lock;
uuid_t           *g_ossp_uuid;

ib_status_t ib_uuid_initialize(void)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if (uuid_create(&g_ossp_uuid) != UUID_RC_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    rc = ib_lock_init(&g_uuid_lock);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_uuid_shutdown(void)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    rc = ib_lock_destroy(&g_uuid_lock);
    uuid_destroy(g_ossp_uuid);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_uuid_ascii_to_bin(
     ib_uuid_t *uuid,
     const char *str
)
{
    IB_FTRACE_INIT();

    uuid_rc_t uuid_rc;
    size_t uuid_len = UUID_LEN_BIN;
    size_t str_len;
    ib_status_t rc = IB_OK;

    if (uuid == NULL || str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    str_len = strlen(str);
    if (str_len != UUID_LEN_STR) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    assert(str_len == UUID_LEN_STR);

    rc = ib_lock_init(&g_uuid_lock);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    uuid_rc = uuid_import(g_ossp_uuid, UUID_FMT_STR, str, str_len);
    if (uuid_rc == UUID_RC_MEM) {
        rc = IB_EALLOC;
        goto finish;
    }
    else if (uuid_rc != UUID_RC_OK) {
        rc = IB_EINVAL;
        goto finish;
    }

    uuid_rc = uuid_export(g_ossp_uuid, UUID_FMT_BIN, (void *)&uuid, &uuid_len);
    if (uuid_rc == UUID_RC_MEM) {
        rc = IB_EALLOC;
        goto finish;
    }
    else if (uuid_rc != UUID_RC_OK || uuid_len != UUID_LEN_BIN) {
        rc = IB_EOTHER;
        goto finish;
    }

finish:
    if (ib_lock_unlock(&g_uuid_lock) != IB_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_uuid_bin_to_ascii(
    char *str,
    const ib_uuid_t *uuid
)
{
    IB_FTRACE_INIT();

    uuid_rc_t uuid_rc;
    size_t uuid_len = UUID_LEN_STR+1;
    ib_status_t rc = IB_OK;

    if (uuid == NULL || str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_lock_init(&g_uuid_lock);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    uuid_rc = uuid_import(g_ossp_uuid, UUID_FMT_BIN, uuid, UUID_LEN_BIN);
    if (uuid_rc == UUID_RC_MEM) {
        rc = IB_EALLOC;
        goto finish;
    }
    else if (uuid_rc != UUID_RC_OK) {
        rc = IB_EINVAL;
        goto finish;
    }

    uuid_rc = uuid_export(g_ossp_uuid, UUID_FMT_STR, (void *)&str, &uuid_len);
    if (uuid_rc == UUID_RC_MEM) {
        rc = IB_EALLOC;
        goto finish;
    }
    else if (uuid_rc != UUID_RC_OK || uuid_len != UUID_LEN_STR+1) {
        rc = IB_EOTHER;
        goto finish;
    }

finish:
    if (ib_lock_unlock(&g_uuid_lock) != IB_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_uuid_create_v4(ib_uuid_t *uuid)
{
    IB_FTRACE_INIT();

    uuid_rc_t uuid_rc;
    size_t uuid_len = UUID_LEN_BIN;
    ib_status_t rc = IB_OK;

    rc = ib_lock_init(&g_uuid_lock);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    uuid_rc = uuid_make(g_ossp_uuid, UUID_MAKE_V4);
    if (uuid_rc == UUID_RC_MEM) {
        rc = IB_EALLOC;
        goto finish;
    }
    else if (uuid_rc != UUID_RC_OK) {
        rc = IB_EOTHER;
        goto finish;
    }

    uuid_rc = uuid_export(g_ossp_uuid, UUID_FMT_BIN, (void *)&uuid, &uuid_len);
    if (uuid_rc == UUID_RC_MEM) {
        rc = IB_EALLOC;
        goto finish;
    }
    else if (uuid_rc != UUID_RC_OK || uuid_len != UUID_LEN_BIN) {
        rc = IB_EOTHER;
        goto finish;
    }

finish:
    if (ib_lock_unlock(&g_uuid_lock) != IB_OK) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    IB_FTRACE_RET_STATUS(rc);
}
