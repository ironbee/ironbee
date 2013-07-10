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


#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/module.h>
#include <ironbee_config_auto.h>
#if ENABLE_JSON
#include <ironbee/json.h>
#endif

#include <persistence_framework.h>

#include <assert.h>

/* Module boiler plate */
#define MODULE_NAME init_collection
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)
IB_MODULE_DECLARE();


/**
 * Module init.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module Module structure.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 */
static ib_status_t init_collection_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void *cbdata
)
{
    return IB_OK;
}

/**
 * Module destruction.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module Module structure.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 */
static ib_status_t init_collection_fini(
    ib_engine_t *ib,
    ib_module_t *module,
    void *cbdata
)
{
    return IB_OK;
}

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,    /* Headeer defaults. */
    MODULE_NAME_STR,              /* Module name. */
    NULL,                         /* Configuration. Dyanmically set in init. */
    0,                            /* Config length is 0. */
    NULL,                         /* Configuration copy function. */
    NULL,                         /* Callback data. */
    NULL,                         /* Config map. */
    NULL,                         /* Directive map. */
    init_collection_init,         /* Initialization. */
    NULL,                         /* Callback data. */
    init_collection_fini,         /* Finalization. */
    NULL,                         /* Callback data. */
);
