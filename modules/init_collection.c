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

#include <ironbee_config_auto.h>

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/module.h>
#include <ironbee/path.h>
#if ENABLE_JSON
#include <ironbee/json.h>
#endif
#include <ironbee/uuid.h>

#include <persistence_framework.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


/* Module boiler plate */
#define MODULE_NAME init_collection
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)
IB_MODULE_DECLARE();

static const char *JSON_TYPE = "json";
static const char *VAR_TYPE  = "var";

/**
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the file is not found.
 * - IB_EALLOC On allocation errror.
 * - IB_EOTHER On an unexpected read error.
 */
static ib_status_t read_file(
    ib_mpool_t  *mp,
    const char  *file,
    const void **out,
    size_t      *sz
)
{
    assert(mp != NULL);
    assert(file != NULL);
    assert(out != NULL);

    int     fd    = open(file, O_RDONLY);
    size_t  bufsz = 1024; /* Size of buffer. */
    size_t  fill  = 0;     /* Total bytes in buf. */
    void   *buf   = ib_mpool_alloc(mp, bufsz);

    if (buf == NULL) {
        return IB_EALLOC;
    }

    if (fd == -1) {
        return IB_ENOENT;
    }

    for (;;) {
        ssize_t r = read(fd, buf + fill, bufsz - fill);

        /* EOF. */
        if (r == 0) {
            *sz = fill;
            *out = buf;
            return IB_OK;
        }

        /* Read error. */
        if (r < 0) {
            return IB_EOTHER;
        }

        /* We read something. */
        fill += r;

        /* Buffer is full. Resize. */
        if (fill == bufsz) {
            size_t  new_bufsz = 2 * bufsz;
            char   *new_buf   = ib_mpool_alloc(mp, new_bufsz);
            if (new_buf == NULL) {
                return IB_EALLOC;
            }
            memcpy(new_buf, buf, bufsz);
            buf   = new_buf;
            bufsz = new_bufsz;
        }
    }

    /* Technically unreachable code. */
    assert(0 && "Unreachable code.");
}

/**
 * Module configuration.
 */
struct init_collection_cfg_t {
    ib_pstnsfw_t *pstnsfw; /**< Module configuration. */
    ib_module_t  *module;  /**< Our module structure at init time. */
    /**
     * The current configuration value.
     *
     * This pointer is a value-passing field and is changed often
     * during configuration time. It is idle at runtime.
     */
    const char   *config_file;
};
typedef struct init_collection_cfg_t init_collection_cfg_t;

#ifdef ENABLE_JSON
/**
 * JSON configuration type.
 */
struct json_t {
    const char *file;
};
typedef struct json_t json_t;
#endif
#ifdef ENABLE_JSON
static ib_status_t json_load_fn(
        void       *impl,
        ib_tx_t    *tx,
        const char *key,
        ib_list_t  *fields,
        void       *cbdata
)
{
    assert(impl != NULL);
    assert(tx != NULL);
    assert(tx->mp != NULL);

    json_t      *json_cfg = (json_t *)impl;
    ib_status_t  rc;
    const char  *err_msg;
    const void  *buf = NULL;
    size_t       sz;

    /* Load the file into a buffer. */
    rc = read_file(tx->mp, json_cfg->file, &buf, &sz);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Failed to read JSON file %s", json_cfg->file);
        return rc;
    }

    /* Parse the buffer into the fields list. */
    rc = ib_json_decode_ex(tx->mp, buf, sz, fields, &err_msg);
    if (rc != IB_OK) {
        ib_log_error_tx(
            tx,
            "Faile to decode JSON file %s: %s",
            json_cfg->file,
            err_msg);
        return rc;
    }

    return IB_OK;
}
#endif
#ifdef ENABLE_JSON
static ib_status_t json_create_fn(
    ib_engine_t      *ib,
    const ib_list_t  *params,
    void            **impl,
    void             *cbdata
)
{
    assert(ib != NULL);
    assert(params != NULL);
    assert(impl != NULL);
    assert(cbdata != NULL);

    ib_mpool_t            *mp = ib_engine_pool_main_get(ib);
    json_t                *json_cfg;
    const ib_list_node_t  *node;
    const char            *json_file;
    init_collection_cfg_t *cfg = (init_collection_cfg_t *)cbdata;

    assert(cfg->config_file != NULL);

    json_cfg = ib_mpool_alloc(mp, sizeof(*json_cfg));
    if (json_cfg == NULL) {
        ib_log_error(ib, "Failed to allocate JSON configuration.");
        return IB_EALLOC;
    }

    /* Get the collection node. We don't care about this. Skip it. */
    node = ib_list_first_const(params);
    if (node == NULL) {
        ib_log_error(ib, "JSON requires at least 2 arguments: name and uri.");
        return IB_EINVAL;
    }

    /* Get the URI to the file. */
    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_log_error(ib, "JSON requires at least 2 arguments: name and uri.");
        return IB_EINVAL;
    }
    json_file = (const char *)ib_list_node_data_const(node);

    json_cfg->file = ib_util_relative_file(mp, cfg->config_file, json_file);
    if (json_cfg->file == NULL) {
        ib_log_error(ib, "Failed to allocate file.");
        return IB_EALLOC;
    }

    *impl = json_cfg;
    return IB_OK;
}
#endif

/**
 * Var implemintation data.
 */
struct var_t {
    const ib_list_t *fields;
};
typedef struct var_t var_t;
static ib_status_t var_create_fn(
    ib_engine_t      *ib,
    const ib_list_t  *params,
    void            **impl,
    void             *cbdata
)
{
    assert(ib != NULL);
    assert(params != NULL);
    assert(impl != NULL);
    assert(cbdata != NULL);

    ib_mpool_t            *mp = ib_engine_pool_main_get(ib);
    var_t                 *var;
    ib_list_t             *fields;
    const ib_list_node_t  *node;
    ib_status_t            rc;

    var = ib_mpool_alloc(mp, sizeof(*var));
    if (var == NULL) {
        ib_log_error(ib, "Failed to allocate JSON configuration.");
        return IB_EALLOC;
    }

    rc = ib_list_create(&fields, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create field list.");
        return rc;
    }

    /* Skip the collection name. */
    node = ib_list_first_const(params);
    if (node == NULL) {
        ib_log_error(ib, "JSON requires at least 2 arguments: name and uri.");
        return IB_EINVAL;
    }

    /* Skip the URI. */
    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_log_error(ib, "JSON requires at least 2 arguments: name and uri.");
        return IB_EINVAL;
    }

    /* For the rest of the nodes... */
    for (; node != NULL; node = ib_list_node_next_const(node)) {
        const char *assignment =
            (const char *)ib_list_node_data_const(node);
        const char *eqsign = index(assignment, '=');
        ib_field_t *field = NULL;

        /* Assume an empty assignment if no equal sign is included. */
        if (eqsign == NULL) {
            /* The whole assignment is just a variable name. */
            rc = ib_field_create(
                &field,
                mp,
                assignment,
                strlen(assignment),
                IB_FTYPE_NULSTR,
                ib_ftype_nulstr_in(""));
        }
        else if (*(eqsign + 1) == '\0') {
            /* The assignment is a var name + '='. */
            rc = ib_field_create(
                &field,
                mp,
                assignment,
                strlen(assignment) - 1, /* -1 drops the '=' from the name. */
                IB_FTYPE_NULSTR,
                ib_ftype_nulstr_in(""));
        }
        else {
            /* Normal assignment. eqsign is the end of the name.
             * eqsign + 1 is the start of the assigned value. */
            rc = ib_field_create(
                &field,
                mp,
                assignment,
                eqsign - assignment,
                IB_FTYPE_NULSTR,
                ib_ftype_nulstr_in(eqsign+1));
        }

        /* Check the field creation in the above if-ifelse-else. */
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to create field for assignment %s",
                assignment);
            return IB_EALLOC;
        }

        /* Make sure we didn't somehow forget to set the field. */
        assert(field != NULL);

        rc = ib_list_push(fields, field);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to push field onto field list.");
            return rc;
        }
    }

    var->fields = fields;
    *impl = var;
    return IB_OK;
}

static ib_status_t var_load_fn(
        void       *impl,
        ib_tx_t    *tx,
        const char *key,
        ib_list_t  *fields,
        void       *cbdata
)
{
    assert(impl != NULL);
    assert(tx != NULL);
    assert(tx->mp != NULL);

    var_t *var = (var_t *)impl;
    const ib_list_node_t *node;

    assert(var->fields != NULL);

    IB_LIST_LOOP_CONST(var->fields, node) {
        ib_status_t rc;
        const ib_field_t *field =
            (const ib_field_t *)ib_list_node_data_const(node);
        rc = ib_list_push(fields, (void *)field);
        if (rc !=  IB_OK) {
            ib_log_error_tx(tx, "Failed to populate fields.");
            return rc;
        }
    }

    return IB_OK;
}

/**
 * @params[in] params List of parameters.
 *                    The first element is the name of the collection.
 *                    The second element is the URI.
 *                    The rest are options.
 */
static ib_status_t domap(
    ib_cfgparser_t        *cp,
    ib_context_t          *ctx,
    const char            *type,
    init_collection_cfg_t *cfg,
    const char            *collection_name,
    const ib_list_t       *params
)
{
    ib_uuid_t uuid;
    char store_name[37];
    ib_status_t rc;

    rc = ib_uuid_create_v4(&uuid);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create UUIDv4 store.");
        return rc;
    }

    rc = ib_uuid_bin_to_ascii(store_name, &uuid);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to convert UUIDv4 to string.");
        return rc;
    }

    rc = ib_pstnsfw_create_store(
        cfg->pstnsfw,
        ctx,
        type,
        store_name,
        params);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create store %s", store_name);
        return rc;
    }

    rc = ib_pstnsfw_map_collection(
        cfg->pstnsfw,
        ctx,
        collection_name,
        "no key",
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
 * vars: key1=val1 key2=val2 ... keyN=valN
 *
 * The vars URI allows initializing a collection of simple key/value pairs.
 *
 * InitCollection MY_VARS vars: key1=value1 key2=value2
 * json-file:///path/file.json [persist]
 *
 * The json-file URI allows loading a more complex collection from a JSON
 * formatted file. If the optional persist parameter is specified, then
 * anything changed is persisted back to the file at the end of the
 * transaction. Next time the collection is initialized, it will be from
 * the persisted data.
 *
 * InitCollection MY_JSON_COLLECTION json-file:///tmp/ironbee/persist/test1.json
 *
 * InitCollection MY_PERSISTED_JSON_COLLECTION json-file:///tmp/ironbee/persist/test2.json persist
 */
static ib_status_t init_collection_common(
    ib_cfgparser_t *cp,
    const char *directive,
    const ib_list_t *vars,
    init_collection_cfg_t *cfg,
    bool indexed
)
{
    assert(cp != NULL);
    assert(directive != NULL);
    assert(vars != NULL);
    assert(cfg != NULL);
    assert(cfg->module != NULL);
    assert(cfg->pstnsfw != NULL);

    ib_status_t            rc;
    const ib_list_node_t  *node;
    const char            *name;   /* The name of the collection. */
    const char            *uri;    /* The URI to the resource. */
    ib_context_t          *ctx;

    /* Set the configuration file before doing much else. */
    cfg->config_file = ib_cfgparser_curr_file(cp);

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve current config context.");
        goto exit_rc;
    }

    /* Get the collection name string */
    node = ib_list_first_const(vars);
    if (node == NULL) {
        ib_cfg_log_error(cp, " %s: No collection name specified", directive);
        goto exit_EINVAL;
    }
    name = (const char *)ib_list_node_data_const(node);

    /* Get the collection uri. */
    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_cfg_log_error(cp, " %s: No collection URI specified", directive);
        goto exit_EINVAL;
    }
    uri = (const char *)ib_list_node_data_const(node);

    if (strncmp(uri, "vars:", sizeof("vars:")) == 0) {
        rc = domap(cp, ctx, VAR_TYPE, cfg, name, vars);
        if (rc != IB_OK) {
            goto exit_rc;
        }
    }
#ifdef ENABLE_JSON
    else if (strncmp(uri, "json-file:", sizeof("json-file:")) == 0) {
        rc = domap(cp, ctx, JSON_TYPE, cfg, name, vars);
        if (rc != IB_OK) {
            goto exit_rc;
        }
    }
#endif
    else {
        ib_cfg_log_error(cp, "URI %s not supported for persitence.", uri);
        goto exit_EINVAL;
    }

    if (indexed) {
        rc = ib_data_register_indexed(
            ib_engine_data_config_get(cp->ib),
            name);
        if (rc != IB_OK) {
            ib_cfg_log_error(
                cp,
                "Failed to index collection %s: %s",
                name,
                ib_status_to_string(rc));
        }
    }

    /* Clear the configuration file to expose errors. */
    cfg->config_file = NULL;
    return IB_OK;
exit_rc:
    cfg->config_file = NULL;
    return rc;
exit_EINVAL:
    cfg->config_file = NULL;
    return IB_EINVAL;
}

static ib_status_t init_collection(
    ib_cfgparser_t *cp,
    const char *directive,
    const ib_list_t *vars,
    void *cbdata
)
{
    return init_collection_common(
        cp,
        directive,
        vars,
        (init_collection_cfg_t *)cbdata,
        false);
}

static ib_status_t init_collection_indexed(
    ib_cfgparser_t *cp,
    const char *directive,
    const ib_list_t *vars,
    void *cbdata
)
{
    return init_collection_common(
        cp,
        directive,
        vars,
        (init_collection_cfg_t *)cbdata,
        true);
}

/**
 * Register directives dynamically so as to define a callback data struct.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cbdata The module global configuration.
 * @returns
 * - IB_OK On Success.
 * - Other on failure of ib_config_register_directives().
 */
static ib_status_t register_directives(
    ib_engine_t *ib,
    init_collection_cfg_t *cbdata)
{
    assert(ib != NULL);
    assert(cbdata != NULL);

    IB_DIRMAP_INIT_STRUCTURE(dirmap) = {
        IB_DIRMAP_INIT_LIST(
            "InitCollection",
            init_collection,
            cbdata
        ),
        IB_DIRMAP_INIT_LIST(
            "InitCollectionIndexed",
            init_collection_indexed,
            cbdata
        ),
        IB_DIRMAP_INIT_LAST
    };

    return ib_config_register_directives(ib, dirmap);
}

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
    assert(ib != NULL);
    assert(module != NULL);

    ib_status_t            rc;
    ib_mpool_t            *mp = ib_engine_pool_main_get(ib);
    init_collection_cfg_t *cfg;

    cfg = ib_mpool_alloc(mp, sizeof(*cfg));
    if (cfg == NULL) {
        ib_log_error(ib, "Failed to allocate module configuration struct.");
        return IB_EALLOC;
    }

    cfg->pstnsfw = NULL;

    rc = ib_pstnsfw_create(ib, module, &(cfg->pstnsfw));
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to register module "
            MODULE_NAME_STR
            " with persistence module.");
        return rc;
    }

    cfg->module = module;

    rc = register_directives(ib, cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to dynamically register directives.");
        return rc;
    }

    rc = ib_pstnsfw_register_type(
        cfg->pstnsfw,
        ib_context_main(ib),
        VAR_TYPE,
        var_create_fn,
        NULL,
        NULL,
        NULL,
        var_load_fn,
        NULL,
        NULL,
        NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register var type.");
        return rc;
    }

#if ENABLE_JSON
    rc = ib_pstnsfw_register_type(
        cfg->pstnsfw,
        ib_context_main(ib),
        JSON_TYPE,
        json_create_fn,
        cfg,
        NULL,
        NULL,
        json_load_fn,
        NULL,
        NULL,
        NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register json type.");
        return rc;
    }
#endif

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
    IB_MODULE_CONFIG_NULL,        /* NULL Configuration: NULL, 0, NULL, NULL */
    NULL,                         /* Config map. */
    NULL,                         /* Directive map. Dynamically built. */
    init_collection_init,         /* Initialization. */
    NULL,                         /* Callback data. */
    init_collection_fini,         /* Finalization. */
    NULL,                         /* Callback data. */
);
