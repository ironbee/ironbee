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

#include <ironbee/managed_collection.h>

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

ib_status_t ib_managed_collection_register_handler(
    ib_engine_t *ib,
    const ib_module_t *module,
    const char *name,
    ib_managed_collection_selection_fn_t selection_fn,
    void *selection_data,
    ib_managed_collection_populate_fn_t populate_fn,
    void *populate_data,
    ib_managed_collection_persist_fn_t persist_fn,
    void *persist_data)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(name != NULL);
    assert(selection_fn != NULL);

    ib_status_t rc;
    ib_collection_manager_t *manager;

    /* Create the manager list if required */
    if (ib->collection_managers == NULL) {
        rc = ib_list_create(&(ib->collection_managers), ib->mp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Allocation and populate a manager object */
    manager = ib_mpool_alloc(ib->mp, sizeof(*manager));
    if (manager == NULL) {
        return IB_EALLOC;
    }
    manager->name = ib_mpool_strdup(ib->mp, name);
    manager->module = module;
    manager->selection_fn = selection_fn;
    manager->selection_data = selection_data;
    manager->populate_fn = populate_fn;
    manager->populate_data = populate_data;
    manager->persist_fn = persist_fn;
    manager->persist_data = persist_data;

    /* Push the new manager onto the manager list */
    rc = ib_list_push(ib->collection_managers, manager);
    return rc;
}

ib_status_t ib_managed_collection_select(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    const char *collection_name,
    const ib_list_t *params,
    const ib_managed_collection_t **pcollection)
{
    assert(ib != NULL);
    assert(params != NULL);
    assert(pcollection != NULL);

    const ib_list_node_t    *node;

    *pcollection = NULL;

    /* If there is no manager list, we're not going to match! */
    if (ib->collection_managers == NULL) {
        return IB_ENOENT;
    }

    /* Walk through the list of managers, take the first one whose
     * selection function returns true */
    IB_LIST_LOOP_CONST(ib->collection_managers, node) {
        const ib_collection_manager_t *manager =
            (const ib_collection_manager_t *)node->data;
        ib_managed_collection_t *collection;
        ib_status_t selected;
        void *data;

        /* Ask the selection function if it wants to handle this one */
        selected = manager->selection_fn(ib,
                                         manager->module,
                                         mp,
                                         collection_name,
                                         params,
                                         manager->selection_data,
                                         &data);
        if (selected == IB_DECLINED) {
            continue;
        }
        else if (selected != IB_OK) {
            return selected;
        }

        /* Create & populate the new collection object */
        collection = ib_mpool_alloc(mp, sizeof(*collection));
        if (collection == NULL) {
            return IB_EALLOC;
        }
        collection->name = ib_mpool_strdup(mp, collection_name);
        collection->manager = manager;
        collection->data = data;
        *pcollection = collection;
        return IB_OK;
    }
    return IB_ENOENT;
}

ib_status_t ib_managed_collection_populate(
    const ib_engine_t *ib,
    ib_tx_t *tx,
    const ib_managed_collection_t *collection)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(collection != NULL);

    ib_managed_collection_inst_t *inst;
    const ib_collection_manager_t *manager = collection->manager;
    ib_field_t *field;
    ib_list_t *list;
    ib_status_t rc;

    /* Create the collection */
    rc = ib_data_add_list(tx->data, collection->name, &field);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_field_value(field, ib_ftype_list_mutable_out(&list));
    if (rc != IB_OK) {
        return rc;
    }

    /* Invoke the populate function to populate the new collection */
    if (manager->populate_fn != NULL) {
        rc = manager->populate_fn(ib, tx, manager->module,
                                  collection->name,
                                  collection->data,
                                  list,
                                  manager->populate_data);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Create the instance list in the tx for the first one */
    if (tx->managed_collections == NULL) {
        rc = ib_list_create(&(tx->managed_collections), tx->mp);
        if (rc != IB_OK) {
            return IB_EALLOC;
        }
    }

    /* Create the managed collection instance object */
    inst = ib_mpool_alloc(tx->mp, sizeof(*inst));
    if (inst == NULL) {
        return IB_EALLOC;
    }
    inst->collection_list = list;
    inst->collection = collection;

    /* Add the instance object to the list list of managed collections */
    rc = ib_list_push(tx->managed_collections, inst);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

ib_status_t ib_managed_collection_persist_all(
    const ib_engine_t *ib,
    ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(tx != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    /* If there is no list created, nothing to do */
    if (tx->managed_collections == NULL) {
        return IB_OK;
    }

    /* Walk through the list of collections */
    IB_LIST_LOOP_CONST(tx->managed_collections, node) {
        const ib_managed_collection_inst_t *collection_inst =
            (const ib_managed_collection_inst_t *)node->data;
        const ib_managed_collection_t *collection = collection_inst->collection;
        const ib_collection_manager_t *manager = collection->manager;
        ib_status_t tmprc;

        if (manager->persist_fn == NULL) {
            continue;
        }

        /* Tell the manager to persist the collection */
        tmprc = manager->persist_fn(ib, tx, manager->module,
                                    collection->name,
                                    collection->data,
                                    collection_inst->collection_list,
                                    manager->persist_data);
        if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
            rc = tmprc;
        }
    }

    return rc;
}
