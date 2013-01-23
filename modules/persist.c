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

#include "ironbee_config_auto.h"

#include <ironbee/engine.h>
#include <ironbee/json.h>
#include <ironbee/kvstore.h>
#include <ironbee/kvstore_filesystem.h>
#include <ironbee/list.h>
#include <ironbee/collection_manager.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <pcre.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

/** File system persistence parameter parsing data */
typedef struct {
    const pcre   *key_pcre;          /**< Compiled PCRE to match key=[name] */
    const ib_collection_manager_t *manager; /**< Collection manager */
} mod_persist_param_data_t;
static mod_persist_param_data_t mod_persist_param_data = { NULL, NULL };

/** File system persistence kvstore data */
typedef struct {
    const char    *collection_name;  /**< Name of the collection */
    const char    *path;             /**< Path to the fs kvstore */
    const char    *key;              /**< Key in TX data for population */
    bool           key_expand;       /**< Key is expandable */
    ib_kvstore_t  *kvstore;          /**< kvstore object */
    uint32_t       expiration;       /**< Expiration time in seconds */
} mod_persist_kvstore_t;

/** File system persistence configuration data */
typedef struct {
    ib_list_t  *kvstore_list;        /**< List of persist_fs_kvstore_t */
} mod_persist_cfg_t;
static mod_persist_cfg_t mod_persist_global_cfg;

/** Default expiration time of persisted collections (seconds) */
static const int default_expiration = 60;

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        persist
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();


/**
 * Handle managed collection register for persistent file system
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the collection
 * @param[in] uri Full collection URI
 * @param[in] uri_scheme URI scheme
 * @param[in] uri_data Hierarchical/data part of the URI (typically a path)
 * @param[in] params List of parameter strings
 * @param[in] register_data Selection callback data
 * @param[out] pmanager_inst_data Pointer to manager specific collection data
 *
 * @returns Status code:
 *   - IB_DECLINED Parameters not recognized
 *   - IB_OK All OK, parameters recognized
 *   - IB_Exxx Other error
 */
static ib_status_t mod_persist_register_fn(
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
    assert(mod_persist_param_data.key_pcre != NULL);

    const ib_list_node_t *node;
    const char *nodestr;
    const char *path;
    const char *key = NULL;
    bool key_expand;
    mod_persist_kvstore_t *persist;
    ib_kvstore_t *kvstore;
    ib_status_t rc;
    struct stat sbuf;
    const int ovecsize = 9;
    int ovector[ovecsize];
    int pcre_rc;
    ib_num_t expiration = default_expiration;

    if (ib_list_elements(params) < 1) {
        return IB_EINVAL;
    }
    path = ib_mpool_strdup(mp, uri_data);
    if (path == NULL) {
        return IB_EALLOC;
    }

    if (stat(path, &sbuf) < 0) {
        ib_log_warning(ib, "persist: Declining \"%s\"; stat(\"%s\") failed: %s",
                       uri, path, strerror(errno));
        return IB_DECLINED;
    }
    if (! S_ISDIR(sbuf.st_mode)) {
        ib_log_warning(ib,
                       "JSON file: Declining \"%s\"; \"%s\" is not a directory",
                       uri, path);
        return IB_DECLINED;
    }

    /* Extract the key name from the next param (only if it's key=<name>) */
    IB_LIST_LOOP_CONST(params, node) {
        nodestr = (const char *)node->data;
        const char *param;
        size_t      param_len;
        const char *value;
        size_t      value_len;

        pcre_rc = pcre_exec(mod_persist_param_data.key_pcre, NULL,
                            nodestr, strlen(nodestr),
                            0, 0, ovector, ovecsize);
        if (pcre_rc < 0) {
            return IB_DECLINED;
        }
        param     = nodestr + ovector[2];
        param_len = (ovector[3] - ovector[2]);
        value     = nodestr + ovector[4];
        value_len = ovector[5] - ovector[4];

        if ( (param_len == 3) && (strncasecmp(param, "key", 3) == 0) ) {
            key = ib_mpool_memdup_to_str(mp, value, value_len);
            if (key == NULL) {
                return IB_EALLOC;
            }
        }
        else if ( (param_len == 6) && (strncasecmp(param, "expire", 6) == 0) ) {
            rc = ib_string_to_num_ex(value, value_len, 0, &expiration);
            if (rc != IB_OK) {
                ib_log_error(ib, "Invalid expiration value \"%.*s\"",
                             (int)value_len, value);
                return rc;
            }
        }
    }
    if (key == NULL) {
        ib_log_error(ib, "No key specified");
    }

    rc = ib_data_expand_test_str(key, &key_expand);
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate and initialize a kvstore object */
    kvstore = ib_mpool_alloc(mp, sizeof(*kvstore));
    if (kvstore == NULL) {
        return IB_EALLOC;
    }
    rc = ib_kvstore_filesystem_init(kvstore, path);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_kvstore_connect(kvstore);
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate and initialize a PERSIST kvstore object */
    persist = ib_mpool_alloc(mp, sizeof(*persist));
    if (persist == NULL) {
        return IB_EALLOC;
    }
    persist->collection_name = ib_mpool_strdup(mp, collection_name);
    if (persist->collection_name == NULL) {
        return IB_EALLOC;
    }
    persist->path = path;
    persist->key = key;
    persist->key_expand = key_expand;
    persist->kvstore = kvstore;
    persist->expiration = expiration;

    /* Finally, store the list as the manager specific collection data */
    *pmanager_inst_data = persist;

    return IB_OK;
}

/**
 * Unregister callback for file-system persistent managed collections
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of the collection
 * @param[in] manager_inst_data Manager instance data
 * @param[in] unregister_data Register callback data
 *
 * @returns Status code:
 *   - IB_OK All OK
 *   - IB_Exxx Other error
 */
ib_status_t mod_persist_unregister_fn(
    const ib_engine_t              *ib,
    const ib_module_t              *module,
    const ib_collection_manager_t  *manager,
    const char                     *collection_name,
    void                           *manager_inst_data,
    void                           *unregister_data)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(collection_name != NULL);
    assert(manager_inst_data != NULL);

    ib_status_t rc;
    const mod_persist_kvstore_t *persist =
        (const mod_persist_kvstore_t *)manager_inst_data;

    rc = ib_kvstore_disconnect(persist->kvstore);
    ib_kvstore_destroy(persist->kvstore);

    return rc;
}

/**
 * Merge policy function that returns the most recent in the list
 * if the list is size 1 or greater.
 *
 * If the list size is 0, this does nothing.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] values Array of @ref ib_kvstore_value_t pointers.
 * @param[in] value_size The length of values.
 * @param[out] resultant_value Pointer to values[0] if value_size > 0.
 * @param[in,out] cbdata Context callback data.
 * @returns IB_OK
 */
static ib_status_t mod_persist_merge_fn(
    ib_kvstore_t *kvstore,
    ib_kvstore_value_t **values,
    size_t value_size,
    ib_kvstore_value_t **resultant_value,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);
    assert(values != NULL);
    ib_kvstore_value_t *result = NULL;
    size_t n;

    if (value_size == 1) {
        result = values[0];
        goto done;
    }
    else if (value_size == 0) {
        result = NULL;
        goto done;
    }

    /* Loop through the list, select the most recent. */
    for(n = 0;  n < value_size;  ++n) {
        const ib_kvstore_value_t *v = values[n];
        if ( (result == NULL) ||
             (ib_clock_timeval_cmp(&v->creation, &result->creation) > 0) )
        {
            result = values[n];
        }
    }

done:
    *resultant_value = result;
    return IB_OK;
}

/**
 * Handle managed collection kvstore / filesystem populate function
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to populate
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of the collection
 * @param[in,out] collection Collection to populate
 * @param[in] manager_inst_data Manager instance data
 * @param[in] populate_data Populate function callback data
 *
 * @returns Status code
 *   - IB_OK If no errors encountered
 *   - IB_DECLINED If the configured key was not found in the kvstore
 *   - Errors returned by ib_data_expand_str(), ib_kvstore_get(),
 *     ib_json_decode()
 *
 */
static ib_status_t mod_persist_populate_fn(
    const ib_engine_t              *ib,
    const ib_tx_t                  *tx,
    const ib_module_t              *module,
    const ib_collection_manager_t  *manager,
    const char                     *collection_name,
    ib_list_t                      *collection,
    void                           *manager_inst_data,
    void                           *populate_data)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(module != NULL);
    assert(collection_name != NULL);
    assert(collection != NULL);
    assert(manager_inst_data != NULL);

    const mod_persist_kvstore_t *persist =
        (const mod_persist_kvstore_t *)manager_inst_data;
    ib_kvstore_t *kvstore = persist->kvstore;
    ib_status_t rc = IB_OK;
    const char *key;
    const char *error = NULL;
    ib_kvstore_key_t kvstore_key;
    ib_kvstore_value_t *kvstore_val;

    /* Generate the key */
    if (persist->key_expand) {
        char *expanded;
        rc = ib_data_expand_str(tx->data, persist->key, false, &expanded);
        if (rc != IB_OK) {
            return rc;
        }
        key = expanded;
    }
    else {
        key = ib_mpool_strdup(tx->mp, persist->key);
    }

    /* Try to get data from the kvstore */
    kvstore_key.key = key;
    kvstore_key.length = strlen(key);
    rc = ib_kvstore_get(kvstore, mod_persist_merge_fn,
                        &kvstore_key, &kvstore_val);
    if (rc == IB_ENOENT) {
        return IB_DECLINED;
    }
    else if (rc != IB_OK) {
        return rc;
    }
    assert(kvstore_val != NULL);
    assert(kvstore_val->value != NULL);

    /* OK, got the data, now decode the JSON */
    rc = ib_json_decode_ex(tx->mp,
                           kvstore_val->value, kvstore_val->value_length,
                           collection, &error);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error decoding JSON for \"%s\" key \"%s\": \"%s\"",
                     collection_name, key,
                     error == NULL ? ib_status_to_string(rc) : error);
    }
    else {
        ib_log_debug(ib,
                     "Populated collection \"%s\" from kvstore \"%s\"",
                     collection_name, persist->path);
    }
    ib_kvstore_free_value(kvstore, kvstore_val);

    return rc;
}

/**
 * Handle managed collection kvstore / filesystem persist function
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to select a context for (or NULL)
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of the collection
 * @param[in] collection Collection to populate
 * @param[in] manager_inst_data Manager instance data
 * @param[in] persist_data Callback data
 *
 * @returns
 *   - IB_OK on success or when @a collection_data is length 0.
 *   - Errors returned by ib_data_expand_str(), ib_json_encode(),
 *     ib_kvstore_set()
 */
static ib_status_t mod_persist_persist_fn(
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

    const mod_persist_kvstore_t *persist =
        (const mod_persist_kvstore_t *)manager_inst_data;
    ib_kvstore_t *kvstore = persist->kvstore;
    ib_status_t rc = IB_OK;
    const char *key;
    ib_kvstore_key_t kvstore_key;
    ib_kvstore_value_t kvstore_val;
    char *buf;
    size_t bufsize;

    /* Generate the key */
    if (persist->key_expand) {
        char *expanded;
        rc = ib_data_expand_str(tx->data, persist->key, false, &expanded);
        if (rc != IB_OK) {
            return rc;
        }
        key = expanded;
    }
    else {
        key = ib_mpool_strdup(tx->mp, persist->key);
    }

    /* Encode the buffer into JSON */
    rc = ib_json_encode(tx->mp, collection, true, &buf, &bufsize);
    if (rc != IB_OK) {
        ib_log_warning(ib,
                       "Error encoding JSON for \"%s\" key \"%s\": \"%s\"",
                       collection_name, key, ib_status_to_string(rc));
        return rc;
    }

    /* Prepare the key / value for the kvstore */
    kvstore_key.key = key;
    kvstore_key.length = strlen(key);
    kvstore_val.value = buf;
    kvstore_val.value_length = bufsize;
    kvstore_val.type = ib_mpool_strdup(tx->mp, "json");
    kvstore_val.type_length = 4;
    kvstore_val.expiration = persist->expiration;

    /* Save the JSON buffer into the kvstore */
    rc = ib_kvstore_set(kvstore, NULL, &kvstore_key, &kvstore_val);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Initialize managed collection for simple name=value parameters
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
    ib_module_t *module,
    void *cbdata)
{
    assert(ib != NULL);
    assert(module != NULL);

    const char *key_pattern = "^(?i)(key|expire)=(.+)$";
    const int compile_flags = PCRE_DOTALL | PCRE_DOLLAR_ENDONLY;
    pcre *compiled;
    const char *error;
    int eoff;
    ib_status_t rc;
    const ib_collection_manager_t *manager;

    /* Register the name/value pair InitCollection handler */
    rc = ib_collection_manager_register(
        ib, module, "Filesystem K/V-Store", "persist-fs://",
        mod_persist_register_fn, NULL,
        mod_persist_unregister_fn, NULL,
        mod_persist_populate_fn, NULL,
        mod_persist_persist_fn, NULL,
        &manager);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to register filesystem persistence handler: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Compile the patterns */
    compiled = pcre_compile(key_pattern, compile_flags, &error, &eoff, NULL);
    if (compiled == NULL) {
        ib_log_error(ib, "Failed to compile pattern \"%s\"", key_pattern);
        return IB_EUNKNOWN;
    }
    mod_persist_param_data.key_pcre = compiled;
    mod_persist_param_data.manager = manager;

    return IB_OK;
}

static ib_status_t mod_persist_fini(ib_engine_t *ib,
                                    ib_module_t *m,
                                    void *cbdata)
{
    if (mod_persist_param_data.key_pcre != NULL) {
        pcre_free((pcre *)mod_persist_param_data.key_pcre);
    }

    return IB_OK;
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,             /* Default metadata */
    MODULE_NAME_STR,                       /* Module name */
    IB_MODULE_CONFIG(&mod_persist_global_cfg), /* Global config data */
    NULL,                                  /* Configuration field map */
    NULL,                                  /* Config directive map */
    mod_persist_init,                      /* Initialize function */
    NULL,                                  /* Callback data */
    mod_persist_fini,                      /* Finish function */
    NULL,                                  /* Callback data */
    NULL,                                  /* Context open function */
    NULL,                                  /* Callback data */
    NULL,                                  /* Context close function */
    NULL,                                  /* Callback data */
    NULL,                                  /* Context destroy function */
    NULL                                   /* Callback data */
);
