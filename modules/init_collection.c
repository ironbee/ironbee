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
 * Module configuration.
 */
struct init_collection_cfg_t {
    ib_pstnsfw_t *pstnsfw; /**< Module configuration. */
    ib_module_t  *module;  /**< Our module structure at init time. */
};
typedef struct init_collection_cfg_t init_collection_cfg_t;

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

static ib_status_t map_vars(
)
{
    return IB_OK;
}

/* Include / exclude JSON support. */
#ifdef ENABLE_JSON
static ib_status_t map_json(
)
{
    return IB_OK;
}
#endif

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
    const char            *name;  /* The name of the collection. */
    const char            *uri;   /* The URI to the resource. */

    /* Get the collection name string */
    node = ib_list_first_const(vars);
    if (node == NULL) {
        ib_cfg_log_error(cp, " %s: No collection name specified", directive);
        return IB_EINVAL;
    }
    name = (const char *)ib_list_node_data_const(node);

    /* Get the collection uri. */
    node = ib_list_node_next_const(node);
    if (node == NULL) {
        ib_cfg_log_error(cp, " %s: No collection URI specified", directive);
        return IB_EINVAL;
    }
    uri = (const char *)ib_list_node_data_const(node);

    if (strncmp(uri, "vars:", sizeof("vars:")) == 0) {
        rc = map_vars();
        if (rc != IB_OK) {
            return rc;
        }
    }
#ifdef ENABLE_JSON
    else if (strncmp(uri, "json-file:", sizeof("json-file:")) == 0) {
        rc = map_json();
        if (rc != IB_OK) {
            return rc;
        }
    }
#endif
    else {
        ib_cfg_log_error(cp, "URI %s not supported for persitence.", uri);
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

    return IB_OK;
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
            NULL
        ),
        IB_DIRMAP_INIT_LIST(
            "InitCollectionIndexed",
            init_collection_indexed,
            NULL
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
