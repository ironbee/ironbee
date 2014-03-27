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

#include "persistence_framework.h"

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/json.h>
#include <ironbee/kvstore.h>
#include <ironbee/kvstore_filesystem.h>
#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/module.h>
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
static const ib_time_t DEFAULT_EXPIRATION = 60LU * 1000000LU;

static const char FILE_URI_PREFIX[] = "persist-fs://";
static const char JSON_TYPE[] = "application_json";

/* Define the module name as well as a string version of it. */
#define MODULE_NAME persist
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

//! Module configuration.
struct persist_cfg_t {
    ib_persist_fw_t *persist_fw; /**< Handle to the persistence framework. */
};
typedef struct persist_cfg_t persist_cfg_t;


//! File store type.
static const char FILE_TYPE[] = "filerw";

/**
 * Implementation instance data of a file read-write.
 */
struct file_rw_t {
    ib_kvstore_t *kvstore;
    ib_engine_t  *ib;
    const char   *key;
    size_t        keysz;
};
typedef struct file_rw_t file_rw_t;

/**
 * Return a pointer to the configuration value if @a opt is prefixed with
 * @a config.
 *
 * The value is not found then NULL is returned.
 *
 * @param[in] config The configuration value.
 * @param[in] opt The option string which contains the value.
 *
 * @returns Pointer to configuration value or NULL.
 */
static const char * get_val(const char *config, const char *opt)
{
    assert(config != NULL);
    assert(opt != NULL);

    size_t s = strlen(config);

    if (strncmp(config, opt, s) == 0) {
        return opt + s;
    }

    return NULL;
}

/**
 * Create a new store and store it in @a impl.
 *
 * @param[in] ib IronBee engine.
 * @param[in] params Parameters list. The first is element is ignored.
 *            The second is the URI and after that are options.
 * @param[in] impl A @ref file_rw_t is put here.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static
ib_status_t file_rw_create_fn(
    ib_engine_t     *ib,
    const ib_list_t *params,
    void            *impl,
    void            *cbdata
)
{
    assert(ib != NULL);
    assert(params != NULL);
    assert(impl != NULL);

    ib_mm_t               mm = ib_engine_mm_main_get(ib);
    const ib_list_node_t *node;
    const char           *uri;
    file_rw_t            *file_rw;
    ib_status_t           rc;

    file_rw = ib_mm_calloc(mm, 1, sizeof(*file_rw));
    if (file_rw == NULL) {
        return IB_EALLOC;
    }

    file_rw->ib = ib;

    node = ib_list_first_const(params);
    if (node == NULL) {
        ib_log_error(ib, "Missing first parameter.");
        return IB_EINVAL;
    }

    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_log_error(ib, "Missing uri parameter.");
        return IB_EINVAL;
    }
    uri = (const char *)ib_list_node_data_const(node);
    if (uri == NULL) {
        ib_log_error(ib, "Missing uri parameter.");
        return IB_EINVAL;
    }

    node = ib_list_node_next_const(node);
    for ( ; node != NULL; node = ib_list_node_next_const(node)) {
        const char *opt = (const char *)ib_list_node_data_const(node);
        const char *val;

        val = get_val("key=", opt);
        if (val != NULL) {
            file_rw->keysz = strlen(val);
            file_rw->key = ib_mm_memdup(mm, val, file_rw->keysz);
            if (file_rw->key == NULL) {
                ib_log_warning(ib, "Failed to copy key.");
                return IB_EALLOC;
            }
        }
    }

    file_rw->kvstore = ib_mm_alloc(mm, ib_kvstore_size());

    if (strncmp(uri, FILE_URI_PREFIX, sizeof(FILE_URI_PREFIX)-1) == 0) {
        const char *dir = uri + sizeof(FILE_URI_PREFIX)-1;
        ib_log_debug(ib, "Creating key-value store in directory: %s", dir);

        rc = ib_kvstore_filesystem_init(file_rw->kvstore, dir);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to initialize kvstore.");
            return rc;
        }

        rc = ib_kvstore_connect(file_rw->kvstore);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to connect to kvstore.");
            return rc;
        }
    }
    else {
        ib_log_error(ib, "Unsupported URI: %s", uri);
        return IB_EINVAL;
    }

    *(file_rw_t **)impl = file_rw;
    return IB_OK;
}

static void file_rw_destroy_fn(
    void *impl,
    void *cbdata
)
{
    assert(impl != NULL);

    file_rw_t *file_rw = (file_rw_t *)impl;

    assert(file_rw->kvstore != NULL);

    ib_kvstore_disconnect(file_rw->kvstore);

    ib_kvstore_destroy(file_rw->kvstore);
}

static ib_status_t file_rw_load_fn(
    void       *impl,
    ib_tx_t    *tx,
    const char *key,
    size_t      key_len,
    ib_list_t  *list,
    void       *cbdata
)
{
    assert(impl != NULL);

    file_rw_t          *file_rw = (file_rw_t *)impl;
    ib_engine_t        *ib = file_rw->ib;
    ib_status_t         rc;
    ib_kvstore_key_t    kv_key;
    ib_kvstore_value_t *kv_val;
    const uint8_t      *value;
    size_t              value_length;
    const char         *type;
    size_t              type_length;

    assert(file_rw->kvstore != NULL);
    assert(ib != NULL);

    kv_key.key = key;
    kv_key.length = key_len;

    /* Get the data. */
    rc = ib_kvstore_get(
        file_rw->kvstore,
        NULL,
        &kv_key,
        &kv_val);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to retrieve key-value for key \"%.*s\".",
            (int),key_len,
            key);
        return rc;
    }

    ib_kvstore_value_type_get(kv_val, &type, &type_length);
    ib_kvstore_value_value_get(kv_val, &value, &value_length);

    /* Deserialize the data. */
    if (  sizeof(JSON_TYPE)-1 == type_length
       && strncmp(JSON_TYPE, type, type_length) == 0)
    {

        /* Deserialize JSON. */
        const char *err_msg;
        ib_mm_t mm = ib_engine_mm_main_get(ib);

        rc = ib_json_decode_ex(
            mm,
            value,
            value_length,
            list,
            &err_msg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error decoding stored JSON: %s", err_msg);
            ib_kvstore_value_destroy(kv_val);
            return rc;
        }
    }
    else {
        ib_log_error(
            ib,
            "Unsupported type encoding: %.*s.",
            (int)type_length,
            type);
        ib_kvstore_value_destroy(kv_val);
        return IB_EOTHER;
    }

    ib_kvstore_value_destroy(kv_val);
    return IB_OK;
}

static ib_status_t file_rw_store_fn(
    void            *impl,
    ib_tx_t         *tx,
    const char      *key,
    size_t           key_len,
    ib_time_t        expiration,
    const ib_list_t *list,
    void            *cbdata
)
{
    assert(impl != NULL);

    file_rw_t          *file_rw = (file_rw_t *)impl;
    ib_engine_t        *ib = file_rw->ib;
    ib_mm_t             mm = ib_engine_mm_main_get(ib);
    ib_status_t         rc;
    ib_kvstore_key_t    kv_key;
    ib_kvstore_value_t *kv_val;
    const uint8_t      *data;
    size_t              dlen;
    ib_time_t           creation = ib_clock_get_time();

    assert(file_rw->kvstore != NULL);

    rc = ib_json_encode(
        mm,
        list,
        true,
        (char **)&data,
        &dlen);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to encode json.");
        return rc;
    }

    kv_key.key = key;
    kv_key.length = key_len;

    rc = ib_kvstore_value_create(&kv_val);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create kvstore value.");
        return rc;
    }

    ib_kvstore_value_value_set(kv_val, data, dlen);
    ib_kvstore_value_type_set(kv_val, JSON_TYPE, sizeof(JSON_TYPE)-1);
    ib_kvstore_value_creation_set(kv_val, creation);
    ib_kvstore_value_expiration_set(kv_val, expiration + creation);

    rc = ib_kvstore_set(file_rw->kvstore, NULL, &kv_key, kv_val);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to store key-value \"%.*s\".",
            (int)kv_key.length,
            (const char *)kv_key.key);
        return rc;
    }

    return IB_OK;
}

/**
 * Create a persistence store that can be used to map a collection.
 */
static ib_status_t persistence_create_store_fn(
    ib_cfgparser_t  *cp,
    const char      *directive,
    const ib_list_t *vars,
    void            *cbdata
)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(directive != NULL);
    assert(vars != NULL);
    assert(cbdata != NULL);

    persist_cfg_t        *cfg = (persist_cfg_t *)cbdata;
    ib_engine_t          *ib = cp->ib;
    const char           *store_name;
    const char           *store_uri;
    ib_context_t         *ctx = NULL;
    const ib_list_node_t *node;
    ib_status_t           rc;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve configuration context.");
        return rc;
    }

    /* Check / extract first configuration parameter, the store name. */
    node = ib_list_first_const(vars);
    if (node == NULL) {
        ib_log_error(ib, "Store name missing from configuration.");
        return IB_EINVAL;
    }
    store_name = (const char *)ib_list_node_data_const(node);
    if (store_name == NULL) {
        ib_log_error(ib, "Store name was NULL.");
        return IB_EINVAL;
    }

    /* Validate that there is a second, required, parameter. */
    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_log_error(ib, "No URI for store %s.", store_name);
        return IB_EINVAL;
    }
    store_uri = (const char *)ib_list_node_data_const(node);
    if (store_uri == NULL) {
        ib_log_error(ib, "No URI for store %s.", store_name);
        return IB_EINVAL;
    }

    rc = ib_persist_fw_create_store(
        cfg->persist_fw,
        ctx,
        FILE_TYPE,
        store_name,
        vars);
    if (rc != IB_OK) {
        return rc;
    }
    return IB_OK;
}

/**
 * Create a store using a random UUID.
 *
 * This name is not given to the user, so it is considered an anonymous store.
 *
 * @param[in] cp Configuration parser.
 * @param[in] ctx Configuration context.
 * @param[in] cfg Module configuration.
 * @param[in] vars Parameter list.
 * @param[out] name The UUID of the store if it was created successfully.
 *             This value is a null-terminated string.
 *             This value is not set unless IB_OK is returned.
 * @returns
 *  - IB_OK On success.
 *  - IB_EALLOC Memory error.
 *  - Other on error.
 */
static ib_status_t create_anonymous_store(
    ib_cfgparser_t   *cp,
    ib_context_t     *ctx,
    persist_cfg_t    *cfg,
    const ib_list_t  *vars,
    const char      **name
)
{
    assert(cp != NULL);
    assert(ctx != NULL);
    assert(cfg != NULL);
    assert(name != NULL);

    ib_mm_t     mm         = cp->mm;
    char       *store_name = ib_mm_alloc(mm, IB_UUID_LENGTH);
    ib_status_t rc;

    if (store_name == NULL) {
        return IB_EALLOC;
    }

    /* Build random store name. */
    rc = ib_uuid_create_v4(store_name);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create UUIDv4 store.");
        return rc;
    }

    rc = ib_persist_fw_create_store(
        cfg->persist_fw,
        ctx,
        FILE_TYPE,
        store_name,
        vars);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to create anonymous store from %s.",
            store_name);
        return rc;
    }

    *name = store_name;
    return IB_OK;
}

/**
 * Map a previously created persistence store to a collection.
 *
 * param[in] cp Configuration parser.
 * param[in] directive This is PersistenceMap.
 * param[in] vars Parameters.
 * param[in] cbdata Callback data. This is a @ref persist_cfg_t.
 *
 * @returns
 *  - IB_OK On success.
 *  - Other on error.
 */
static ib_status_t persistence_map_fn(
    ib_cfgparser_t  *cp,
    const char      *directive,
    const ib_list_t *vars,
    void            *cbdata
)
{
    assert(cp != NULL);
    assert(directive != NULL);
    assert(vars != NULL);

    const char           *store_name;
    const char           *collection_name;
    const char           *key = NULL;
    ib_status_t           rc;
    const ib_list_node_t *node;
    ib_context_t         *ctx;
    persist_cfg_t        *cfg = (persist_cfg_t *)cbdata;
    ib_num_t              expire;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve configuration context.");
        return rc;
    }

    /* Get the parameters collection_name, store_name, and key. */
    node = ib_list_first_const(vars);
    if (node == NULL) {
        ib_cfg_log_error(cp, "Collection name missing.");
        return IB_EINVAL;
    }
    collection_name = (const char *)ib_list_node_data_const(node);
    if (collection_name == NULL) {
        ib_cfg_log_error(cp, "Collection name missing.");
        return IB_EINVAL;
    }

    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_cfg_log_error(cp, "Store name missing.");
        return IB_EINVAL;
    }
    store_name = (const char *)ib_list_node_data_const(node);
    if (collection_name == NULL) {
        ib_cfg_log_error(cp, "Store name missing.");
        return IB_EINVAL;
    }

    /* The default key value is the collection name. */
    key = collection_name;

    /* Go to the next element. */
    node = ib_list_node_next_const(node);
    for ( ; node != NULL; node = ib_list_node_next_const(node)) {
        /* Grab other parameters. */
        const char *config_str = (const char *)ib_list_node_data_const(node);
        const char *tmp_str = NULL;

        /* Try to get key configuration. */
        tmp_str = get_val("key=", config_str);
        if (tmp_str != NULL) {
            ib_mm_t mm = cp->mm;
            key = ib_mm_strdup(mm, tmp_str);
            continue;
        }

        tmp_str = get_val("expire=", config_str);
        if (tmp_str != NULL) {
            rc = ib_string_to_num(tmp_str, 10, &expire);
            if (rc != IB_OK) {
                ib_cfg_log_warning(
                    cp,
                    "Failed to parse expiration value %s.",
                    tmp_str);
            }
            continue;
        }

        ib_cfg_log_warning(
            cp,
            "Unsupported configuration option for directive %s: %s",
            directive,
            config_str);
    }

    /* Attempt a simple mapping, assuming store_name exists. */
    rc = ib_persist_fw_map_collection(
        cfg->persist_fw,
        ctx,
        collection_name,
        IB_S2SL(key == NULL ? "" : key),
        expire, /* Expiration in seconds. */
        store_name);
    /* Exit on success or a non-IB_ENOENT error. */
    if (rc != IB_ENOENT) {
        return rc;
    }

    /* If we reach this, the store was not found. Perhaps the store name
     * is really a URI? */
    ib_cfg_log_debug(
        cp,
        "Store %s does not exist. "
        "Attempting to create an anonymous store using the name as a URI.",
        store_name);

    /* Try to make an anonymous store (use a UUID as the name). */
    rc = create_anonymous_store(cp, ctx, cfg, vars, &store_name);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to create anonymous store for %s.",
            store_name);
        return rc;
    }

    /* Try to map the against the store. */
    rc = ib_persist_fw_map_collection(
        cfg->persist_fw,
        ctx,
        collection_name,
        IB_S2SL(key == NULL ? "" : key),
        expire, /* Expiration in seconds. */
        store_name);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to map store %s to collection %s.",
            store_name,
            collection_name);
        return rc;
    }

    return IB_OK;
}

/**
 * Register directives so as to define a callback data struct.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg The module configuration.
 *
 * @returns
 *  - IB_OK On Success.
 *  - Other on failure of ib_config_register_directives().
 */
static ib_status_t register_directives(
    ib_engine_t   *ib,
    persist_cfg_t *cfg)
{
    assert(ib != NULL);
    assert(cfg != NULL);

    ib_status_t rc;

    rc = ib_config_register_directive(
        ib,
        "PersistenceStore",
        IB_DIRTYPE_LIST,
        (ib_void_fn_t)persistence_create_store_fn,
        NULL,
        cfg,
        NULL,
        NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_config_register_directive(
        ib,
        "PersistenceMap",
        IB_DIRTYPE_LIST,
        (ib_void_fn_t)persistence_map_fn,
        NULL,
        cfg,
        NULL,
        NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Initialize persist managed collection module
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] cbdata Callback data
 *
 * @returns Status code:
 * - IB_OK All OK, parameters recognized
 * - IB_Exxx Other error
 */
static ib_status_t mod_persist_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata)
{
    assert(ib != NULL);
    assert(module != NULL);

    persist_cfg_t *cfg        = NULL;
    ib_mm_t        mm         = ib_engine_mm_main_get(ib);
    ib_status_t    rc;

    cfg = ib_mm_alloc(mm, sizeof(*cfg));
    if (cfg == NULL) {
        return IB_EALLOC;
    }

    /* Get a handle to the persistence framework. */
    cfg->persist_fw = NULL;
    rc = ib_persist_fw_create(ib, module, &(cfg->persist_fw));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create persistence handle.");
        return rc;
    }

    rc = ib_persist_fw_register_type(
        cfg->persist_fw,
        ib_context_main(ib),
        FILE_TYPE,
        file_rw_create_fn,
        NULL,
        file_rw_destroy_fn,
        NULL,
        file_rw_load_fn,
        NULL,
        file_rw_store_fn,
        NULL
    );
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register file type.");
        return rc;
    }

    rc = register_directives(ib, cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register directive.");
        return rc;
    }

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
    NULL,                                  /* Finish function */
    NULL,                                  /* Callback data */
);
