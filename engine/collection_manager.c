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
 * @brief IronBee --- Managed Collection Logic
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/collection_manager.h>
#include "collection_manager_private.h"

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#include "core_private.h"
#include "engine_private.h"

#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>
#include <unistd.h>

ib_status_t ib_collection_manager_init(
    ib_engine_t *ib)
{
    ib_status_t rc;

    /* Create the collection manager list */
    rc = ib_list_create(&(ib->collection_managers), ib->mp);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_collection_manager_finish(
    ib_engine_t *ib)
{
    return IB_OK;
}

ib_status_t ib_collection_manager_register(
    ib_engine_t                            *ib,
    const ib_module_t                      *module,
    const char                             *name,
    const char                             *uri_scheme,
    ib_collection_manager_register_fn_t     register_fn,
    void                                   *register_data,
    ib_collection_manager_unregister_fn_t   unregister_fn,
    void                                   *unregister_data,
    ib_collection_manager_populate_fn_t     populate_fn,
    void                                   *populate_data,
    ib_collection_manager_persist_fn_t      persist_fn,
    void                                   *persist_data,
    const ib_collection_manager_t         **pmanager)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(name != NULL);
    assert(uri_scheme != NULL);
    assert(register_fn != NULL);

    ib_status_t rc;
    ib_collection_manager_t *manager;

    /* Allocation and populate a manager object */
    manager = ib_mpool_alloc(ib->mp, sizeof(*manager));
    if (manager == NULL) {
        return IB_EALLOC;
    }
    manager->name            = ib_mpool_strdup(ib->mp, name);
    manager->uri_scheme      = ib_mpool_strdup(ib->mp, uri_scheme);
    manager->module          = module;
    manager->register_fn     = register_fn;
    manager->register_data   = register_data;
    manager->unregister_fn   = unregister_fn;
    manager->unregister_data = unregister_data;
    manager->populate_fn     = populate_fn;
    manager->populate_data   = populate_data;
    manager->persist_fn      = persist_fn;
    manager->persist_data    = persist_data;

    /* If the caller wants a handle to the manager, give it to them */
    if (pmanager != NULL) {
        *pmanager = manager;
    }

    /* Push the new manager onto the manager list */
    rc = ib_list_push(ib->collection_managers, manager);
    return rc;
}

const char *ib_collection_manager_name(
    const ib_collection_manager_t *manager)
{
    assert(manager != NULL);
    return manager->name;
}

ib_status_t ib_collection_manager_populate_from_list(
    const ib_tx_t   *tx,
    const ib_list_t *field_list,
    ib_list_t       *collection)
{
    assert(field_list != NULL);
    assert(collection != NULL);

    const ib_list_node_t *node;

    /* Copy all of the fields from the field list to the collection */
    IB_LIST_LOOP_CONST(field_list, node) {
        ib_status_t rc;
        const ib_field_t *field = (const ib_field_t *)node->data;
        ib_field_t *newf;

        rc = ib_field_copy(&newf, tx->mp, field->name, field->nlen, field);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_list_push(collection, newf);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}
