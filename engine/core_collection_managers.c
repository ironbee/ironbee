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
 * @brief IronBee --- Core Module: Collection Managers
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#include "core_private.h"
#include "engine_private.h"

#include <ironbee/core.h>
#include <ironbee/json.h>
#include <ironbee/collection_manager.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <pcre.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>

/** Name/value Pair static data */
typedef struct {
    const pcre                    *pattern;   /**< Compiled PCRE */
    const ib_collection_manager_t *manager;   /**< The manager object */
} core_vars_manager_t;
static core_vars_manager_t core_vars_manager = { NULL, NULL };

/** Core InitCollection vars parameter data */
typedef struct {
    const char     *name;            /**< Variable name */
    const char     *value;           /**< Variable value */
} core_vars_t;

/** Core JSON file parameter data */
typedef struct {
    const char     *path;            /**< Path to the file */
    bool            persist;         /**< Persist the collection? */
} core_json_file_t;

/**
 * Handle managed collection registration for vars "name=value" parameters
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object to register with
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the collection
 * @param[in] uri Full collection URI (unused)
 * @param[in] uri_scheme URI scheme (unused)
 * @param[in] uri_data Hierarchical/data part of the URI
 * @param[in] params List of parameter strings
 * @param[in] register_data Register callback data
 * @param[out] pmanager_inst_data Pointer to manager specific collection data
 *
 * @returns Status code:
 *   - IB_OK All OK
 *   - IB_Exxx Other error
 */
static ib_status_t core_managed_collection_vars_register_fn(
    const ib_engine_t             *ib,
    const ib_module_t             *module,
    const ib_collection_manager_t *manager,
    ib_mpool_t                    *mp,
    const char                    *collection_name,
    const char                    *uri,
    const char                    *uri_scheme,
    const char                    *uri_data,
    const ib_list_t               *params,
    void                          *register_data,
    void                         **pmanager_inst_data)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(mp != NULL);
    assert(collection_name != NULL);
    assert(params != NULL);
    assert(pmanager_inst_data != NULL);

    const ib_list_node_t *node;
    ib_list_t *vars_list;
    ib_list_t *field_list;
    ib_mpool_t *tmp = ib_engine_pool_temp_get(ib);
    ib_status_t rc;

    if (strlen(uri_data) != 0) {
        return IB_DECLINED;
    }
    if (ib_list_elements(params) < 1) {
        return IB_EINVAL;
    }

    /* Create a temporary list */
    rc = ib_list_create(&vars_list, tmp);
    if (rc != IB_OK) {
        return rc;
    }

    /* First pass; walk through all params, look for "a=b" type syntax */
    IB_LIST_LOOP_CONST(params, node) {
        const int ovecsize = 9;
        int ovector[ovecsize];
        const char *param = (const char *)node->data;
        core_vars_t *vars;
        int pcre_rc;

        pcre_rc = pcre_exec(core_vars_manager.pattern, NULL,
                            param, strlen(param),
                            0, 0, ovector, ovecsize);
        if (pcre_rc < 0) {
            return IB_DECLINED;
        }

        vars = ib_mpool_alloc(tmp, sizeof(*vars));
        if (vars == NULL) {
            return IB_EALLOC;
        }
        vars->name  = ib_mpool_memdup_to_str(tmp,
                                             param + ovector[2],
                                             ovector[3] - ovector[2]);
        vars->value = ib_mpool_memdup_to_str(tmp,
                                             param + ovector[4],
                                             ovector[5] - ovector[4]);
        if ( (vars->name == NULL) || (vars->value == NULL) ) {
            return IB_EALLOC;
        }
        rc = ib_list_push(vars_list, vars);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Build the list of fields */
    rc = ib_list_create(&field_list, mp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Now walk though the list, creating a field for each one */
    IB_LIST_LOOP_CONST(vars_list, node) {
        const core_vars_t *vars = (const core_vars_t *)node->data;
        ib_field_t *field;
        ib_field_val_union_t fval;

        rc = ib_field_from_string(mp,
                                  IB_FIELD_NAME(vars->name), vars->value,
                                  &field, &fval);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error creating field (\"%s\", \"%s\"): %s",
                         vars->name, vars->value,
                         ib_status_to_string(rc));
            return rc;
        }
        rc = ib_list_push(field_list, field);
        if (rc != IB_OK) {
            return rc;
        }

        if (field->type == IB_FTYPE_NUM) {
            ib_log_debug(ib, "Created numeric field \"%s\" %"PRId64" in \"%s\"",
                         vars->name, fval.num, collection_name);
        }

        else if (field->type == IB_FTYPE_FLOAT) {
            ib_log_debug(ib, "Created float field \"%s\" %f in \"%s\"",
                         vars->name, (double)fval.fnum, collection_name);
        }
        else {
            ib_log_debug(ib, "Created string field \"%s\" \"%s\" in \"%s\"",
                         vars->name, fval.nulstr, collection_name);
        }
    }

    /* Finally, store the list as the manager specific collection data */
    *pmanager_inst_data = field_list;

    return IB_OK;
}

/**
 * Handle managed collection vars populate function
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to populate
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object to register with
 * @param[in] collection_name The name of the collection.
 * @param[in,out] collection Collection to populate with fields in @a
 *                collection_data.
 * @param[in] manager_inst_data Manager instance data
 * @param[in] populate_data Populate callback data
 *
 * @returns
 *   - IB_OK on success or when @a collection_data is length 0.
 *   - Errors returned by ib_collection_manager_populate_from_list()
 */
static ib_status_t core_managed_collection_vars_populate_fn(
    const ib_engine_t             *ib,
    const ib_tx_t                 *tx,
    const ib_module_t             *module,
    const ib_collection_manager_t *manager,
    const char                    *collection_name,
    ib_list_t                     *collection,
    void                          *manager_inst_data,
    void                          *populate_data)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(module != NULL);
    assert(collection_name != NULL);
    assert(collection != NULL);
    assert(manager_inst_data != NULL);

    const ib_list_t *field_list = (const ib_list_t *)manager_inst_data;
    ib_status_t rc;

    rc = ib_collection_manager_populate_from_list(tx, field_list, collection);
    return rc;
}

#if ENABLE_JSON
/**
 * Handle managed collection: register for JSON file
 *
 * Examines the incoming parameters; if if it looks like a JSON file,
 * take it; otherwise do nothing (decline)
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the collection
 * @param[in] uri Full collection URI
 * @param[in] uri_scheme URI scheme (unused)
 * @param[in] uri_data Hierarchical/data part of the URI (typically a path)
 * @param[in] params List of parameter strings
 * @param[in] data Selection callback data
 * @param[out] pmanager_inst_data Pointer to manager specific data
 *
 * @returns Status code:
 *   - IB_DECLINED Parameters not recognized
 *   - IB_OK All OK, parameters recognized
 *   - IB_Exxx Other error
 */
static ib_status_t core_managed_collection_jsonfile_register_fn(
    const ib_engine_t              *ib,
    const ib_module_t              *module,
    const ib_collection_manager_t  *manager,
    ib_mpool_t                     *mp,
    const char                     *collection_name,
    const char                     *uri,
    const char                     *uri_scheme,
    const char                     *uri_data,
    const ib_list_t                *params,
    void                           *register_data,
    void                          **pmanager_inst_data)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(mp != NULL);
    assert(collection_name != NULL);
    assert(params != NULL);
    assert(pmanager_inst_data != NULL);

    const ib_list_node_t *node;
    const char *path;
    const char *param;
    const char *path_start = uri_data;
    bool persist = false;
    core_json_file_t *json_file;

    /* Get the first element in the list */
    if (ib_list_elements(params) > 1) {
        return IB_EINVAL;
    }

    /* Look at the first param (if it exists) */
    node = ib_list_first_const(params);
    if (node != NULL) {
        param = (const char *)node->data;
        if (strcasecmp(param, "persist") == 0) {
            persist = true;
        }
        else {
            ib_log_warning(ib, "JSON file: \"%s\"; unknown parameter \"%s\"",
                           uri, param);
            return IB_EINVAL;
        }
    }

    /* Try to stat the file */
    if (!persist) {
        struct stat sbuf;
        if (stat(path_start, &sbuf) < 0) {
            ib_log_warning(ib,
                           "JSON file: Declining \"%s\"; "
                           "stat(\"%s\") failed: %s",
                           uri, path_start, strerror(errno));
            return IB_DECLINED;
        }
        if (! S_ISREG(sbuf.st_mode)) {
            ib_log_warning(ib,
                           "JSON file: Declining \"%s\"; \"%s\" is not a file",
                           uri, path_start);
            return IB_DECLINED;
        }
    }

    /* Happy now, copy the file name, be done */
    path = ib_mpool_strdup(mp, path_start);
    if (path == NULL) {
        return IB_EALLOC;
    }
    json_file = ib_mpool_alloc(mp, sizeof(*json_file));
    if (json_file == NULL) {
        return IB_EALLOC;
    }
    json_file->path    = path;
    json_file->persist = persist;

    /* Store the file object as the manager specific collection data */
    *pmanager_inst_data = json_file;

    return IB_OK;
}

/**
 * Handle managed collection: JSON file populate function
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to populate
 * @param[in] module Collection manager's module object
 * @param[in] collection_name The name of the collection.
 * @param[in] collection_data An ib_list_t of fields copied into @a collection.
 * @param[in,out] collection Collection to populate with fields in @a
 *                collection_data.
 * @param[in] populate_data Populate callback data
 *
 * @returns
 *   - IB_OK on success or when @a collection_data is length 0.
 *   - IB_EUNKOWN for errors from file I/O
 *   - IB_EALLOC for allocation errors
 *   - Errors from ib_json_decode_ex()
 */
static ib_status_t core_managed_collection_jsonfile_populate_fn(
    const ib_engine_t             *ib,
    const ib_tx_t                 *tx,
    const ib_module_t             *module,
    const ib_collection_manager_t *manager,
    const char                    *collection_name,
    ib_list_t                     *collection,
    void                          *manager_inst_data,
    void                          *populate_data)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(module != NULL);
    assert(manager != NULL);
    assert(collection_name != NULL);
    assert(collection != NULL);
    assert(manager_inst_data != NULL);

    const core_json_file_t *json_file =
        (const core_json_file_t *)manager_inst_data;
    int fd;
    ssize_t filesize;
    ssize_t remain;
    uint8_t *buf;
    uint8_t *bufp;
    struct stat sbuf;
    ib_status_t rc;
    const char *error;

    /* Get the file's size */
    if (stat(json_file->path, &sbuf) < 0) {
        ib_log_warning(ib, "JSON file: stat(\"%s\") failed: %s",
                       json_file->path, strerror(errno));
        return json_file->persist ? IB_OK : IB_DECLINED;
    }
    else {
        filesize = sbuf.st_size;
        if (filesize == 0) {
            return IB_OK;
        }
    }

    fd = open(json_file->path, O_RDONLY);
    if (fd < 0) {
        ib_log_warning(ib, "JSON file: open(\"%s\") failed: %s",
                       json_file->path, strerror(errno));
        return IB_DECLINED;
    }
    buf = ib_mpool_alloc(tx->mp, filesize);
    if (buf == NULL) {
        return IB_EALLOC;
    }
    remain = filesize;
    bufp = buf;
    while(remain > 0) {
        ssize_t bytes = read(fd, bufp, remain);
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            ib_log_warning(ib, "JSON file: read(\"%s\") failed: %s",
                           json_file->path, strerror(errno));
            close(fd);
            return IB_EUNKNOWN;
        }
        else if (bytes == 0) {
            ib_log_warning(ib, "JSON file: \"%s\": end of file reached",
                           json_file->path);
            close(fd);
            return IB_EUNKNOWN;
        }
        else {
            remain -= bytes;
            bufp += bytes;
            assert(remain >= 0);
        }
    }
    close(fd);

    /* Now, decode the JSON buffer */
    rc = ib_json_decode_ex(tx->mp, buf, filesize, collection, &error);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error decoding JSON buffer for \"%s\": \"%s\"",
                     collection_name,
                     error == NULL ? ib_status_to_string(rc) : error);
    }
    else {
        ib_log_debug(ib,
                     "Populated collection \"%s\" from JSON file \"%s\"",
                     collection_name, json_file->path);
    }

    return rc;
}

/**
 * Handle managed collection: JSON file persist function
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to select a context for (or NULL)
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of the collection to populate
 * @param[in] collection Collection to populate
 * @param[in] manager_inst_data Manager instance data
 * @param[in] persist_data Persist callback data
 *
 * @returns
 *   - IB_OK on success or when @a collection_data is length 0.
 *   - IB_DECLINED if not configured to persist
 *   - IB_EUNKNOWN for file open/write/close errors
 *   - Errors returned by ib_json_encode()
 */
static ib_status_t core_managed_collection_jsonfile_persist_fn(
    const ib_engine_t             *ib,
    const ib_tx_t                 *tx,
    const ib_module_t             *module,
    const ib_collection_manager_t *manager,
    const char                    *collection_name,
    const ib_list_t               *collection,
    void                          *manager_inst_data,
    void                          *persist_data)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(module != NULL);
    assert(manager != NULL);
    assert(collection_name != NULL);
    assert(collection != NULL);
    assert(manager_inst_data != NULL);

    const core_json_file_t *json_file =
        (const core_json_file_t *)manager_inst_data;
    int fd;
    ssize_t remain;
    char *buf;
    size_t bufsize;
    uint8_t *bufp;
    ib_status_t rc;

    if (! json_file->persist) {
        return IB_DECLINED;
    }

    rc = ib_json_encode(tx->mp, collection, true, &buf, &bufsize);
    if (rc != IB_OK) {
        ib_log_warning(ib,
                       "JSON file: failed to encode collection \"%s\": %s",
                       collection_name, strerror(errno));
        return IB_EUNKNOWN;
    }

    fd = open(json_file->path, O_WRONLY | O_CREAT | O_TRUNC,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        ib_log_warning(ib, "JSON file persist: open(\"%s\") failed: %s",
                       json_file->path, strerror(errno));
        return IB_EUNKNOWN;
    }
    remain = (ssize_t)bufsize;
    bufp = (uint8_t *)buf;
    while(remain > 0) {
        ssize_t bytes = write(fd, bufp, remain);
        if (bytes <= 0) {
            if (errno == EINTR) {
                continue;
            }
            ib_log_warning(ib, "JSON file: write(\"%s\") failed: %s",
                           json_file->path, strerror(errno));
            close(fd);
            return IB_EUNKNOWN;
        }
        else {
            remain -= bytes;
            bufp += bytes;
            assert(remain >= 0);
        }
    }
    close(fd);

    return IB_OK;
}
#endif /* ENABLE_JSON */

ib_status_t ib_core_collection_managers_register(
    ib_engine_t  *ib,
    const ib_module_t *module)
{
    assert(ib != NULL);
    assert(module != NULL);

    const char *pattern = "^(\\w+)=(.*)$";
    const int compile_flags = PCRE_DOTALL | PCRE_DOLLAR_ENDONLY;
    pcre *compiled;
    const char *error;
    int eoff;
    ib_status_t rc;
    const ib_collection_manager_t *manager;

    /* Register the name/value pair InitCollection manager */
    rc = ib_collection_manager_register(
        ib, module, "core name/value pair", "vars:",
        core_managed_collection_vars_register_fn, NULL,
        NULL, NULL,
        core_managed_collection_vars_populate_fn, NULL,
        NULL, NULL,
        &manager);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to register core name/value pair handler: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Compile the name/value pair pattern */
    compiled = pcre_compile(pattern, compile_flags, &error, &eoff, NULL);
    if (compiled == NULL) {
        ib_log_error(ib, "Failed to compile pattern \"%s\": %s", pattern,
                     error ? error : "(null)");
        return IB_EUNKNOWN;
    }
    core_vars_manager.pattern = compiled;
    core_vars_manager.manager = manager;

#if ENABLE_JSON
    /* Register the JSON file InitCollection manager */
    rc = ib_collection_manager_register(
        ib, module, "core JSON file", "json-file://",
        core_managed_collection_jsonfile_register_fn, NULL,
        NULL, NULL,
        core_managed_collection_jsonfile_populate_fn, NULL,
        core_managed_collection_jsonfile_persist_fn, NULL,
        &manager);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to register core JSON file handler: %s",
                     ib_status_to_string(rc));
        return rc;
    }
#endif

    return IB_OK;
}
