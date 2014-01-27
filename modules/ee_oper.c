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
 * @brief IronBee --- Eudoxus operator Module
 *
 * This module adds Eudoxus operators
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include <ironautomata/eudoxus.h>

#include <ironbee/capture.h>
#include <ironbee/context.h>
#include <ironbee/engine_state.h>
#include <ironbee/hash.h>
#include <ironbee/module.h>
#include <ironbee/operator.h>
#include <ironbee/path.h>
#include <ironbee/rule_engine.h>
#include <ironbee/util.h>

#include <assert.h>
#include <unistd.h>

/** Module name. */
#define MODULE_NAME        eudoxus_operators
/** Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

#ifndef DOXYGEN_SKIP
IB_MODULE_DECLARE();
#endif

/* See definition below for documentation. */
typedef struct ee_config_t ee_config_t;
typedef struct ee_operator_data_t ee_operator_data_t;
typedef struct ee_tx_data_t ee_tx_data_t;

struct ee_config_t {
    /** Hash of eudoxus patterns defined via the LoadEudoxus directive. */
    ib_hash_t *eudoxus_pattern_hash;
};

/* Operator instance data. */
struct ee_operator_data_t {
    /** Pointer to the eudoxus pattern for this instance. */
    ia_eudoxus_t *eudoxus;
};

/* Callback data for ee match */
struct ee_callback_data_t
{
    ib_tx_t *tx;
    ib_field_t *capture;
};
typedef struct ee_callback_data_t ee_callback_data_t;

/* Per-tx inter-call data */
struct ee_tx_data_t
{
    /** Eudoxus state */
    ia_eudoxus_state_t *eudoxus_state;
    /** Have we reached the end of the automata? */
    bool end_of_automata;
};

/**
 * Access configuration data.
 *
 * @param[in] ib IronBee engine.
 * @return
 * - configuration on success.
 * - NULL on failure.
 */
static
ee_config_t *ee_get_config(
    ib_engine_t *ib
)
{
    assert(ib != NULL);

    ib_module_t  *module;
    ib_context_t *context;
    ib_status_t   rc;
    ee_config_t  *config;

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
 * Get or create an ib_hash_t inside of @c tx for storing the operator state.
 *
 * The hash is stored at the key @c HASH_NAME_STR.
 *
 * @param[in] m  This module.
 * @param[in] tx The transaction containing @c tx->data which holds
 *            the @a operator_data object.
 * @param[out] hash The fetched or created rule data hash. This is set
 *             to NULL on failure.
 *
 * @return
 *   - IB_OK on success.
 *   - IB_EALLOC on allocation failure
 */
static
ib_status_t get_or_create_operator_data_hash(
    const ib_module_t  *m,
    ib_tx_t            *tx,
    ib_hash_t         **hash
)
{
    assert(tx != NULL);
    assert(tx->mp != NULL);

    ib_status_t rc;

    /* Get or create the hash that contains the rule data. */
    rc = ib_tx_get_module_data(tx, m, hash);
    if ( (rc == IB_OK) && (*hash != NULL) ) {
        return IB_OK;
    }

    rc = ib_hash_create(hash, tx->mp);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tx_set_module_data(tx, m, *hash);
    if (rc != IB_OK) {
        *hash = NULL;
    }

    return rc;
}

/**
 * Return the per-transaction state for the operator.
 *
 * @param[in] m This module.
 * @param[in,out] tx Transaction to lookup the data in.
 * @param[in] instance_data Pointer to the operator instance data.
 *                          The pointer value is used key to lookup the instance
 *                          state.
 * @param[out] tx_data Returns the tx data if found.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_ENOENT if the state is not found. The caller should create it
 *               and add it to the hash using @ref set_ee_tx_data
 */
static
ib_status_t get_ee_tx_data(
    const ib_module_t   *m,
    ib_tx_t             *tx,
    ee_operator_data_t  *instance_data,
    ee_tx_data_t       **tx_data
)
{
    assert(tx != NULL);
    assert(tx->mp != NULL);
    assert(instance_data != NULL);
    assert(tx_data != NULL);

    ib_hash_t *hash;
    ib_status_t rc;

    rc = get_or_create_operator_data_hash(m, tx, &hash);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_hash_get_ex(hash, tx_data, (const char *)&instance_data, sizeof(ee_operator_data_t *));
    if (rc != IB_OK) {
        *tx_data = NULL;
    }

    return rc;
}

/**
 * Store the per-transaction data for use with the operator.
 *
 * @param[in] m This module.
 * @param[in,out] tx Transaction to store the data in.
 * @param[in] instance_data Pointer to the operator instance data.
 * @param[in] tx_data Data to be stored.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_ENOENT if the structure does not exist.
 */
static
ib_status_t set_ee_tx_data(
    const ib_module_t  *m,
    ib_tx_t            *tx,
    ee_operator_data_t *instance_data,
    ee_tx_data_t       *tx_data
)
{
    assert(tx != NULL);
    assert(tx->mp != NULL);
    assert(instance_data != NULL);
    assert(tx_data != NULL);

    ib_hash_t *hash;
    ib_status_t rc;

    rc = get_or_create_operator_data_hash(m, tx, &hash);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_hash_set_ex(hash,
                        (const char *)&instance_data,
                        sizeof(ee_operator_data_t *),
                        tx_data);

    return rc;
}


/**
 * Load a eudoxus pattern so it can be used in rules.
 *
 * The filename should point to a compiled automata. If a relative path is
 * given, it will be loaded relative to the current configuration file.
 *
 * @param[in] cp Configuration parser.
 * @param[in] name Directive name.
 * @param[in] pattern_name Name to associate with the pattern.
 * @param[in] filename Filename to load.
 * @param[in] cbdata Callback data (unused)
 * @return
 * - IB_OK on success.
 * - IB_EEXIST if the pattern has already been defined.
 * - IB_EINVAL if there was an error loading the automata.
 */
static
ib_status_t load_eudoxus_pattern_param2(ib_cfgparser_t *cp,
                                        const char *name,
                                        const char *pattern_name,
                                        const char *filename,
                                        void *cbdata)
{
    ib_engine_t *ib;
    ib_status_t rc;
    const char *automata_file;
    ia_eudoxus_result_t ia_rc;
    ib_hash_t *eudoxus_pattern_hash;
    ia_eudoxus_t *eudoxus;
    const ee_config_t* config;
    ib_mpool_t *mp_tmp;
    void *tmp;

    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(pattern_name != NULL);
    assert(filename != NULL);

    mp_tmp = ib_engine_pool_temp_get(cp->ib);
    ib = cp->ib;
    config = ee_get_config(ib);
    assert(config != NULL);

    eudoxus_pattern_hash = config->eudoxus_pattern_hash;
    assert(eudoxus_pattern_hash != NULL);

    /* Check if the pattern name is already in use */
    rc = ib_hash_get(eudoxus_pattern_hash, &tmp, pattern_name);
    if (rc == IB_OK) {
        ib_log_error(cp->ib,
                     MODULE_NAME_STR ": Pattern named \"%s\" already defined",
                     pattern_name);
        return IB_EEXIST;
    }

    automata_file = ib_util_relative_file(mp_tmp, cp->curr->file, filename);

    if (access(automata_file, R_OK) != 0) {
        ib_log_error(cp->ib,
                     MODULE_NAME_STR ": Error accessing eudoxus automata file: %s.",
                     automata_file);

        return IB_EINVAL;
    }

    ia_rc = ia_eudoxus_create_from_path(&eudoxus, automata_file);
    if (ia_rc != IA_EUDOXUS_OK) {
        ib_log_error(cp->ib,
                     MODULE_NAME_STR ": Error loading eudoxus automata file[%d]: %s.",
                     ia_rc, automata_file);
        return IB_EINVAL;
    }

    rc = ib_hash_set(eudoxus_pattern_hash, pattern_name, eudoxus);
    if (rc != IB_OK) {
        ia_eudoxus_destroy(eudoxus);
        return rc;
    }

    return IB_OK;
}

/**
 * Eudoxus first match callback function.  Called when a match occurs.
 *
 * Always returns IA_EUDOXUS_CMD_STOP to stop matching (unless an
 * error occurs). If capture is enabled the matched text will be stored in the
 * capture variable.
 *
 * @param[in] engine Eudoxus engine.
 * @param[in] output Output defined by automata.
 * @param[in] output_length Length of output.
 * @param[in] input Current location in the input (first character
 *                  after the match).
 * @param[in,out] cbdata Pointer to the ee_callback_data_t instance we are
 *                       handling. This is needed for handling capture
 *                       of the match.
 * @return IA_EUDOXUS_CMD_ERROR on error, IA_EUDOXUS_CMD_STOP otherwise.
 */
static
ia_eudoxus_command_t ee_first_match_callback(ia_eudoxus_t* engine,
                                             const char *output,
                                             size_t output_length,
                                             const uint8_t *input,
                                             void *cbdata)
{
    assert(cbdata != NULL);
    assert(output != NULL);

    ib_status_t rc;
    uint32_t match_len;
    const ee_callback_data_t *ee_cbdata = cbdata;
    ib_tx_t *tx = ee_cbdata->tx;
    ib_field_t *capture = ee_cbdata->capture;
    ib_bytestr_t *bs;
    ib_field_t *field;
    const char *name;

    assert(tx != NULL);

    if (capture != NULL) {
        if (output_length != sizeof(uint32_t)) {
            return IA_EUDOXUS_CMD_ERROR;
        }
        match_len = *(uint32_t *)(output);
        rc = ib_capture_clear(capture);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error clearing captures: %s",
                            ib_status_to_string(rc));
            return IA_EUDOXUS_CMD_ERROR;
        }
        /* Create a byte-string representation */
        rc = ib_bytestr_dup_mem(&bs,
                                tx->mp,
                                (input - match_len),
                                match_len);
        if (rc != IB_OK) {
            return IA_EUDOXUS_CMD_ERROR;
        }
        name = ib_capture_name(0);
        rc = ib_field_create(&field, tx->mp, name, strlen(name),
                             IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bs));
        if (rc != IB_OK) {
            return IA_EUDOXUS_CMD_ERROR;
        }
        rc = ib_capture_set_item(capture, 0, tx->mp, field);
        if (rc != IB_OK) {
            return IA_EUDOXUS_CMD_ERROR;
        }
    }

    return IA_EUDOXUS_CMD_STOP;
}

/**
 * Create an instance of the @c ee_match_any operator.
 *
 * Looks up the automata name and adds the automata to the operator instance.
 *
 * @param[in] ctx Current context.
 * @param[in] parameters Automata name.
 * @param[out] instance_data Instance data.
 * @param[in] cbdata Callback data.
 */
static
ib_status_t ee_match_any_operator_create(
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);
    assert(parameters != NULL);
    assert(instance_data != NULL);

    ib_status_t rc;
    ia_eudoxus_t* eudoxus;
    ee_operator_data_t *operator_data;
    ib_module_t *module;
    ib_engine_t *ib = ib_context_get_engine(ctx);
    ib_mpool_t *pool = ib_context_get_mpool(ctx);
    const ee_config_t *config = ee_get_config(ib);
    const ib_hash_t *eudoxus_pattern_hash;

    assert(config != NULL);
    assert(config->eudoxus_pattern_hash != NULL);

    /* Get my module object */
    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error getting eudoxus operator module object: %s",
                     ib_status_to_string(rc));
        return rc;
    }
    /* Allocate a rule data object, populate it */
    operator_data = ib_mpool_alloc(pool, sizeof(*operator_data));
    if (operator_data == NULL) {
        return IB_EALLOC;
    }

    eudoxus_pattern_hash = config->eudoxus_pattern_hash;

    rc = ib_hash_get(eudoxus_pattern_hash, &eudoxus, parameters);
    if (rc == IB_ENOENT ) {
        ib_log_error(ib,
                     MODULE_NAME_STR ": No eudoxus automata named %s found.",
                     parameters);
        return rc;
    }
    else if (rc != IB_OK) {
        ib_log_error(ib,
                     MODULE_NAME_STR ": Failed to setup eudoxus automata operator.");
        return rc;
    }

    operator_data->eudoxus = eudoxus;
    *(ee_operator_data_t **)instance_data = operator_data;

    return IB_OK;
}

/**
 * Helper function for stream and non-stream execution.
 *
 * @param[in] tx Transaction
 * @param[in] operator_data Operator data.
 * @param[in] data Per-transaction data for this operator instance.
 * @param[in] field Input field.
 * @param[in] capture Where to capture.
 * @param[out] result Result of execution.
 */
static
ib_status_t ee_match_any_operator_execute_common(
    ib_tx_t *tx,
    ee_operator_data_t *operator_data,
    ee_tx_data_t *data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result
)
{
    ib_status_t rc;
    ia_eudoxus_result_t ia_rc;
    ia_eudoxus_state_t* state = NULL;
    const char *input;
    size_t input_len;

    assert(tx != NULL);
    assert(operator_data != NULL);
    assert(data != NULL);

    *result = 0;

    if (field->type == IB_FTYPE_NULSTR) {
        rc = ib_field_value(field, ib_ftype_nulstr_out(&input));
        if (rc != IB_OK) {
            return rc;
        }
        input_len = strlen(input);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            return rc;
        }
        input = (const char *)ib_bytestr_const_ptr(bs);
        input_len = ib_bytestr_length(bs);
    }
    else if (field->type == IB_FTYPE_LIST) {
        return IB_ENOTIMPL;
    }
    else {
        return IB_EINVAL;
    }

    if (data->end_of_automata) {
        /* Nothing to do. */
        return IB_OK;
    }

    /* Run eudoxus */
    state = data->eudoxus_state;
    rc = IB_OK;

    ia_rc = ia_eudoxus_execute(state, (const uint8_t *)input, input_len);
    if (ia_rc == IA_EUDOXUS_STOP) {
        *result = 1;
        rc = IB_OK;
    }
    else if (ia_rc == IA_EUDOXUS_END) {
        data->end_of_automata = true;
        rc = IB_OK;
    }
    else if (ia_rc != IA_EUDOXUS_OK) {
        rc = IB_EUNKNOWN;
    }

    return rc;
}

/**
 * Execute the @c ee_match_any operator.
 *
 * At first match the operator will stop searching and return true.
 *
 * The capture option is supported; the matched pattern will be placed in the
 * capture variable if a match occurs.
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 */
static
ib_status_t ee_match_any_operator_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ia_eudoxus_result_t ia_rc;
    ee_operator_data_t *operator_data = instance_data;
    ia_eudoxus_t* eudoxus = operator_data->eudoxus;

    assert(tx != NULL);
    assert(instance_data != NULL);

    *result = 0;

    /* Not streaming, so create data for this use only */
    ee_callback_data_t local_cbdata = { tx, capture };
    ee_tx_data_t local_data;

    ia_rc = ia_eudoxus_create_state(&local_data.eudoxus_state,
                                    eudoxus,
                                    ee_first_match_callback,
                                    (void *)&local_cbdata);
    if (ia_rc != IA_EUDOXUS_OK) {
        if (local_data.eudoxus_state != NULL) {
            ia_eudoxus_destroy_state(local_data.eudoxus_state);
        }
        return IB_EINVAL;
    }
    local_data.end_of_automata = false;

    return ee_match_any_operator_execute_common(
        tx, operator_data, &local_data, field, capture, result
    );
}

/**
 * Execute the @c ee_match_any operator in a streaming fashion.
 *
 * See ee_match_any_operator_execute().
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 */
static
ib_status_t ee_match_any_operator_execute_stream(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;
    ia_eudoxus_result_t ia_rc;
    ee_operator_data_t *operator_data = instance_data;
    ia_eudoxus_t* eudoxus = operator_data->eudoxus;
    ee_tx_data_t* data = NULL;
    ee_callback_data_t *ee_cbdata;
    const ib_module_t *m = (const ib_module_t *)cbdata;

    assert(m != NULL);
    assert(tx != NULL);
    assert(instance_data != NULL);

    *result = 0;

    /* Persist data. */
    rc = get_ee_tx_data(m, tx, operator_data, &data);
    if (rc == IB_ENOENT) {
        /* Data not found create it */
        data = ib_mpool_alloc(tx->mp, sizeof(*data));
        ee_cbdata = ib_mpool_alloc(tx->mp, sizeof(*ee_cbdata));
        if (ee_cbdata == NULL) {
            return IB_EALLOC;
        }
        ee_cbdata->tx = tx;
        ee_cbdata->capture = capture;
        ia_rc = ia_eudoxus_create_state(&data->eudoxus_state,
                                        eudoxus,
                                        ee_first_match_callback,
                                        (void *)ee_cbdata);
        if (ia_rc != IA_EUDOXUS_OK) {
            if (data->eudoxus_state != NULL) {
                ia_eudoxus_destroy_state(data->eudoxus_state);
                data->eudoxus_state = NULL;
            }
            return IB_EINVAL;
        }
        data->end_of_automata = false;
        set_ee_tx_data(m, tx, operator_data, data);
    }
    else if (rc != IB_OK) {
        /* Error getting the state -- abort */
        return rc;
    }

    return ee_match_any_operator_execute_common(
        tx, operator_data, data, field, capture, result
    );
}

/**
 * Destroy the eudoxus state when the transaction is complete.
 *
 * After the transaction is complete iterate over all of the states create
 * during the transaction and destroy them.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Current transaction.
 * @param[in] event Event type (should always be @ref tx_finished_event)
 * @param[in] cbdata Callback data -- pointer to this module (@ref ib_module_t).
 *
 * @returns IB_OK on success.
 */
static
ib_status_t ee_tx_finished_handler(ib_engine_t *ib,
                                   ib_tx_t *tx,
                                   ib_state_event_type_t event,
                                   void *cbdata)
{
    ib_status_t rc;
    ib_hash_t *hash;
    ib_mpool_t *pool;
    const ib_module_t *m = (const ib_module_t *)cbdata;
    ee_tx_data_t *data;
    ib_hash_iterator_t *iterator;

    rc = ib_tx_get_module_data(tx, m, &hash);
    if (rc != IB_OK || hash == NULL) {
        return rc;
    }

    rc = ib_mpool_create(&pool, "temp", NULL);
    if (rc != IB_OK) {
        return rc;
    }

    iterator = ib_hash_iterator_create(pool);
    if (iterator == NULL) {
        ib_mpool_destroy(pool);
        return IB_EALLOC;
    }
    for (
        ib_hash_iterator_first(iterator, hash);
        ! ib_hash_iterator_at_end(iterator);
        ib_hash_iterator_next(iterator)
    ) {
        ib_hash_iterator_fetch(NULL, NULL, &data, iterator);
        if (data->eudoxus_state != NULL) {
            ia_eudoxus_destroy_state(data->eudoxus_state);
            data->eudoxus_state = NULL;
        }
    }

    ib_mpool_destroy(pool);

    return IB_OK;
}


/**
 * Initialize the eudoxus operator module.
 *
 * Registers the operators and the hash for storing the eudoxus engine
 * instances created by the LoadEudoxus directive.
 *
 * @param[in] ib Ironbee engine.
 * @param[in] m Module instance.
 * @param[in] cbdata Not used.
 */
static
ib_status_t ee_module_init(ib_engine_t *ib,
                           ib_module_t *m,
                           void        *cbdata)
{
    ib_status_t rc;
    ib_mpool_t *main_mp;
    ib_mpool_t *mod_mp;
    ee_config_t *config;

    main_mp = ib_engine_pool_main_get(ib);
    config = ee_get_config(ib);
    assert(config != NULL);

    rc = ib_mpool_create(&mod_mp, "ee_module", main_mp);
    if (rc != IB_OK ) {
        return rc;
    }
    if (config->eudoxus_pattern_hash == NULL) {
        rc = ib_hash_create_nocase(&(config->eudoxus_pattern_hash), mod_mp);
        if (rc != IB_OK ) {
            return rc;
        }
    }

    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "ee_match_any",
        IB_OP_CAPABILITY_CAPTURE,
        &ee_match_any_operator_create, NULL,
        NULL, NULL,
        &ee_match_any_operator_execute, m
    );
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Error registering ee_match_any operator: %s",
            ib_status_to_string(rc));
        return rc;
    }
    rc = ib_operator_stream_create_and_register(
        NULL,
        ib,
        "ee_match_any",
        IB_OP_CAPABILITY_CAPTURE,
        &ee_match_any_operator_create, NULL,
        NULL, NULL,
        &ee_match_any_operator_execute_stream, m
    );
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Error registering ee_match_any stream operator: %s",
            ib_status_to_string(rc));
        return rc;
    }

    rc = ib_hook_tx_register(ib,
                             tx_finished_event,
                             ee_tx_finished_handler,
                             m);

    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Error registering transaction finished event for ee_match_any operator: %s",
            ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
}

/**
 * Release resources when the module is unloaded.
 *
 * All eudoxus engines created by the LoadEudoxus directive are destroyed.
 *
 * @param[in] ib Ironbee engine.
 * @param[in] m Module instance.
 * @param[in] cbdata Not used.
 */
static
ib_status_t ee_module_finish(ib_engine_t *ib,
                             ib_module_t *m,
                             void        *cbdata)
{
    ib_status_t rc;
    ia_eudoxus_t *eudoxus;
    ib_mpool_t *pool;
    const ee_config_t *config = ee_get_config(ib);
    ib_hash_t *eudoxus_pattern_hash;
    ib_hash_iterator_t *iterator;

    if (
        config                       == NULL ||
        config->eudoxus_pattern_hash == NULL
    ) {
        return IB_OK;
    }

    eudoxus_pattern_hash = config->eudoxus_pattern_hash;

    rc = ib_mpool_create(&pool, "temp", NULL);
    if (rc != IB_OK) {
        return rc;
    }

    iterator = ib_hash_iterator_create(pool);
    if (iterator == NULL) {
        ib_mpool_destroy(pool);
        return IB_EALLOC;
    }
    for (
        ib_hash_iterator_first(iterator, eudoxus_pattern_hash);
        ! ib_hash_iterator_at_end(iterator);
        ib_hash_iterator_next(iterator)
    ) {
        ib_hash_iterator_fetch(NULL, NULL, &eudoxus, iterator);
        if (eudoxus != NULL) {
            ia_eudoxus_destroy(eudoxus);
        }
    }
    ib_hash_clear(eudoxus_pattern_hash);
    ib_mpool_release(pool);

    return IB_OK;
}

/**
 * Initial values of @ref ee_config_t.
 *
 * This static will *only* be passed to IronBee as part of module
 * definition.  It will never be read or written by any code in this file.
 */
static ee_config_t g_ee_config = {NULL};

#ifndef DOXYGEN_SKIP
static IB_DIRMAP_INIT_STRUCTURE(eudoxus_directive_map) = {
    IB_DIRMAP_INIT_PARAM2(
        "LoadEudoxus",
        load_eudoxus_pattern_param2,
        NULL
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,            /**< Default metadata */
    MODULE_NAME_STR,                      /**< Module name */
    IB_MODULE_CONFIG(&g_ee_config),       /**< Global config data */
    NULL,                                 /**< Configuration field map */
    eudoxus_directive_map,                /**< Config directive map */
    ee_module_init,                       /**< Initialize function */
    NULL,                                 /**< Callback data */
    ee_module_finish,                     /**< Finish function */
    NULL,                                 /**< Callback data */
);
#endif
