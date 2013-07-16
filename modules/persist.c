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

/**
 * @file
 * @brief IronBee --- Persistence module.
 */

#include "ironbee_config_auto.h"

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/json.h>
#include <ironbee/kvstore.h>
#include <ironbee/kvstore_filesystem.h>
#include <ironbee/list.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <pcre.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

//! Default expiration time of persisted collections (useconds)
static const ib_time_t default_expiration = 60LU * 1000000LU;

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        persist
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Initialize persist managed collection module
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] cbdata Callback data
 *
 * @returns Status code:
 *   - IB_OK All OK, parameters recognized
 *   - IB_Exxx Other error
 */
static ib_status_t mod_persist_init(
    ib_engine_t  *ib,
    ib_module_t  *module,
    void         *cbdata)
{
    assert(ib != NULL);
    assert(module != NULL);

    ib_log_debug(ib, "Default expiration time: %zd", default_expiration);

    return IB_OK;
}

static ib_status_t mod_persist_fini(ib_engine_t *ib,
                                    ib_module_t *module,
                                    void *cbdata)
{
    return IB_OK;
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,             /* Default metadata */
    MODULE_NAME_STR,                       /* Module name */
    IB_MODULE_CONFIG_NULL,                 /* Set by initializer. */
    NULL,                                  /* Configuration field map */
    NULL,                                  /* Config directive map */
    mod_persist_init,                      /* Initialize function */
    NULL,                                  /* Callback data */
    mod_persist_fini,                      /* Finish function */
    NULL,                                  /* Callback data */
);
