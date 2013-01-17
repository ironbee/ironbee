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

#include "managed_collection_private.h"
#include "core_private.h"
#include "engine_private.h"

#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>
#include <unistd.h>

ib_status_t ib_managed_collection_register_manager(
    ib_engine_t                            *ib,
    const ib_module_t                      *module,
    const char                             *name,
    const char                             *uri_scheme,
    ib_managed_collection_register_fn_t     register_fn,
    void                                   *register_data,
    ib_managed_collection_unregister_fn_t   unregister_fn,
    void                                   *unregister_data,
    ib_managed_collection_populate_fn_t     populate_fn,
    void                                   *populate_data,
    ib_managed_collection_persist_fn_t      persist_fn,
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

ib_status_t ib_managed_collection_create(
    ib_engine_t              *ib,
    ib_mpool_t               *mp,
    const char               *collection_name,
    ib_managed_collection_t **pcollection)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(collection_name != NULL);
    assert(pcollection != NULL);
    ib_managed_collection_t *collection;
    ib_status_t              rc;

    collection = ib_mpool_alloc(mp, sizeof(*collection));
    if (collection == NULL) {
        return IB_EALLOC;
    }
    collection->collection_name = ib_mpool_strdup(mp, collection_name);
    if (collection->collection_name == NULL) {
        return IB_EALLOC;
    }
    rc = ib_list_create(&(collection->manager_inst_list), mp);
    if (rc != IB_OK) {
        return rc;
    }

    *pcollection = collection;
    return IB_OK;
}

ib_status_t ib_managed_collection_select(
    ib_engine_t              *ib,
    ib_mpool_t               *mp,
    const char               *collection_name,
    const char               *uri,
    const ib_list_t          *params,
    ib_managed_collection_t  *collection,
    ib_list_t                *managers)
{
    assert(ib != NULL);
    assert(params != NULL);
    assert(collection != NULL);

    const ib_list_node_t *node;

    /* If there is no manager list, we're not going to match! */
    if (ib->collection_managers == NULL) {
        return IB_ENOENT;
    }

    /* Walk through the list of managers, register all of the ones that whose
     * uri_scheme matches the URI.  Any of the register functions can return
     * IB_DECLINED to indicate that it can't managed the collection. */
    IB_LIST_LOOP_CONST(ib->collection_managers, node) {
        const ib_collection_manager_t *manager =
            (const ib_collection_manager_t *)node->data;
        ib_collection_manager_inst_t *inst;
        void *inst_data;
        const char *uri_data;
        size_t slen;
        ib_status_t rc;

        /* Does the scheme match? */
        slen = strlen(manager->uri_scheme);
        if (strncasecmp(manager->uri_scheme, uri, slen) != 0) {
            continue;
        }
        uri_data = (uri + slen);

        /* Register the managed collection with the collection manager */
        rc = manager->register_fn(ib,
                                  manager->module,
                                  manager,
                                  mp,
                                  collection_name,
                                  uri,
                                  manager->uri_scheme,
                                  uri_data,
                                  params,
                                  manager->register_data,
                                  &inst_data);
        if (rc == IB_DECLINED) {
            continue;
        }
        else if (rc != IB_OK) {
            return rc;
        }

        if (managers != NULL) {
            rc = ib_list_push(managers, (ib_collection_manager_t *)manager);
        }

        /* Create & populate the new collection instance object */
        inst = ib_mpool_alloc(mp, sizeof(*inst));
        if (inst == NULL) {
            return IB_EALLOC;
        }
        inst->manager           = manager;
        inst->collection        = collection;
        inst->uri               = ib_mpool_strdup(mp, uri);
        inst->manager_inst_data = inst_data;

        /* Push the instance on the managers list */
        rc = ib_list_push(collection->manager_inst_list, inst);
        if (rc != IB_OK) {
            return rc;
        }

        ib_log_trace(ib,
                     "Registered collection manager \"%s\" "
                     "for collection \"%s\" URI \"%s\"",
                     manager->name, collection_name, uri);
        return IB_OK;
    }
    return IB_ENOENT;
}

ib_status_t ib_managed_collection_unregister(
    ib_engine_t                   *ib,
    ib_module_t                   *module,
    const ib_managed_collection_t *collection)
{
    ib_status_t rc;
    const ib_list_node_t *node;

    /* Loop through the collection manager list, unregister them all */
    IB_LIST_LOOP_CONST(collection->manager_inst_list, node) {
        const ib_collection_manager_inst_t *manager_inst =
            (const ib_collection_manager_inst_t *)node->data;
        const ib_collection_manager_t *manager =
            manager_inst->manager;

        /* Invoke the populate function to populate the new collection */
        if (manager->unregister_fn != NULL) {
            rc = manager->unregister_fn(ib, manager->module, manager,
                                        collection->collection_name,
                                        manager_inst->manager_inst_data,
                                        manager->unregister_data);
            if (rc != IB_OK) {
                ib_log_error(ib,
                             "Failed to unregister collection manager \"%s\" "
                             "for managed collection \"%s\": %s",
                             manager->name, collection->collection_name,
                             ib_status_to_string(rc));
            }
        }
    }
    ib_list_clear(collection->manager_inst_list);

    return IB_OK;
}

ib_status_t ib_managed_collection_populate(
    const ib_engine_t             *ib,
    ib_tx_t                       *tx,
    const ib_managed_collection_t *collection)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(collection != NULL);

    ib_managed_collection_inst_t *inst;
    const ib_list_node_t *node;
    ib_field_t *field;
    ib_list_t *list;
    ib_status_t rc;

    /* Create the collection */
    rc = ib_data_add_list(tx->data, collection->collection_name, &field);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_field_value(field, ib_ftype_list_mutable_out(&list));
    if (rc != IB_OK) {
        return rc;
    }

    /* Walk through all of the associated managers.
     * The first to return IB_OK causes the loop to exit. */
    ib_log_debug_tx(tx, "Attempting to populate managed collection \"%s\"",
                    collection->collection_name);
    IB_LIST_LOOP_CONST(collection->manager_inst_list, node) {
        const ib_collection_manager_inst_t *manager_inst =
            (const ib_collection_manager_inst_t *)node->data;
        const ib_collection_manager_t *manager =
            manager_inst->manager;

        /* Invoke the populate function to populate the new collection */
        if (manager->populate_fn != NULL) {
            rc = manager->populate_fn(ib, tx,
                                      manager->module, manager,
                                      collection->collection_name,
                                      list,
                                      manager_inst->manager_inst_data,
                                      manager->populate_data);

            /* If the populate function declined, try the next one */
            if (rc == IB_DECLINED) {
                ib_log_trace_tx(tx, "Collection manager \"%s\" declined to "
                                "populate \"%s\"",
                                manager->name, collection->collection_name);
                continue;
            }
            else if (rc != IB_OK) {
                ib_log_warning_tx(tx,
                                  "Collection manager \"%s\" "
                                  "failed to populate \"%s\": %s",
                                  manager->name, collection->collection_name,
                                  ib_status_to_string(rc));
                return rc;
            }
            else {
                ib_log_trace_tx(tx,
                                "Collection manager \"%s\" populated \"%s\"",
                                manager->name, collection->collection_name);
                break;
            }
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
    ib_tx_t           *tx)
{
    assert(ib != NULL);
    assert(tx != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    /* If there is no list created, nothing to do */
    if (tx->managed_collections == NULL) {
        ib_log_trace_tx(tx, "Not managed collections to persist");
        return IB_OK;
    }

    /* Walk through the list of collections */
    ib_log_debug_tx(tx, "Persisting %zd managed collections",
                    ib_list_elements(tx->managed_collections));
    IB_LIST_LOOP_CONST(tx->managed_collections, node) {
        const ib_managed_collection_inst_t *collection_inst =
            (const ib_managed_collection_inst_t *)node->data;
        const ib_managed_collection_t *collection = collection_inst->collection;
        const ib_list_node_t *manager_inst_node;

        ib_log_debug_tx(tx, "Attempting to persist managed collection \"%s\"",
                        collection->collection_name);

        IB_LIST_LOOP_CONST(collection->manager_inst_list, manager_inst_node) {
            const ib_collection_manager_inst_t *manager_inst =
                (const ib_collection_manager_inst_t *)manager_inst_node->data;
            const ib_collection_manager_t *manager = manager_inst->manager;

            ib_status_t tmprc;

            if (manager->persist_fn == NULL) {
                continue;
            }

            /* Tell the manager to persist the collection */
            tmprc = manager->persist_fn(ib, tx, manager->module, manager,
                                        collection->collection_name,
                                        collection_inst->collection_list,
                                        manager_inst->manager_inst_data,
                                        manager->persist_data);
            if (tmprc == IB_DECLINED) {
                ib_log_trace_tx(tx,
                                "Collection manager \"%s\" "
                                "declined to persist \"%s\"",
                                manager->name, collection->collection_name);
            }
            else if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
                ib_log_warning_tx(tx,
                                  "Collection manager \"%s\" "
                                  "failed to persist \"%s\": %s",
                                  manager->name, collection->collection_name,
                                  ib_status_to_string(rc));
                rc = tmprc;
            }
            else if (tmprc == IB_OK) {
                ib_log_trace_tx(tx,
                                "Collection manager \"%s\" persisted \"%s\"",
                                manager->name, collection->collection_name);
            }
        }
    }

    return rc;
}

const char *ib_managed_collection_manager_name(
    const ib_collection_manager_t *manager)
{
    assert(manager != NULL);
    return manager->name;
}

ib_status_t ib_managed_collection_populate_from_list(
    const ib_tx_t                 *tx,
    const ib_list_t               *field_list,
    ib_list_t                     *collection)
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

ib_status_t ib_managed_collection_init(
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

ib_status_t ib_managed_collection_finish(
    ib_engine_t *ib)
{
    return IB_OK;
}
