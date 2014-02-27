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
 * @brief IronBee --- Fast Pattern Module
 *
 * This module adds support for fast rules.  See fast/fast.html for details.
 *
 * Provides a single directive:
 * @code
 * FastAutomata <path>
 * @endcode
 *
 * @c FastAutomata must occur in the main context and at most once in
 * configuration.  It loads the specified automata and enables the fast rule
 * subsystem.  The loaded automata must be consistent with the fast rules in
 * the configuration.  This consistency is usually achieved by feeding the
 * rules into a set of scripts which creates the automata (see
 * fast/fast.html).
 *
 * In general, @c EOTHER is used to indicate IronBee related failures and
 * @c EINVAL is used to indicate IronAutomata related failures.
 *
 * @todo Support response phases.
 * @todo Support streaming phases.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/eudoxus.h>

#include <ironbee/cfgmap.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/module.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <assert.h>

/** Module name. */
#define MODULE_NAME        fast
/** Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

#ifndef DOXYGEN_SKIP
IB_MODULE_DECLARE();
#endif

/* Documented in definitions below. */
typedef struct fast_runtime_t                 fast_runtime_t;
typedef struct fast_config_t                  fast_config_t;
typedef struct fast_search_t                  fast_search_t;
typedef struct fast_collection_spec_t         fast_collection_spec_t;
typedef struct fast_collection_runtime_spec_t fast_collection_runtime_spec_t;
typedef struct fast_specs_t                   fast_specs_t;

/**
 * Module runtime data.
 *
 * A copy of this struct is contained in @ref fast_config_t.  It is used to
 * distinguish runtime data from configuration parameters.
 */
struct fast_runtime_t
{
    /** AC automata; outputs are indices into rule index. */
    ia_eudoxus_t *eudoxus;

    /** Rule index: pointers to rules based on automata outputs. */
    const ib_rule_t **index;

    /** Hash of id (@c const @c char *) to index (@c uint32_t *) */
    ib_hash_t *by_id;

    /** Specs on what to feed. */
    fast_specs_t *specs;
};

/**
 * Module configuration data.
 */
struct fast_config_t
{
    /** Rules in this context */
    ib_hash_t *rules;

    /** Runtime data */
    fast_runtime_t *runtime;
};

/**
 * Search state.
 *
 * This structure holds the data used during a search of the automata.  In
 * particular it is the callback data of the function passed to
 * ia_eudoxus_execute().
 */
struct fast_search_t
{
    /** Runtime data. */
    const fast_runtime_t *runtime;

    /** Rule execution context. */
    const ib_rule_exec_t *rule_exec;

    /** Rules eligible to be added. */
    const ib_hash_t *rules;

    /** List to add eligible rules to. */
    ib_list_t *rule_list;

    /** Rules already added by pointer.  No data. */
    ib_hash_t *rule_set;
};

/* Configuration */

/** IndexSize key for automata metadata. */
static const char *c_index_size_key = "IndexSize";

/** Index key for automata metadata. */
static const char *c_index_key = "Index";

/** Name of action to mark rules as fast. */
static const char *c_fast_action = "fast";

/**
 * Collection specification.
 */
struct fast_collection_spec_t
{
    /** Name of collection to feed to automata. */
    const char *name;
    /** String to separate key and value with. */
    const char *separator;
};

/**
 * Collection specification with source instead of string.
 */
struct fast_collection_runtime_spec_t
{
    /** Source of collection to feed to automata. */
    const ib_var_source_t *source;
    /** String to separate key and value with. */
    const char *separator;
};

/**
 * All runtime specifications.
 */
struct fast_specs_t
{
    const ib_var_source_t                **request_header_bytestrings;
    const fast_collection_runtime_spec_t  *request_header_collections;
    const ib_var_source_t                **request_body_bytestrings;
    const fast_collection_runtime_spec_t  *request_body_collections;
    const ib_var_source_t                **response_header_bytestrings;
    const fast_collection_runtime_spec_t  *response_header_collections;
    const ib_var_source_t                **response_body_bytestrings;
    const fast_collection_runtime_spec_t  *response_body_collections;
};

/** Bytestrings to feed during REQUEST_HEADER phase. */
static const char *c_request_header_bytestrings[] = {
    "REQUEST_METHOD",
    "REQUEST_URI_RAW",
    "REQUEST_PROTOCOL",
    NULL
};
/** Collections to feed during REQUEST_HEADER phase. */
static const fast_collection_spec_t c_request_header_collections[] = {
    { "REQUEST_HEADERS",    ":" },
    { "REQUEST_URI_PARAMS", "=" },
    { NULL, NULL }
};

/** Bytestrings to feed during REQUEST phase. */
static const char *c_request_body_bytestrings[] = {
    NULL
};

/** Collections to feed during REQUEST phase. */
static const fast_collection_spec_t c_request_body_collections[] = {
    { "REQUEST_BODY_PARAMS", "=" },
    { NULL, NULL }
};

/** Bytestrings to feed during RESPONSE_HEADER phase. */
static const char *c_response_header_bytestrings[] = {
    "RESPONSE_PROTOCOL",
    "RESPONSE_STATUS",
    "RESPONSE_MESSAGE",
    NULL
};
/** Collections to feed during RESPONSE_HEADER phase. */
static const fast_collection_spec_t c_response_header_collections[] = {
    { "RESPONSE_HEADERS", ":" },
    { NULL, NULL }
};

/** Bytestrings to feed during RESPONSE phase. */
static const char *c_response_body_bytestrings[] = {
    NULL
};

/** Collections to feed during RESPONSE phase. */
static const fast_collection_spec_t c_response_body_collections[] = {
    { NULL, NULL }
};

/** String to separate bytestrings. */
static const char *c_bytestring_separator = " ";
/** String to separate different keys, bytestring or collection entries. */
static const char *c_data_separator = "\n";

/* Helper functions */

/**
 * Size of static C-array @a a.
 */
#define ARRAY_SIZE(a) sizeof((a)) / sizeof(*(a))

/**
 * As ia_eudoxus_error() but uses "no error" for NULL.
 *
 * @param[in] eudoxus Eudoxus engine.
 * @return Error message as string.
 *
 * @sa ia_eudoxus_error()
 */
static inline
const char *fast_eudoxus_error(
    const ia_eudoxus_t *eudoxus
)
{
    return ia_eudoxus_error(eudoxus) == NULL ?
        "no error" :
        ia_eudoxus_error(eudoxus);
}

/**
 * Access configuration data.
 *
 * @param[in] ib  IronBee engine.
 * @param[in] ctx Context to fetch for.
 * @return
 * - configuration on success.
 * - NULL on failure.
 */
static
fast_config_t *fast_get_config(
    const ib_engine_t  *ib,
    const ib_context_t *ctx
)
{
    assert(ib != NULL);

    ib_module_t   *module;
    ib_status_t    rc;
    fast_config_t *config;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        return NULL;
    }

    rc = ib_context_module_config(ctx, module, &config);
    if (rc != IB_OK) {
        return NULL;
    }

    return config;
}

/**
 * Access configuration data from a module, read-only.
 *
 * @param[in] m   Module.
 * @param[in] ctx Context to fetch for.
 * @return
 * - configuration on success.
 * - NULL on failure.
 */
static
const fast_config_t *fast_get_config_module(
    const ib_module_t  *m,
    const ib_context_t *ctx
)
{
    assert(m != NULL);
    assert(ctx != NULL);

    ib_status_t rc;
    const fast_config_t *cfg = NULL;

    rc = ib_context_module_config(ctx, m, &cfg);
    if (rc != IB_OK) {
        return NULL;
    }
    return cfg;
}

/**
 * Feed data to the automata.
 *
 * @param[in] ib          IronBee engine; used for logging.
 * @param[in] eudoxus     Eudoxus engine; used for ia_eudoxus_error().
 * @param[in] state       Current Eudoxus execution state; updated.
 * @param[in] data        Data to send to automata.
 * @param[in] data_length Length of @a data.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 */
static
ib_status_t fast_feed(
    const ib_engine_t  *ib,
    const ia_eudoxus_t *eudoxus,
    ia_eudoxus_state_t *state,
    const uint8_t      *data,
    size_t              data_length
)
{
    assert(ib      != NULL);
    assert(eudoxus != NULL);
    assert(state   != NULL);
    assert(data    != NULL);

    ia_eudoxus_result_t irc;

    irc = ia_eudoxus_execute(state, data, data_length);
    if (irc != IA_EUDOXUS_OK) {
        ib_log_error(
            ib,
            "fast: Error executing eudoxus: %s",
            fast_eudoxus_error(eudoxus)
        );
        return IB_EINVAL;
    }

    return IB_OK;
}

/**
 * Feed a byte string from an @ref ib_var_store_t to the automata.
 *
 * @param[in] ib                IronBee engine; used for logging.
 * @param[in] eudoxus           Eudoxus engine; used for
 *                              ia_eudoxus_error().
 * @param[in] state             Current Eudoxus execution state; updated.
 * @param[in] var_store         Var store.
 * @param[in] bytestring_source Source of bytestring.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static ib_status_t fast_feed_var_bytestring(
    const ib_engine_t     *ib,
    const ia_eudoxus_t    *eudoxus,
    ia_eudoxus_state_t    *state,
    const ib_var_store_t  *var_store,
    const ib_var_source_t *bytestring_source
)
{
    assert(ib                != NULL);
    assert(eudoxus           != NULL);
    assert(state             != NULL);
    assert(var_store         != NULL);
    assert(bytestring_source != NULL);

    const ib_field_t   *field;
    const ib_bytestr_t *bs;
    ib_status_t         rc;
    const char         *name;
    size_t              name_length;

    ib_var_source_name(bytestring_source, &name, &name_length);

    rc = ib_var_source_get_const(bytestring_source, &field, var_store);
    if (rc == IB_ENOENT) {
        ib_log_error(
            ib,
            "fast: No such data %.*s",
            (int)name_length, name
        );
        return IB_EOTHER;
    }
    else if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error fetching var %.*s: %s",
            (int)name_length, name,
            ib_status_to_string(rc)
        );
        return IB_EOTHER;
    }

    rc = ib_field_value_type(
        field,
        ib_ftype_bytestr_out(&bs),
        IB_FTYPE_BYTESTR
    );
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error loading data field %.*s: %s",
            (int)name_length, name,
            ib_status_to_string(rc)
        );
        return IB_EOTHER;
    }

    if (ib_bytestr_const_ptr(bs) == NULL || ib_bytestr_size(bs) == 0) {
        return IB_OK;
    }
    return fast_feed(
        ib,
        eudoxus,
        state,
        ib_bytestr_const_ptr(bs), ib_bytestr_size(bs)
    );
}

/**
 * Feed a collection of byte strings from an @ref ib_var_store_t to automata.
 *
 * @param[in] ib          IronBee engine; used for logging.
 * @param[in] eudoxus     Eudoxus engine; used for ia_eudoxus_error().
 * @param[in] state       Current Eudoxus execution state; updated.
 * @param[in] var_store   Var store.
 * @param[in] collection  Collection to feed.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_feed_var_collection(
    const ib_engine_t                    *ib,
    const ia_eudoxus_t                   *eudoxus,
    ia_eudoxus_state_t                   *state,
    const ib_var_store_t                 *var_store,
    const fast_collection_runtime_spec_t *collection
)
{
    assert(ib         != NULL);
    assert(eudoxus    != NULL);
    assert(state      != NULL);
    assert(var_store  != NULL);
    assert(collection != NULL);

    const ib_field_t     *field;
    const ib_list_t      *subfields;
    const ib_list_node_t *node;
    const ib_field_t     *subfield;
    const ib_bytestr_t   *bs;
    ib_status_t           rc;
    const char           *name;
    size_t                name_length;

    ib_var_source_name(collection->source, &name, &name_length);

    rc = ib_var_source_get_const(collection->source, &field, var_store);
    if (rc == IB_ENOENT) {
        // Var not set.
        return IB_OK;
    }
    else if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error fetching data %.*s: %s",
            (int)name_length, name,
            ib_status_to_string(rc)
        );
        return IB_EOTHER;
    }

    rc = ib_field_value_type(
        field,
        ib_ftype_list_out(&subfields),
        IB_FTYPE_LIST
    );
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error loading data field %.*s: %s",
            (int)name_length, name,
            ib_status_to_string(rc)
        );
        return IB_EOTHER;
    }

    IB_LIST_LOOP_CONST(subfields, node) {
        subfield = (const ib_field_t *)ib_list_node_data_const(node);
        assert(subfield != NULL);

        rc = ib_field_value_type(
            subfield,
            ib_ftype_bytestr_out(&bs),
            IB_FTYPE_BYTESTR
        );
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "fast: Error loading data subfield %s of %.*s: %s",
                subfield->name,
                (int)name_length, name,
                ib_status_to_string(rc)
            );
            return IB_EOTHER;
        }

        rc = fast_feed(
            ib,
            eudoxus,
            state,
            (const uint8_t *)subfield->name,
            subfield->nlen
        );
        if (rc != IB_OK) {
            return rc;
        }

        rc = fast_feed(
            ib,
            eudoxus,
            state,
            (const uint8_t *)collection->separator,
            strlen(collection->separator)
        );
        if (rc != IB_OK) {
            return rc;
        }

        if (ib_bytestr_const_ptr(bs) != NULL && ib_bytestr_size(bs) > 0) {
            rc = fast_feed(
                ib,
                eudoxus,
                state,
                ib_bytestr_const_ptr(bs),
                ib_bytestr_size(bs)
            );
            if (rc != IB_OK) {
                return rc;
            }
        }

        rc = fast_feed(
            ib,
            eudoxus,
            state,
            (const uint8_t *)c_data_separator,
            strlen(c_data_separator)
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Feed data for a specific phase.
 *
 * Pull and feed the specified bytestrings and collections to an automata.
 * This function is similar to fast_rule_injection() but requires an already
 * functioning automata execution.  It can be combined with other feed
 * functions.
 *
 * @param[in] ib          IronBee engine.
 * @param[in] eudoxus     Eudoxus engine.
 * @param[in] state       Eudoxus execution state; updated.
 * @param[in] var_store   Var store.
 * @param[in] bytestrings Bytestrings to feed.
 * @param[in] collections Collections to feed.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_feed_phase(
    const ib_engine_t                     *ib,
    const ia_eudoxus_t                    *eudoxus,
    ia_eudoxus_state_t                    *state,
    const ib_var_store_t                  *var_store,
    const ib_var_source_t                **bytestrings,
    const fast_collection_runtime_spec_t  *collections
)
{
    assert(ib          != NULL);
    assert(eudoxus     != NULL);
    assert(state       != NULL);
    assert(var_store   != NULL);
    assert(bytestrings != NULL);
    assert(collections != NULL);

    ib_status_t rc;

    /* Lower level feed_* routines log errors, so we simply abort on
     * non-OK returns. */
    for (
        const ib_var_source_t **bytestring_source = bytestrings;
        *bytestring_source != NULL;
        ++bytestring_source
    ) {
        rc = fast_feed_var_bytestring(
            ib,
            eudoxus,
            state,
            var_store,
            *bytestring_source
        );
        if (rc != IB_OK) {
            return rc;
        }
        rc = fast_feed(
            ib,
            eudoxus,
            state,
            (const uint8_t *)c_bytestring_separator,
            strlen(c_bytestring_separator)
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    rc = fast_feed(
        ib,
        eudoxus,
        state,
        (uint8_t *)c_data_separator,
        strlen(c_data_separator)
    );
    if (rc != IB_OK) {
        return rc;
    }

    for (
        const fast_collection_runtime_spec_t *collection = collections;
        collection->source != NULL;
        ++collection
    ) {
        rc = fast_feed_var_collection(
            ib,
            eudoxus,
            state,
            var_store,
            collection
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Convert strings to sources.
 *
 * @param[in]  ib   Engine.
 * @param[out] dst  Where to store sources.
 * @param[in]  src  Strings to convert.
 * @param[in]  nsrc Number of strings in @a src.
 * @return
 * - IB_OK on success.
 * - Any error returned by ib_var_source_acquire().
 **/
static
ib_status_t fast_convert_specs_bytestrs(
    ib_engine_t             *ib,
    const ib_var_source_t ***dst,
    const char             **src,
    size_t                   nsrc
)
{
    assert(ib  != NULL);
    assert(src != NULL);
    assert(dst != NULL);

    ib_mm_t mm = ib_engine_mm_main_get(ib);
    ib_var_config_t *config = ib_engine_var_config_get(ib);

    *dst = ib_mm_calloc(mm, nsrc, sizeof(**dst));
    for (size_t i = 0; i < nsrc; ++i) {
        const char *name = src[i];
        ib_var_source_t *source = NULL;
        ib_status_t rc;

        if (name != NULL) {
            rc = ib_var_source_acquire(&source, mm, config, IB_S2SL(name));
            if (rc != IB_OK) {
                ib_log_error(
                    ib,
                    "Error fetching source for %s: %s",
                    name,
                    ib_status_to_string(rc)
                );
                return rc;
            }
        }
        (*dst)[i] = source;
    }

    return IB_OK;
}

/**
 * Convert collection spec to runtime collection spec.
 *
 * @param[in]  ib   Engine.
 * @param[out] dst  Where to store runtime spec.
 * @param[in]  src  Specs to convert.
 * @param[in]  nsrc Number of specs in @a src.
 * @return
 * - IB_OK on success.
 * - Any error returned by ib_var_source_acquire().
 **/
static
ib_status_t fast_convert_specs_collections(
    ib_engine_t                           *ib,
    const fast_collection_runtime_spec_t **dst,
    const fast_collection_spec_t          *src,
    size_t                                 nsrc
)
{
    assert(ib  != NULL);
    assert(src != NULL);
    assert(dst != NULL);

    ib_mm_t mm = ib_engine_mm_main_get(ib);
    ib_var_config_t *config = ib_engine_var_config_get(ib);
    fast_collection_runtime_spec_t *result;

    result = ib_mm_calloc(mm, nsrc, sizeof(**dst));
    for (size_t i = 0; i < nsrc; ++i) {
        const fast_collection_spec_t *spec = &(src[i]);
        ib_var_source_t *source = NULL;
        ib_status_t rc;

        if (spec->name != NULL) {
            rc = ib_var_source_acquire(
                &source,
                mm,
                config,
                IB_S2SL(spec->name)
            );
            if (rc != IB_OK) {
                ib_log_error(
                    ib,
                    "Error fetching source for %s: %s",
                    spec->name,
                    ib_status_to_string(rc)
                );
                return rc;
            }
        }
        result[i].source = source;
        result[i].separator = spec->separator;
    }

    *dst = result;

    return IB_OK;
}

/**
 * Convert specs to runtime specs.
 *
 * @param[in] ib    IronBee engine.
 * @param[in] specs Specs structure to fill.
 * @return
 * - IB_OK on success.
 * - Any error returned by fast_convert_specs_bytestrs() or
 *   fast_convert_specs_collections()
 **/
static
ib_status_t fast_convert_specs(
    ib_engine_t  *ib,
    fast_specs_t *specs
)
{
    ib_status_t rc;

    rc = fast_convert_specs_bytestrs(
        ib,
        &specs->request_header_bytestrings,
        c_request_header_bytestrings,
        ARRAY_SIZE(c_request_header_bytestrings)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = fast_convert_specs_collections(
        ib,
        &specs->request_header_collections,
        c_request_header_collections,
        ARRAY_SIZE(c_request_header_collections)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = fast_convert_specs_bytestrs(
        ib,
        &specs->request_body_bytestrings,
        c_request_body_bytestrings,
        ARRAY_SIZE(c_request_body_bytestrings)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = fast_convert_specs_collections(
        ib,
        &specs->request_body_collections,
        c_request_body_collections,
        ARRAY_SIZE(c_request_body_collections)
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = fast_convert_specs_bytestrs(
        ib,
        &specs->response_header_bytestrings,
        c_response_header_bytestrings,
        ARRAY_SIZE(c_response_header_bytestrings)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = fast_convert_specs_collections(
        ib,
        &specs->response_header_collections,
        c_response_header_collections,
        ARRAY_SIZE(c_response_header_collections)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = fast_convert_specs_bytestrs(
        ib,
        &specs->response_body_bytestrings,
        c_response_body_bytestrings,
        ARRAY_SIZE(c_response_body_bytestrings)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = fast_convert_specs_collections(
        ib,
        &specs->response_body_collections,
        c_response_body_collections,
        ARRAY_SIZE(c_response_body_collections)
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/* Callbacks */

/**
 * Called by Eudoxus when automata finds an eligible rule.
 *
 * @param[in] eudoxus        Eudoxus engine; used to record errors.
 * @param[in] output         Eudoxus output; @c uint32_t of index location.
 * @param[in] output_length  Length of @a output; must be @c sizeof(uint32_t).
 * @param[in] input_location Location in input; ignored.
 * @param[in] callback_data  The @ref fast_search_t.
 * @return
 * - IA_EUDOXUS_CMD_CONTINUE on success.
 * - IA_EUDOXUS_CMD_ERROR on any error; message will be set.
 */
static
ia_eudoxus_command_t fast_eudoxus_callback(
    ia_eudoxus_t  *eudoxus,
    const char    *output,
    size_t         output_length,
    const uint8_t *input_location,
    void          *callback_data
)
{
    assert(eudoxus       != NULL);
    assert(output        != NULL);
    assert(callback_data != NULL);

    fast_search_t *search = (fast_search_t *)callback_data;

    assert(search->runtime   != NULL);
    assert(search->rule_exec != NULL);
    assert(search->rule_list != NULL);
    assert(search->rule_set  != NULL);

    uint32_t         index;
    const ib_rule_t *rule;
    ib_status_t      rc;

    /* Error instead of assert as automata may be invalid. */
    if (output_length != sizeof(uint32_t)) {
        ia_eudoxus_set_error_printf(
            eudoxus,
            "Invalid automata; output length; expected = %zd actual = %zd.",
            sizeof(uint32_t),
            output_length
        );
        return IA_EUDOXUS_CMD_ERROR;
    }

    memcpy(&index, output, sizeof(index));
    rule = search->runtime->index[index];

    if (rule == NULL) {
        /* Rule is in automata but not claimed.  This can occur when fast
         * rules are present but enabled anywhere.
         */
        return IA_EUDOXUS_CMD_CONTINUE;
    }

    /* Check phase. */
    if (rule->meta.phase != search->rule_exec->phase) {
        return IA_EUDOXUS_CMD_CONTINUE;
    }

    /* Check context, i.e., is in eligible rules */
    rc = ib_hash_get_ex(
        search->rules,
        NULL,
        output,
        output_length
    );
    if (rc == IB_ENOENT) {
        return IA_EUDOXUS_CMD_CONTINUE;
    }
    else if (rc != IB_OK) {
        ib_log_error(
            search->rule_exec->ib,
            "Unexpected return code from eligible rules check: %s",
            ib_status_to_string(rc)
        );
        return IA_EUDOXUS_CMD_ERROR;
    }

    /* Check/mark if already added. */
    {
        void *dummy_value;
        rc = ib_hash_get_ex(
            search->rule_set,
            &dummy_value,
            output,
            output_length
        );
        if (rc == IB_OK) {
            /* Rule already added. */
            return IA_EUDOXUS_CMD_CONTINUE;
        }
        if (rc != IB_ENOENT) {
            /* Error. */
            ia_eudoxus_set_error_printf(
                eudoxus,
                "Unexpected error reading from rule set hash: %s",
                ib_status_to_string(rc)
            );
            return IA_EUDOXUS_CMD_ERROR;
        }
    }

    rc = ib_hash_set_ex(search->rule_set, output, output_length, (void *)1);
    if (rc != IB_OK) {
        ia_eudoxus_set_error_printf(
            eudoxus,
            "Unexpected error writing to rule set hash: %s",
            ib_status_to_string(rc)
        );
        return IA_EUDOXUS_CMD_ERROR;
    }

    rc = ib_list_push(search->rule_list, (void *)rule);
    if (rc != IB_OK) {
         ia_eudoxus_set_error_printf(
             eudoxus,
             "Error pushing rule onto rule list: %s",
            ib_status_to_string(rc)
        );
        return IA_EUDOXUS_CMD_ERROR;
    }

    return IA_EUDOXUS_CMD_CONTINUE;
}

/**
 * Called for every rule to determine if rule is owned by fast module.
 *
 * @param[in] ib     IronBee engine.
 * @param[in] rule   Rule to evaluate claim.
 * @param[in] ctx    Context @a rule is enabled in.
 * @param[in] cbdata Runtime.
 *
 * @returns
 * - IB_OK if rule is a fast rule.
 * - IB_DECLINED if rule is not a fast rule.
 * - IB_EOTHER if IronBee API fails.
 * - IB_EINVAL if rule wants to be a fast rule but cannot be.  This can
 *   occur if a rule is marked as fast but either lacks an id or is not in
 *   the loaded automata.
 */
ib_status_t fast_ownership(
    const ib_engine_t  *ib,
    const ib_rule_t    *rule,
    const ib_context_t *ctx,
    void               *cbdata
)
{
/* These macros are local to this function. */
#ifndef DOXYGEN_SKIP
#define FAST_RETURN(return_rc) { rc = return_rc; goto done; }
#define FAST_CHECK_RC(msg) \
    if (rc != IB_OK) { \
        ib_log_error(ib, "fast: %s: %s", (msg), ib_status_to_string((rc))); \
        FAST_RETURN(IB_EOTHER); \
    }
#endif

    assert(ib   != NULL);
    assert(rule != NULL);

    fast_config_t *cfg = fast_get_config(ib, ctx);

    assert(cfg != NULL);
    assert(cfg->rules != NULL);

    fast_runtime_t *runtime = (fast_runtime_t *)cbdata;

    assert(runtime        != NULL);
    assert(runtime->index != NULL);
    assert(runtime->by_id != NULL);
    assert(runtime == cfg->runtime);

    ib_status_t      rc;
    ib_list_t       *actions;
    ib_mpool_lite_t *tmp_mp = NULL;
    ib_mm_t          tmp_mm;
    uint32_t        *index;

    /* This memory pool will exist only as long as this stack frame. */
    rc = ib_mpool_lite_create(&tmp_mp);
    FAST_CHECK_RC("Error creating temporary memory pool");
    tmp_mm = ib_mm_mpool_lite(tmp_mp);

    rc = ib_list_create(&actions, tmp_mm);
    FAST_CHECK_RC("Error creating list to hold results");

    rc = ib_rule_search_action(
        ib,
        rule,
        IB_RULE_ACTION_TRUE,
        c_fast_action,
        actions,
        NULL
    );
    FAST_CHECK_RC("Error accessing actions of rule");

    if (ib_list_elements(actions) == 0) {
        /* Decline rule. */
        FAST_RETURN(IB_DECLINED);
    }

    if (rule->meta.id == NULL) {
        ib_log_error(ib, "fast: Fast rule lacks id.");
        FAST_RETURN(IB_EINVAL);
    }

    rc = ib_hash_get(
        runtime->by_id,
        &index,
        rule->meta.id
    );
    if (rc == IB_ENOENT) {
        ib_log_error(
            ib,
            "fast: Fast rule %s not in automata.",
            rule->meta.id
        );
        FAST_RETURN(IB_EINVAL);
    }
    FAST_CHECK_RC("Error accessing by_id hash.");

    /* Mark as eligible in this context. */
    {
        void *dummy_value;
        rc = ib_hash_set_ex(
            cfg->rules, /* per context hash */
            (const char *)index,
            sizeof(*index),
            &dummy_value
        );
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "fast: Fast rule %s unable to be added to eligible rules: %s",
                rule->meta.id,
                ib_status_to_string(rc)
            );
            FAST_RETURN(IB_EINVAL);
        }
    }

    /* Claim rule. */
    runtime->index[*index] = rule;
    FAST_RETURN(IB_OK);

#undef FAST_CHECK_RC
#undef FAST_RETURN
    assert(! "Should never reach this line.");
done:
    if (tmp_mp != NULL) {
        ib_mpool_lite_destroy(tmp_mp);
    }
    return rc;
}

/**
 * Evaluate automata for a single phase.
 *
 * This function handles injection for a single phase.  It is called by
 * phase specific functions that simply forward their parameters along with
 * the bytestrings and collections specific to the phase.
 *
 * @sa fast_feed_phase()
 *
 * @param[in] ib          IronBee engine.
 * @param[in] rule_exec   Current rule execution context.
 * @param[in] rule_list   List to add injected rules to; updated.
 * @param[in] cbdata      Module.
 * @param[in] bytestrings Bytestrings to feed.
 * @param[in] collections Collections to feed.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_rule_injection(
    const ib_engine_t                     *ib,
    const ib_rule_exec_t                  *rule_exec,
    ib_list_t                             *rule_list,
    void                                  *cbdata,
    const ib_var_source_t                **bytestrings,
    const fast_collection_runtime_spec_t  *collections
)
{
    assert(ib                       != NULL);
    assert(rule_exec                != NULL);
    assert(rule_exec->tx            != NULL);
    assert(rule_exec->tx->var_store != NULL);
    assert(rule_list                != NULL);

    const ib_module_t *m = (const ib_module_t *)(cbdata);
    const fast_config_t *cfg = fast_get_config_module(m, rule_exec->tx->ctx);
    assert(cfg != NULL);
    const fast_runtime_t *runtime = cfg->runtime;
    assert(runtime          != NULL);
    assert(runtime->eudoxus != NULL);
    assert(runtime->index   != NULL);

    ia_eudoxus_result_t   irc;
    ia_eudoxus_state_t   *state = NULL;
    ib_status_t           rc;
    const ib_var_store_t *var_store;
    ib_mpool_lite_t      *tmp_mp = NULL;
    ib_mm_t               tmp_mm;
    ib_hash_t            *rule_set;

    rc = ib_mpool_lite_create(&tmp_mp);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error creating temporary memory pool: %s",
            ib_status_to_string(rc)
        );
        tmp_mp = NULL;
        rc = IB_EOTHER;
        goto done;
    }
    tmp_mm = ib_mm_mpool_lite(tmp_mp);

    rc = ib_hash_create(&rule_set, tmp_mm);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error creating rule set hash: %s",
            ib_status_to_string(rc)
        );
        rc = IB_EOTHER;
        goto done;
    }

    fast_search_t search = {
        .runtime   = runtime,
        .rule_exec = rule_exec,
        .rules     = cfg->rules,
        .rule_list = rule_list,
        .rule_set  = rule_set
    };

    var_store = rule_exec->tx->var_store;

    irc = ia_eudoxus_create_state(
        &state,
        runtime->eudoxus,
        fast_eudoxus_callback,
        &search
    );
    if (irc != IA_EUDOXUS_OK) {
        ib_log_error(
            ib,
            "fast: Error creating state: %s",
            fast_eudoxus_error(runtime->eudoxus)
        );
        rc = IB_EINVAL;
        goto done;
    }

    /* fast_feed_phase() will handle logging errors. */
    rc = fast_feed_phase(
        ib,
        runtime->eudoxus,
        state,
        var_store,
        bytestrings,
        collections
    );

done:
    if (state != NULL) {
        ia_eudoxus_destroy_state(state);
    }
    if (tmp_mp != NULL) {
        ib_mpool_lite_destroy(tmp_mp);
    }

    return rc;
}

/**
 * Called at REQUEST_HEADER phase to determine additional rules to inject.
 *
 * @param[in] ib        IronBee engine.
 * @param[in] rule_exec Current rule execution context.
 * @param[in] rule_list List to add injected rules to; updated.
 * @param[in] cbdata    Module.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_rule_injection_request_header(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata
)
{
    const fast_specs_t *specs = fast_get_config_module(
        (const ib_module_t *)cbdata,
        rule_exec->tx->ctx
    )->runtime->specs;
    assert(specs != NULL);
    return fast_rule_injection(
        ib,
        rule_exec,
        rule_list,
        cbdata,
        specs->request_header_bytestrings,
        specs->request_header_collections
    );
}

/**
 * Called at REQUEST phase to determine additional rules to inject.
 *
 * @param[in] ib        IronBee engine.
 * @param[in] rule_exec Current rule execution context.
 * @param[in] rule_list List to add injected rules to; updated.
 * @param[in] cbdata    Module.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_rule_injection_request_body(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata
)
{
    const fast_specs_t *specs = fast_get_config_module(
        (const ib_module_t *)cbdata,
        rule_exec->tx->ctx
    )->runtime->specs;
    assert(specs != NULL);
    return fast_rule_injection(
        ib,
        rule_exec,
        rule_list,
        cbdata,
        specs->request_body_bytestrings,
        specs->request_body_collections
    );
}

/**
 * Called at RESPONSE_HEADER phase to determine additional rules to inject.
 *
 * @param[in] ib        IronBee engine.
 * @param[in] rule_exec Current rule execution context.
 * @param[in] rule_list List to add injected rules to; updated.
 * @param[in] cbdata    Module.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_rule_injection_response_header(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata
)
{
    const fast_specs_t *specs = fast_get_config_module(
        (const ib_module_t *)cbdata,
        rule_exec->tx->ctx
    )->runtime->specs;
    assert(specs != NULL);

    return fast_rule_injection(
        ib,
        rule_exec,
        rule_list,
        cbdata,
        specs->response_header_bytestrings,
        specs->response_header_collections
    );
}

/**
 * Called at RESPONSE phase to determine additional rules to inject.
 *
 * @param[in] ib        IronBee engine.
 * @param[in] rule_exec Current rule execution context.
 * @param[in] rule_list List to add injected rules to; updated.
 * @param[in] cbdata    Module.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_rule_injection_response_body(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata
)
{
    const fast_specs_t *specs = fast_get_config_module(
        (const ib_module_t *)cbdata,
        rule_exec->tx->ctx
    )->runtime->specs;
    assert(specs != NULL);

    return fast_rule_injection(
        ib,
        rule_exec,
        rule_list,
        cbdata,
        specs->response_body_bytestrings,
        specs->response_body_collections
    );
}

/**
 * Called on context open.
 *
 * On open of each context, will initialize a hash to contain which rules
 * are in this context.
 *
 * @param[in] ib  Engine.
 * @param[in] ctx Context.
 * @param[in] event Unused.
 * @param[in] cbdata Unused.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - As fast_convert_specs()
 **/
static
ib_status_t fast_ctx_open(
    ib_engine_t           *ib,
    ib_context_t          *ctx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    ib_status_t rc;
    fast_config_t *cfg = fast_get_config(ib, ctx);

    assert(cfg != NULL);

    rc = ib_hash_create(&cfg->rules, ib_engine_mm_main_get(ib));
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error creating rule hash for context %s: %s",
            ib_context_name_get(ctx),
            ib_status_to_string(rc)
        );
        return rc;
    }
    return IB_OK;
}

/**
 * Called on context close.
 *
 * On close of main context, will call fast_convert_specs().  This is the
 * appropriate time to do so as all vars will be registered by then.
 *
 * @param[in] ib  Engine.
 * @param[in] ctx Context.
 * @param[in] event Unused.
 * @param[in] cbdata Unused.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EOTHER on unexpected failure.
 * - As fast_convert_specs()
 **/
static
ib_status_t fast_ctx_close(
    ib_engine_t           *ib,
    ib_context_t          *ctx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    if (ib_context_type(ctx) == IB_CTYPE_MAIN) {
        fast_config_t *cfg = fast_get_config(ib, ctx);
        if (cfg == NULL) {
            return IB_EOTHER;
        }
        cfg->runtime->specs = (fast_specs_t *)ib_mm_alloc(
            ib_engine_mm_main_get(ib),
            sizeof(*(cfg->runtime->specs))
        );
        if (cfg->runtime->specs == NULL) {
            return IB_EALLOC;
        }

        return fast_convert_specs(ib, cfg->runtime->specs);
    }

    return IB_OK;
}

/**
 * Called when @c FastAutomata directive appears in configuration.
 *
 * @param[in] cp     Configuration parsed; used for logging.
 * @param[in] name   Name; ignored.
 * @param[in] p1     Path to automata.
 * @param[in] cbdata Ignored.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EINVAL on failures related to invalid use such as not main context,
 *   duplicate directive, or missing/malformed automata; will emit log
 *   message.
 * - IB_EOTHER on failures due to IronBee API failures; will emit log message.
 * - IB_EALLOC on failures due to memory allocation; no log message.
 **/
static
ib_status_t fast_dir_fast_automata(
    ib_cfgparser_t *cp,
    const char     *name,
    const char     *p1,
    void           *cbdata
)
{
/* These macros are local to this function. */
#ifndef DOXYGEN_SKIP
#define FAST_METADATA_ERROR(msg, param) \
    { \
        ib_cfg_log_error(cp, "fast: %s: " msg " (%d %s)", p1, (param), irc, fast_eudoxus_error(runtime->eudoxus)); \
        return IB_EINVAL; \
    }
#define FAST_CHECK_RC(msg) \
    if (rc != IB_OK) { \
        ib_cfg_log_error(cp, "fast: %s: %s: %s", p1, msg, ib_status_to_string(rc)); \
        return IB_EOTHER; \
    }
#endif

    assert(cp     != NULL);
    assert(cp->ib != NULL);
    assert(cp->mp != NULL);
    assert(name   != NULL);
    assert(p1     != NULL);

    ib_engine_t         *ib;
    ib_mm_t              mm;
    fast_runtime_t      *runtime;
    fast_config_t       *config;
    ia_eudoxus_result_t  irc;
    ib_status_t          rc;
    const uint8_t       *data;
    size_t               data_size;
    uint32_t             index_size;
    ib_module_t         *module;

    ib     = cp->ib;
    if (cp->cur_ctx != ib_context_main(ib)) {
        ib_cfg_log_error(
            cp,
            "fast: %s: FastAutomata directive must occur in main context.",
            p1
        );
        return IB_EINVAL;
    }

    mm     = ib_engine_mm_main_get(ib);
    config = fast_get_config(ib, cp->cur_ctx);

    assert(config != NULL);

    if (config->runtime != NULL) {
        ib_cfg_log_error(
            cp,
            "fast: %s: FastAutomata directive must be unique.",
            p1
        );
        return IB_EINVAL;
    }

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "fast: %s: Unable to get my own module: %s",
            p1,
            ib_status_to_string(rc)
        );
        return rc;
    }

    /* Create Runtime */
    config->runtime = runtime =
        ib_mm_calloc(mm, 1, sizeof(*config->runtime));
    if (config->runtime == NULL) {
        return IB_EALLOC;
    }

    /* Load Automata */
    irc = ia_eudoxus_create_from_path(&runtime->eudoxus, p1);
    if (irc != IA_EUDOXUS_OK) {
        /* Note: ia_eudoxus_error() will not work as runtime->eudoxus
         * did not finish construction. */
        ib_cfg_log_error(
            cp,
            "fast: %s: Error loading automata: %d",
            p1,
            irc
        );
        return IB_EINVAL;
    }

    /* Find IndexSize */
    irc = ia_eudoxus_metadata_with_key(
        runtime->eudoxus,
        (const uint8_t *)c_index_size_key,
        strlen(c_index_size_key),
        &data,
        &data_size
    );
    if (irc == IA_EUDOXUS_END) {
        FAST_METADATA_ERROR(
            "Automata does not contain %s metadata.",
            c_index_size_key
        );
    }
    else if (irc != IA_EUDOXUS_OK) {
        FAST_METADATA_ERROR(
            "Error processing %s; likely corrupt.",
            c_index_size_key
        );
    }
    if (data_size != sizeof(uint32_t)) {
        FAST_METADATA_ERROR(
            "%s is incorrectly formatted; likely corrupt.",
            c_index_size_key
        );
        return IB_EINVAL;
    }
    memcpy(&index_size, data, sizeof(index_size));
    if (index_size == 0) {
        ib_cfg_log_error(cp, "Automata has index size of 0.");
        return IB_EINVAL;
    }

    /* Create index */
    runtime->index =
        ib_mm_calloc(mm, index_size, sizeof(*runtime->index));
    if (runtime->index == NULL) {
        return IB_EALLOC;
    }

    /* Create by_id */
    rc = ib_hash_create(&runtime->by_id, mm);
    FAST_CHECK_RC("Error creating hash");

    /* Load index */
    irc = ia_eudoxus_metadata_with_key(
        runtime->eudoxus,
        (const uint8_t *)c_index_key,
        strlen(c_index_key),
        &data,
        &data_size
    );
    if (irc == IA_EUDOXUS_END) {
        FAST_METADATA_ERROR(
            "Automata does not contain %s metadata.",
            c_index_key
        );
    }
    else if (irc != IA_EUDOXUS_OK) {
        FAST_METADATA_ERROR(
            "Error processing %s; likely corrupt.",
            c_index_key
        );
    }
    {
        uint32_t *indices =
            ib_mm_calloc(mm, index_size, sizeof(uint32_t));
        if (indices == NULL) {
            return IB_EALLOC;
        }
        for (size_t i = 0; i < index_size; ++i) {
            indices[i] = i;
        }

        const uint8_t *data_end = data + data_size;
        size_t index = 0;
        while (data < data_end) {
            rc = ib_hash_set(
                runtime->by_id,
                (const char *)data,
                &(indices[index])
            );
            if (rc != IB_OK) {
                ib_cfg_log_error(
                    cp,
                    "fast: %s: Error building id map: %zd %s",
                    p1,
                    index,
                    ib_status_to_string(rc)
                );
                return IB_EOTHER;
            }
            ++index;
            data += strlen((const char *)data) + 1;
        }
    }

    /* Register hooks */
    rc = ib_rule_register_injection_fn(
        ib,
        MODULE_NAME_STR,
        IB_PHASE_REQUEST_HEADER,
        fast_rule_injection_request_header, module
    );
    FAST_CHECK_RC("Error registering injection for request header phase.");
    rc = ib_rule_register_injection_fn(
        ib,
        MODULE_NAME_STR,
        IB_PHASE_REQUEST,
        fast_rule_injection_request_body, module
    );
    FAST_CHECK_RC("Error registering injection for request header phase.");
    rc = ib_rule_register_injection_fn(
        ib,
        MODULE_NAME_STR,
        IB_PHASE_RESPONSE_HEADER,
        fast_rule_injection_response_header, module
    );
    FAST_CHECK_RC("Error registering injection for response header phase.");
    rc = ib_rule_register_injection_fn(
        ib,
        MODULE_NAME_STR,
        IB_PHASE_RESPONSE,
        fast_rule_injection_response_body, module
    );
    FAST_CHECK_RC("Error registering injection for response header phase.");

    rc = ib_rule_register_ownership_fn(
        ib,
        MODULE_NAME_STR,
        fast_ownership, runtime
    );
    FAST_CHECK_RC("Error registering ownership");

    /* Register the fast "action" */
    rc = ib_action_register(
        ib,
        c_fast_action,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL
    );
    FAST_CHECK_RC("Error registering action");

    /* Register context open hook to setup per-context data. */
    rc = ib_hook_context_register(ib, context_open_event,
                                  fast_ctx_open, NULL);
    FAST_CHECK_RC("Error registering context close.");
    /* Register context close hook to convert specs once all vars are
     * registered. */
    rc = ib_hook_context_register(ib, context_close_event,
                                  fast_ctx_close, NULL);
    FAST_CHECK_RC("Error registering context close.");

    return IB_OK;
#undef FAST_METADATA_ERROR
#undef FAST_IB_ERROR
}

/**
 * Called when module unloads.
 *
 * @param[in] ib     IronBee engine; used to fetch runtime.
 * @param[in] m      Ignored.
 * @param[in] cbdata Ignored.
 * @return IB_OK
 */
static
ib_status_t fast_fini(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *cbdata
)
{
    fast_config_t *config = NULL;
    const ib_context_t *ctx = ib_context_main(ib);
    if (ctx == NULL) {
        return IB_OK;
    }

    config = fast_get_config(ib, ctx);
    if (
        config                   == NULL ||
        config->runtime          == NULL ||
        config->runtime->eudoxus == NULL
    ) {
        return IB_OK;
    }

    ia_eudoxus_destroy(config->runtime->eudoxus);

    return IB_OK;
}

/**
 * Initial values of @ref fast_config_t.
 *
 * This static will *only* be passed to IronBee as part of module
 * definition.  It will never be read or written by any code in this file.
 */
static fast_config_t g_fast_config = {NULL, NULL};

#ifndef DOXYGEN_SKIP
static IB_DIRMAP_INIT_STRUCTURE(fast_directive_map) = {
    IB_DIRMAP_INIT_PARAM1(
        "FastAutomata",
        fast_dir_fast_automata,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&g_fast_config),    /**< Global config data */
    NULL,                                /**< Configuration field map */
    fast_directive_map,                  /**< Config directive map */
    NULL,                                /**< Initialize function */
    NULL,                                /**< Callback data */
    fast_fini,                           /**< Finish function */
    NULL,                                /**< Callback data */
);
#endif
