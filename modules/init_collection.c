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
 * @brief IronBee --- Init Collection Module
 *
 * This module provides the InitCollection directive.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "persistence_framework.h"

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/file.h>
#if ENABLE_JSON
#include <ironbee/json.h>
#endif
#include <ironbee/module.h>
#include <ironbee/path.h>
#include <ironbee/string.h>
#include <ironbee/transformation.h>
#include <ironbee/uuid.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Module boilerplate */
#define MODULE_NAME init_collection
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)
IB_MODULE_DECLARE();

/* JSON handlers are registered under this type. */
#define JSON_TYPE "json"

/* JSON URI Prefix. */
#define JSON_URI_PREFIX "json-file://"

/* VAR handlers are registered under this type. */
#define VAR_TYPE "var"

/**
 * Module configuration.
 */
struct init_collection_cfg_t {
    ib_persist_fw_t *persist_fw; /**< Handle to the persistence framework. */

    /**
     * The current configuration value.
     *
     * This pointer is a value-passing field and is changed often
     * during configuration time. It is used by the JSON
     * support code to find JSON files relative to the current
     * configuration file. This field is idle at runtime.
     */
    const char   *config_file;
};
typedef struct init_collection_cfg_t init_collection_cfg_t;

/* All JSON-related static functions and types are located here.
 * Do not move JSON code outside of the #if or builds disabling
 * JSON will probably fail. */
#if ENABLE_JSON

/**
 * JSON configuration type.
 */
struct json_t {
    const char *file; /**< The file containing the JSON. */
};
typedef struct json_t json_t;

/**
 * JSON Load callback.
 *
 * @param[in] impl The implementation created by json_create_fn().
 * @param[in] tx The transaction.
 * @param[in] key Unused.
 * @param[in] key_len Unused.
 * @param[in] fields The output fields.
 * @param[in] cbdata Callback data. Unused.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
static ib_status_t json_load_fn(
    void       *impl,
    ib_tx_t    *tx,
    const char *key,
    size_t      key_len,
    ib_list_t  *fields,
    void       *cbdata
)
{
    assert(impl != NULL);
    assert(tx != NULL);
    assert(tx->mp != NULL);
    assert(fields != NULL);

    json_t        *json_cfg = (json_t *)impl;
    ib_status_t    rc;
    const char    *err_msg;
    const uint8_t *buf = NULL;
    size_t         sz;

    /* Load the file into a buffer. */
    rc = ib_file_readall(tx->mp, json_cfg->file, &buf, &sz);
    if (rc != IB_OK) {
        if (rc == IB_EOTHER || rc == IB_EINVAL) {
            ib_log_error_tx(
                tx,
                "Error reading file \"%s\": %s",
                json_cfg->file,
                strerror(errno));
        }
        else {
            ib_log_error_tx(tx, "Failed to read JSON file \"%s\"", json_cfg->file);
        }
        return rc;
    }

    /* Parse the buffer into the fields list. */
    rc = ib_json_decode_ex(tx->mp, buf, sz, fields, &err_msg);
    if (rc != IB_OK) {
        ib_log_error_tx(
            tx,
            "Error decoding JSON file \"%s\": %s",
            json_cfg->file,
            err_msg);
        return rc;
    }

    return IB_OK;
}

/**
 * Create a new @a impl which is passed to json_load_fn().
 *
 * @param[in] ib IronBee Engine.
 * @param[in] params Parameters to constructor.
 * @param[out] impl The @ref json_t to be constructed.
 * @param[in] cbdata Callback data. An @ref init_collection_cfg_t.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL On invalid entry.
 * - IB_EALLOC On allocation error.
 */
static ib_status_t json_create_fn(
    ib_engine_t     *ib,
    const ib_list_t *params,
    void            *impl,
    void            *cbdata
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
    if (strstr(json_file, JSON_URI_PREFIX) == NULL) {
        ib_log_error(ib, "JSON URI Malformed: %s", json_file);
        return IB_EINVAL;
    }

    /* Move the character pointer past the prefix so only the file remains. */
    json_file += (sizeof(JSON_URI_PREFIX)-1);

    json_cfg->file = ib_util_relative_file(mp, cfg->config_file, json_file);
    if (json_cfg->file == NULL) {
        return IB_EALLOC;
    }

    *(json_t **)impl = json_cfg;
    return IB_OK;
}
#endif /* ENABLE_JSON */

/**
 * Var implementation data.
 */
struct var_t {
    const ib_list_t *fields; /**< Fields to return. */
};
typedef struct var_t var_t;
/**
 * Create vars.
 *
 * @param[in] ib IronBee engine.
 * @param[in] params Parameters.
 * @param[out] impl A new @ref var_t to be constructed.
 * @param[in] cbdata Callback data. Unused.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL On an invalid input from the config file.
 * - IB_EALLOC On allocation errors.
 * - Other on sub call errors.
 */
static ib_status_t var_create_fn(
    ib_engine_t     *ib,
    const ib_list_t *params,
    void            *impl,
    void            *cbdata
)
{
    assert(ib != NULL);
    assert(params != NULL);
    assert(impl != NULL);
    assert(cbdata == NULL);

    ib_mpool_t            *mp = ib_engine_pool_main_get(ib);
    var_t                 *var;
    ib_list_t             *fields;
    const ib_list_node_t  *node;
    ib_status_t            rc;
    ib_list_t             *transformations = NULL;
    const char            *collection_name; /* Used in logging. */


    var = ib_mpool_alloc(mp, sizeof(*var));
    if (var == NULL) {
        return IB_EALLOC;
    }

    rc = ib_list_create(&fields, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create field list.");
        return rc;
    }

    /* The collection name. We will skip this. */
    node = ib_list_first_const(params);
    if (node == NULL) {
        ib_log_error(ib, "VAR requires at least 2 arguments: name and uri.");
        return IB_EINVAL;
    }
    collection_name = (const char *)ib_list_node_data_const(node);

    /* The URI. We will skip this. */
    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_log_error(ib, "VAR requires at least 2 arguments: name and uri.");
        return IB_EINVAL;
    }

    /* Skip the URI parameter. */
    node = ib_list_node_next_const(node);

    /* For the rest of the nodes... */
    for ( ; node != NULL; node = ib_list_node_next_const(node)) {
        const char *assignment =
            (const char *)ib_list_node_data_const(node);
        const char *eqsign = index(assignment, '=');
        const ib_field_t *field = NULL;

        /* Assume an empty assignment if no equal sign is included. */
        if (eqsign == NULL) {
            ib_bytestr_t *bs = NULL;
            rc = ib_bytestr_dup_nulstr(&bs, mp, "");
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to create byte string.");
                return rc;
            }

            /* The whole assignment is just a variable name. */
            rc = ib_field_create(
                (ib_field_t **)&field,
                mp,
                assignment,
                strlen(assignment),
                IB_FTYPE_BYTESTR,
                ib_ftype_bytestr_in(bs));
        }
        else if (*(eqsign + 1) == '\0') {
            ib_bytestr_t *bs = NULL;
            rc = ib_bytestr_dup_nulstr(&bs, mp, "");
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to create byte string.");
                return rc;
            }

            /* The assignment is a var name + '='. */
            rc = ib_field_create(
                (ib_field_t **)&field,
                mp,
                assignment,
                strlen(assignment) - 1, /* -1 drops the '=' from the name. */
                IB_FTYPE_BYTESTR,
                ib_ftype_bytestr_in(bs));
        }
        else {
            const char *value;

            rc = ib_cfg_parse_target_string(mp, eqsign+1, &value, &transformations);
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to parse target value.");
            }

            /* Make sure value is a copy, not the original string. */
            if (value == eqsign+1) {
                value = ib_mpool_strdup(mp, value);
                if (value == NULL) {
                    return IB_EALLOC;
                }
            }

            /* Normal assignment. eqsign is the end of the name.
             * eqsign + 1 is the start of the assigned value. */
            ib_field_create_bytestr_alias(
                (ib_field_t **)&field,
                mp,
                assignment, eqsign - assignment,
                (const uint8_t *)IB_S2SL(value));
        }

        /* Check the field creation in the above if-then-else. */
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to create field for assignment %s",
                assignment);
            return IB_EALLOC;
        }

        /* Make sure we didn't somehow forget to set the field. */
        assert(field != NULL);

        /* Apply transformations to the field. */
        if (transformations != NULL) {
            const ib_list_node_t *tfn_node;
            IB_LIST_LOOP_CONST(transformations, tfn_node) {
                const ib_field_t *tmp_field;
                const ib_tfn_t   *tfn;
                const char       *tfn_name =
                    (const char *)ib_list_node_data_const(tfn_node);

                rc = ib_tfn_lookup(ib, tfn_name, &tfn);
                if (rc != IB_OK) {
                    ib_log_error(
                        ib,
                        "Could not fetch transformation %s. "
                        "Not initializing %s in %s.",
                        tfn_name,
                        field->name,
                        collection_name);
                    /* Don't signal a fatal error. Just don't work. */
                    return IB_OK;
                }

                rc = ib_tfn_execute(mp, tfn, field, &tmp_field);
                if (rc != IB_OK) {
                    ib_log_error(
                        ib,
                        "Failed to run transformation %s for InitCollection. "
                        "Not initializing %s in %s: %s",
                        tfn_name,
                        field->name,
                        collection_name,
                        ib_status_to_string(rc));
                    /* As above, failure should not kill the whole config. */
                    return IB_OK;
                }

                /* Promote the temporary field to the new current field. */
                field = tmp_field;
            }
        }

        rc = ib_list_push(fields, (void *)field);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to push field onto field list.");
            return rc;
        }
    }

    var->fields = fields;
    *(var_t **)impl = var;
    return IB_OK;
}

/**
 * Load fields created by var_create_fn().
 *
 * @param[in] impl The @ref var_t created by var_create_fn().
 * @param[in] tx The current transaction.
 * @param[in] key Unused.
 * @param[in] key_len Unused.
 * @param[in] fields The output fields.
 * @param[in] cbdata Callback data. Unused.
 *
 * @return
 * - IB_OK On success.
 * - IB_EOTHER On unexpected list manipulation errors.
 */
static
ib_status_t var_load_fn(
    void       *impl,
    ib_tx_t    *tx,
    const char *key,
    size_t      key_len,
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
 * Instantiate an instance of @a type and map @a collection_name with it.
 *
 * This function requests that the persistence framework create
 * a new named store using a random UUID as the name can calling
 * ib_persist_fw_create_store(). That collection named @a collection_name
 * is then mapped to that store, meaning that it will be populated
 * and persisted in the course of a transaction.
 *
 * @param[in] cp The configuration parser.
 * @param[in] ctx The configuration context.
 * @param[in] type The type being mapped.
 * @param[in] cfg The configuration used in this module.
 * @param[in] collection_name The name of the collection.
 * @param[in] params List of parameters.
 *                   The first element is the name of the collection.
 *                   The second element is the URI.
 *                   The rest are options.
 * @returns
 * - IB_OK On success.
 * - Other on failure of ib_uuid_create_v4_from_mp() or @c ib_persist_fw_* calls.
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
    char store_name[37];
    ib_status_t rc;

    rc = ib_uuid_create_v4(store_name);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create UUIDv4 store.");
        return rc;
    }

    rc = ib_persist_fw_create_store(
        cfg->persist_fw,
        ctx,
        type,
        store_name,
        params);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create store %s", store_name);
        return rc;
    }

    rc = ib_persist_fw_map_collection(
        cfg->persist_fw,
        ctx,
        collection_name,
        IB_S2SL("no key"),
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
 * Implement the InitCollection directive.
 *
 * @param[in] cp Configuration parser.
 * @param[in] directive InitCollection.
 * @param[in] vars List of `char *` types making up the parameters.
 * @param[in] cfg The module configuration.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If an error in the configuration parameters is detected.
 * - IB_EALLOC On memory allocation errors.
 * - Other when interacting with IronBee API.
 */
static ib_status_t init_collection_common(
    ib_cfgparser_t        *cp,
    const char            *directive,
    const ib_list_t       *vars,
    init_collection_cfg_t *cfg
)
{
    assert(cp != NULL);
    assert(directive != NULL);
    assert(vars != NULL);
    assert(cfg != NULL);
    assert(cfg->persist_fw != NULL);

    ib_status_t            rc;
    const ib_list_node_t  *node;
    const char            *name;   /* The name of the collection. */
    const char            *uri;    /* The URI to the resource. */
    ib_context_t          *ctx;

    ib_cfg_log_debug(cp, "Initializing collection.");

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
        ib_cfg_log_error(cp, "%s: No collection name specified", directive);
        goto exit_EINVAL;
    }
    name = (const char *)ib_list_node_data_const(node);
    if (name == NULL) {
        ib_cfg_log_error(cp, "Name parameter unexpectedly NULL.");
        goto exit_EINVAL;
    }

    /* Get the collection uri. */
    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_cfg_log_error(cp, "%s: No collection URI specified", directive);
        goto exit_EINVAL;
    }
    uri = (const char *)ib_list_node_data_const(node);
    if (uri == NULL) {
        ib_cfg_log_error(cp, "URI parameter unexpectedly NULL.");
        goto exit_EINVAL;
    }


    if (strncmp(uri, "vars:", sizeof("vars:")) == 0) {
        rc = domap(cp, ctx, VAR_TYPE, cfg, name, vars);
        if (rc != IB_OK) {
            goto exit_rc;
        }
    }
#if ENABLE_JSON
    else if (strncmp(uri, JSON_URI_PREFIX, sizeof(JSON_URI_PREFIX)-3) == 0) {
        rc = domap(cp, ctx, JSON_TYPE, cfg, name, vars);
        if (rc != IB_OK) {
            goto exit_rc;
        }
    }
#endif
    else {
        ib_cfg_log_error(cp, "URI \"%s\" not supported for persistence.", uri);
        goto exit_EINVAL;
    }

    rc = ib_var_source_register(
        NULL,
        ib_engine_var_config_get(cp->ib),
        IB_S2SL(name),
        IB_PHASE_NONE, IB_PHASE_NONE
    );
    if (rc != IB_EEXIST && rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to register collection %s: %s",
            name,
            ib_status_to_string(rc));
        goto exit_rc;
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

/**
 * Implement the IndexCollection directive.
 *
 * param[in] cp The configuration parser.
 * param[in] directive InitCollection.
 * param[in] vars Argument list to the directive.
 * param[in] cbdata An @ref init_collection_cfg_t.
 *
 * @returns results of init_collection_common();
 */
static ib_status_t init_collection_fn(
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
        (init_collection_cfg_t *)cbdata
    );
}

/**
 * Register directives so as to define a callback data struct.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg The module configuration.
 *
 * @returns
 * - IB_OK On Success.
 * - Other on failure of ib_config_register_directives().
 */
static ib_status_t register_directives(
    ib_engine_t           *ib,
    init_collection_cfg_t *cfg)
{
    assert(ib != NULL);
    assert(cfg != NULL);

    ib_status_t rc;

    rc = ib_config_register_directive(
        ib,
        "InitCollection",
        IB_DIRTYPE_LIST,
        (ib_void_fn_t)init_collection_fn,
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
        return IB_EALLOC;
    }

    cfg->persist_fw = NULL;

    rc = ib_persist_fw_create(ib, module, &(cfg->persist_fw));
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to register module "
            MODULE_NAME_STR
            " with persistence module.");
        return rc;
    }

    rc = register_directives(ib, cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register directives.");
        return rc;
    }

    rc = ib_persist_fw_register_type(
        cfg->persist_fw,
        ib_context_main(ib),
        VAR_TYPE,
        var_create_fn,        /* Create. */
        NULL,
        NULL,                 /* Destroy. */
        NULL,
        var_load_fn,          /* Load. */
        NULL,
        NULL,                 /* Store. */
        NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register var type.");
        return rc;
    }

#if ENABLE_JSON
    rc = ib_persist_fw_register_type(
        cfg->persist_fw,
        ib_context_main(ib),
        JSON_TYPE,
        json_create_fn,      /* Create. */
        cfg,
        NULL,                /* Destroy. */
        NULL,
        json_load_fn,        /* Load. */
        NULL,
        NULL,                /* Store. */
        NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register json type.");
        return rc;
    }
#endif

    return IB_OK;
}

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,    /* Header defaults. */
    MODULE_NAME_STR,              /* Module name. */
    IB_MODULE_CONFIG_NULL,        /* NULL Configuration: NULL, 0, NULL, NULL */
    NULL,                         /* Config map. */
    NULL,                         /* Directive map. Dynamically built. */
    init_collection_init,         /* Initialization. */
    NULL,                         /* Callback data. */
    NULL,                         /* Finalization. */
    NULL,                         /* Callback data. */
);
