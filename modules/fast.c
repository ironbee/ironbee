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
 * @c FastAutomata is context independent and must occur at most once in
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
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/rule_engine.h>

#include <assert.h>

/*! Module name. */
#define MODULE_NAME        fast
/*! Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();
 
/* Documented in definitions below. */
typedef struct fast_runtime_t         fast_runtime_t;
typedef struct fast_config_t          fast_config_t;
typedef struct fast_search_t          fast_search_t;
typedef struct fast_collection_spec_t fast_collection_spec_t;

/**
 * Module runtime data.
 *
 * A copy of this struct is contained in fast_config_t.  It is used to
 * distinguish runtime data from configuration parameters.
 */
struct fast_runtime_t
{
    /*! AC automata; outputs are indices into rule index. */
    ia_eudoxus_t *eudoxus;

    /*! Rule index: pointers to rules based on automata outputs. */
    const ib_rule_t **index;

    /*! Hash of id (@c const @c char *) to index (@c uint32_t *) */
    ib_hash_t *by_id;
};

/**
 * Module configuration data.
 *
 * Currently there is no configuration data as the fast module is context
 * independent.  That is, nothing about it varies across configuration
 * context.
 */
struct fast_config_t
{
    /*! Runtime data */
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
    /*! Runtime data. */
    const fast_runtime_t *runtime;

    /*! Rule execution context. */
    const ib_rule_exec_t *rule_exec;

    /*! List to add eligible rules to. */
    ib_list_t *rule_list;

    /*! Rules already added by pointer.  No data. */
    ib_hash_t *rule_set;
};

/* Configuration */

/*! IndexSize key for automata metadata. */
static const char *c_index_size_key          = "IndexSize";
/*! Index key for automata metadata. */
static const char *c_index_key               = "Index";

/**
 * Collection specification.
 */
struct fast_collection_spec_t
{
    /*! Name of collection to feed to automata. */
    const char *name;
    /*! String to separate key and value with. */
    const char *separator;
};

/*! Bytestrings to feed during REQUEST_HEADER phase. */
static const char *c_request_header_bytestrings[] = {
    "REQUEST_METHOD",
    "REQUEST_URI",
    "REQUEST_PROTOCOL",
    NULL
};
/*! Collections to feed during REQUEST_HEADER phase. */
static const fast_collection_spec_t c_request_header_collections[] = {
    { "REQUEST_HEADERS",    ":" },
    { "REQUEST_URI_PARAMS", "=" },
    { NULL, NULL }
};

/*! Bytestrings to feed during REQUEST_BODY phase. */
static const char *c_request_body_bytestrings[] = {
    NULL
};

/*! Collections to feed during REQUEST_BODY phase. */
static const fast_collection_spec_t c_request_body_collections[] = {
    { "REQUEST_BODY_PARAMS", "=" },
    { NULL, NULL }
};

/*! String to separate bytestrings. */
static const char *c_bytestring_separator = " ";
/*! String to separate different keys, bytestring or collection entries. */
static const char *c_data_separator       = "\n";

/* Helper functions */

/**
 * As ia_eudoxus_error() but uses "no error" for NULL.
 *
 * @param[in] eudoxus Eudoxus engine.
 * @return Error message as string.
 *
 * @sa ia_eudoxus_error()
 */
static inline
const char *fast_eudoxus_error(const ia_eudoxus_t *eudoxus)
{
    return ia_eudoxus_error(eudoxus) == NULL ?
        "no error" : 
         ia_eudoxus_error(eudoxus);
}

/**
 * Access configuration data.
 *
 * @param[in] ib IronBee engine.
 * @return
 * - configuration on success.
 * - NULL on failure.
 */
static
fast_config_t *fast_get_config(
     ib_engine_t *ib
)
{
    assert(ib != NULL);

    ib_module_t   *module;
    ib_context_t  *context;
    ib_status_t    rc;
    fast_config_t *config;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        return NULL;
    }

    context = ib_context_main(ib);
    if (context == NULL) {
        return NULL;
    }

    rc = ib_context_module_config(context, module, &config);
    if (rc != IB_OK) {
        return NULL;
    }

    return config;
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
            "fast: Eudoxus Execution Failure: %s",
            fast_eudoxus_error(eudoxus)
        );
        return IB_EINVAL;
    }

    return IB_OK;
}

/**
 * Feed a byte string from an @ref ib_data_t to the automata.
 *
 * @param[in] ib                    IronBee engine; used for logging.
 * @param[in] eudoxus               Eudoxus engine; used for
 *                                  ia_eudoxus_error().
 * @param[in] state                 Current Eudoxus execution state; updated.
 * @param[in] data                  Data source.
 * @param[in] bytestring_field_name Name of data field to feed.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_feed_data_bytestring(
    const ib_engine_t *ib,
    const ia_eudoxus_t *eudoxus,
    ia_eudoxus_state_t *state,
    const ib_data_t *data,
    const char *bytestring_field_name
)
{
    assert(ib                    != NULL);
    assert(eudoxus               != NULL);
    assert(state                 != NULL);
    assert(data                  != NULL);
    assert(bytestring_field_name != NULL);
    
    ib_field_t         *field;
    const ib_bytestr_t *bs;
    ib_status_t         rc;

    rc = ib_data_get(data, bytestring_field_name, &field);
    if (rc == IB_ENOENT) {
        ib_log_error(
            ib,
            "fast: No such data %s",
            bytestring_field_name
        );
        return IB_EOTHER;
    }
    else if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error fetching data %s: %s",
            bytestring_field_name,
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
            "fast: Error loading data field %s: %s",
            bytestring_field_name,
            ib_status_to_string(rc)
        );
        return IB_EOTHER;
    }

    return fast_feed(
        ib,
        eudoxus,
        state,
        ib_bytestr_const_ptr(bs), ib_bytestr_size(bs)
    );
}

/**
 * Feed a collection of byte strings from an @ref ib_data_t to automata.
 *
 * @param[in] ib          IronBee engine; used for logging.
 * @param[in] eudoxus     Eudoxus engine; used for ia_eudoxus_error().
 * @param[in] state       Current Eudoxus execution state; updated.
 * @param[in] data        Data source.
 * @param[in] collections Collection to feed.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_feed_data_collection(
    const ib_engine_t             *ib,
    const ia_eudoxus_t            *eudoxus,
    ia_eudoxus_state_t            *state,
    const ib_data_t               *data,
    const fast_collection_spec_t  *collection
)
{
    assert(ib         != NULL);
    assert(eudoxus    != NULL);
    assert(state      != NULL);
    assert(data       != NULL);
    assert(collection != NULL);
    
    ib_field_t           *field;
    const ib_list_t      *subfields;
    const ib_list_node_t *node;
    const ib_field_t     *subfield;
    const ib_bytestr_t   *bs;
    ib_status_t           rc;

    rc = ib_data_get(data, collection->name, &field);
    if (rc == IB_ENOENT) {
        ib_log_error(
            ib,
            "fast: No such data %s",
            collection->name
        );
        return IB_EOTHER;
    }
    else if (rc != IB_OK) {
        ib_log_error(
            ib,
            "fast: Error fetching data %s: %s",
            collection->name,
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
            "fast: Error loading data field %s: %s",
            collection->name,
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
                "fast: Error loading data subfield %s of %s: %s",
                subfield->name,
                collection->name,
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
            sizeof(collection->separator)
        );
        if (rc != IB_OK) {
            return rc;
        }

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

        rc = fast_feed(
            ib,
            eudoxus,
            state,
            (const uint8_t *)c_data_separator,
            sizeof(c_data_separator)
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
 * @param[in] data        Data source.
 * @param[in] bytestrings Bytestrings to feed.
 * @param[in] collections Collections to feed.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_feed_phase(
    const ib_engine_t             *ib,
    const ia_eudoxus_t            *eudoxus,
    ia_eudoxus_state_t            *state,
    const ib_data_t               *data,
    const char                   **bytestrings,
    const fast_collection_spec_t  *collections
)
{
    assert(ib          != NULL);
    assert(eudoxus     != NULL);
    assert(state       != NULL);
    assert(data        != NULL);
    assert(bytestrings != NULL);
    assert(collections != NULL);
    
    ib_status_t rc;

    /* Lower level feed_* routines log errors, so we simply abort on
     * non-OK returns. */
    for (
        const char **bytestring_name = bytestrings;
        *bytestring_name != NULL;
        ++bytestring_name
    ) {
        rc = fast_feed_data_bytestring(
            ib,
            eudoxus,
            state,
            data,
            *bytestring_name
        );
        if (rc != IB_OK) {
            return rc;
        }
        rc = fast_feed(
            ib,
            eudoxus,
            state,
            (const uint8_t *)c_bytestring_separator,
            sizeof(c_bytestring_separator)
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
        sizeof(c_data_separator)
    );
    if (rc != IB_OK) {
        return rc;
    }

    for (
        const fast_collection_spec_t *collection = collections;
        collection != NULL;
        ++collection
    ) {
        rc = fast_feed_data_collection(
            ib,
            eudoxus,
            state,
            data,
            collection
        );
        if (rc != IB_OK) {
            return rc;
        }
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
      ia_eudoxus_t  *engine,
     const char    *output,
     size_t         output_length,
     const uint8_t *input_location,
     void          *callback_data
)
{
    assert(engine        != NULL);
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
            engine,
            "Invalid automata; output length; expected = %zd actual = %zd.",
            sizeof(uint32_t),
            output_length
        );
        return IA_EUDOXUS_CMD_ERROR;
    }

    memcpy(&index, output, sizeof(index));
    rule = search->runtime->index[index];

    if (rule->meta.phase != search->rule_exec->phase) {
        return IA_EUDOXUS_CMD_CONTINUE;
    }

    /* Check/mark if already added. */
    rc = ib_hash_get_ex(search->rule_set, NULL, output, output_length);
    if (rc == IB_OK) {
        /* Rule already added. */
        return IA_EUDOXUS_CMD_CONTINUE;
    }
    if (rc != IB_ENOENT) {
        /* Error. */
        ia_eudoxus_set_error_printf(
            engine,
            "Unexpected error reading from rule set hash: %s",
            ib_status_to_string(rc)
        );
        return IA_EUDOXUS_CMD_ERROR;
    }

    rc = ib_hash_set_ex(search->rule_set, output, output_length, (void *)1);
    if (rc != IB_OK) {
        ia_eudoxus_set_error_printf(
            engine,
            "Unexpected error writing to rule set hash: %s",
            ib_status_to_string(rc)
        );
        return IA_EUDOXUS_CMD_ERROR;
    }

    rc = ib_list_push(search->rule_list, (void *)rule);
    if (rc != IB_OK) {
         ia_eudoxus_set_error_printf(
             engine,
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
    const ib_engine_t *ib,
    const ib_rule_t   *rule,
    void              *cbdata
)
{
/* These macros are local to this function. */
#define FAST_RETURN(return_rc) { rc = return_rc; goto done; }
#define FAST_CHECK_RC(msg) \
    if (rc != IB_OK) { \
        ib_log_error(ib, "fast: %s: %s", (msg), ib_status_to_string((rc))); \
        FAST_RETURN(IB_EOTHER); \
    }

    assert(ib   != NULL);
    assert(rule != NULL);

    fast_runtime_t *runtime = (fast_runtime_t *)cbdata;

    assert(runtime        != NULL);
    assert(runtime->index != NULL);
    assert(runtime->by_id != NULL);

    ib_status_t  rc;
    ib_list_t   *actions;
    ib_mpool_t  *mp;
    uint32_t    *index;

    /* This memory pool will exist only as long as this stack frame. */
    rc = ib_mpool_create(&mp, "fast_ownership_tmp", NULL);
    FAST_CHECK_RC("Could not create temporary memory pool");

    rc = ib_list_create(&actions, mp);
    FAST_CHECK_RC("Could not create list to hold results");

    rc = ib_rule_search_action(
        ib,
        rule,
        RULE_ACTION_TRUE,
        "fast",
        actions,
        NULL
    );
    FAST_CHECK_RC("Could not access actions of rule");

    if (ib_list_elements(actions) == 0) {
        /* Decline rule. */
        FAST_RETURN(IB_DECLINED);
    }

    if (rule->meta.id == NULL) {
        ib_log_error(ib, "fast: fast rule lacks id.");
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
            "fast: fast rule %s not in automata.",
            rule->meta.id
        );
        FAST_RETURN(IB_EINVAL);
    }
    FAST_CHECK_RC("Could not access by_id hash.");

    /* Claim rule. */
    runtime->index[*index] = rule;
    FAST_RETURN(IB_OK);

#undef FAST_CHECK_RC
#undef FAST_RETURN
    assert(! "Should never reach this line.");
done:
    ib_mpool_destroy(mp);
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
 * @param[in] cbdata      Runtime.
 * @param[in] bytestrings Bytestrings to feed.
 * @param[in] collections Collections to feed.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL on IronAutomata failure; will emit log message.
 * - IB_EOTHER on IronBee failure; will emit log message.
 */
static
ib_status_t fast_rule_injection(
    const ib_engine_t             *ib,
    const ib_rule_exec_t          *rule_exec,
    ib_list_t                     *rule_list,
    void                          *cbdata,
    const char                   **bytestrings,
    const fast_collection_spec_t  *collections
)
{
    assert(ib                  != NULL);
    assert(rule_exec           != NULL);
    assert(rule_exec->tx       != NULL);
    assert(rule_exec->tx->data != NULL);
    assert(rule_list           != NULL);

    const fast_runtime_t *runtime = (const fast_runtime_t *)cbdata;

    assert(runtime          != NULL);
    assert(runtime->eudoxus != NULL);
    assert(runtime->index   != NULL);

    ia_eudoxus_result_t  irc;
    ia_eudoxus_state_t  *state;
    ib_status_t          rc;
    const ib_data_t     *data;
    ib_mpool_t          *tmp_mp;
    ib_hash_t           *rule_set;

    rc = ib_mpool_create(&tmp_mp, "fast temporary pool", NULL);
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

    rc = ib_hash_create(&rule_set, tmp_mp);

    fast_search_t search = {
        .runtime   = runtime,
        .rule_exec = rule_exec,
        .rule_list = rule_list,
        .rule_set  = rule_set
    };

    data = rule_exec->tx->data;

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
        data,
        bytestrings, 
        collections
    );

done:
    if (tmp_mp != NULL) {
        ib_mpool_destroy(tmp_mp);
    }

    return rc;
}

/**
 * Called at REQUEST_HEADER phase to determine additional rules to inject.
 *
 * @param[in] ib        IronBee engine.
 * @param[in] rule_exec Current rule execution context.
 * @param[in] rule_list List to add injected rules to; updated.
 * @param[in] cbdata    Runtime.
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
    return fast_rule_injection(
        ib,
        rule_exec,
        rule_list,
        cbdata,
        c_request_header_bytestrings,
        c_request_header_collections
    );
}

/**
 * Called at REQUEST_BODY phase to determine additional rules to inject.
 *
 * @param[in] ib        IronBee engine.
 * @param[in] rule_exec Current rule execution context.
 * @param[in] rule_list List to add injected rules to; updated.
 * @param[in] cbdata    Runtime.
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
    return fast_rule_injection(
        ib,
        rule_exec,
        rule_list,
        cbdata,
        c_request_body_bytestrings,
        c_request_body_collections
    );
}

/**
 * Called when @c FastAutomata directive appears in configuration.
 *
 * @param[in] cp     Configuration parsed; used for logging.
 * @param[in] name   Name; must be @ref c_fast_automata_directive.
 * @param[in] p1     Path to automata.
 * @param[in] cbdata Ignored.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EINVAL on failures probably due to missing or malformed automata;
 *   will emit log message.
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
#define FAST_METADATA_ERROR(msg, param) \
    { \
        ib_cfg_log_error(cp, "fast: %s: " # msg, p1, (param)); \
        return IB_EINVAL; \
    }
#define FAST_CHECK_RC(msg) \
    if (rc != IB_OK) { \
        ib_cfg_log_error(cp, "fast: %s: %s: %s", p1, msg, ib_status_to_string(rc)); \
        return IB_EOTHER; \
    }

    assert(cp     != NULL);
    assert(cp->ib != NULL);
    assert(cp->mp != NULL);
    assert(name   != NULL);
    assert(p1     != NULL);

    ib_engine_t         *ib;
    ib_mpool_t          *mp;
    ib_mpool_t          *cfg_mp;
    fast_runtime_t      *runtime;
    fast_config_t       *config;
    ia_eudoxus_result_t  irc;
    ib_status_t          rc;
    const uint8_t       *data;
    size_t               data_size;
    uint32_t             index_size;

    ib     = cp->ib;
    mp     = ib_engine_pool_main_get(ib);
    cfg_mp = cp->mp;
    config = fast_get_config(ib);

    assert(config != NULL);

    if (config->runtime != NULL) {
        ib_cfg_log_error(
            cp,
            "fast: %s: FastAutomata directive must be unique.",
            p1
        );
        return IB_EINVAL;
    }

    /* Create Runtime */
    config->runtime = runtime =
        ib_mpool_calloc(mp, 1, sizeof(*config->runtime));
    if (config->runtime == NULL) {
        return IB_EALLOC;
    }

    /* Load Automata */
    irc = ia_eudoxus_create_from_path(&runtime->eudoxus, p1);
    if (irc != IA_EUDOXUS_OK) {
        ib_cfg_log_error(
            cp,
            "fast: %s: Error loading automata: %d %s",
            p1,
            irc,
            fast_eudoxus_error(runtime->eudoxus)
        );
        return IB_EINVAL;
    }

    /* Find IndexSize */
    irc = ia_eudoxus_metadata_with_key(
        runtime->eudoxus,
        (const uint8_t *)c_index_size_key,
        sizeof(c_index_size_key),
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
            "Could not process %s; likely corrupt.",
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

    /* Create index */
    runtime->index =
        ib_mpool_calloc(mp, index_size, sizeof(*runtime->index));
    if (runtime->index == NULL) {
        return IB_EALLOC;
    }

    /* Create by_id */
    rc = ib_hash_create(&runtime->by_id, cfg_mp);
    FAST_CHECK_RC("Could not create hash");

    /* Load index */
    irc = ia_eudoxus_metadata_with_key(
        runtime->eudoxus,
        (const uint8_t *)c_index_key,
        sizeof(c_index_key),
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
            "Could not process %s; likely corrupt.",
            c_index_key
        );
    }
    {
        uint32_t *indices =
            ib_mpool_calloc(cfg_mp, index_size, sizeof(uint32_t));
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
        "fast",
        PHASE_REQUEST_HEADER,
        fast_rule_injection_request_header, runtime
    );
    FAST_CHECK_RC("Error registering injection for request header phase.");
    rc = ib_rule_register_injection_fn(
        ib,
        "fast",
        PHASE_REQUEST_BODY,
        fast_rule_injection_request_body, runtime
    );
    FAST_CHECK_RC("Error registering injection for request header phase.");

    rc = ib_rule_register_ownership_fn(
        ib,
        "fast",
        fast_ownership, runtime
    );
    FAST_CHECK_RC("Error registering ownership");

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
    fast_config_t *config = fast_get_config(ib);
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
static fast_config_t g_fast_config = {NULL};

/*! Module directive map. */
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
    NULL,                                /**< Context open function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context close function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context destroy function */
    NULL                                 /**< Callback data */
);
