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
 * @brief IronBee --- PCRE Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/bytestr.h>
#include <ironbee/capture.h>
#include <ironbee/cfgmap.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/mm.h>
#include <ironbee/module.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>
#include <ironbee/transformation.h>
#include <ironbee/type_convert.h>
#include <ironbee/util.h>
#include <ironbee/json.h>

#include <ironbee_config_auto_gen.h>

#include <pcre.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <limits.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        pcre
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* How many matches will PCRE find and populate. */
#define MATCH_MAX 10

/* Port forward the pcre constant PCRE_PARTIAL as PCRE_PARTIAL_SOFT. */
#ifndef PCRE_PARTIAL_SOFT
#define PCRE_PARTIAL_SOFT PCRE_PARTIAL
#endif

/**
 * From pcreapi man page.
 */
#define WORKSPACE_SIZE_MIN     (20)

/**
 * Build a reasonable buffer size.
 */
#define WORKSPACE_SIZE_DEFAULT (WORKSPACE_SIZE_MIN * 10)

/* Define the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Module Configuration Structure.
 */
struct modpcre_cfg_t {
    ib_num_t       study;                 /**< Bool: Study compiled regexs */
    ib_num_t       use_jit;               /**< Bool: Use JIT if available */
    ib_num_t       match_limit;           /**< Match limit */
    ib_num_t       match_limit_recursion; /**< Match recursion depth limit */
    ib_num_t       jit_stack_start;       /**< Starting JIT stack size */
    ib_num_t       jit_stack_max;         /**< Max JIT stack size */
    ib_num_t       dfa_workspace_size;    /**< Size of DFA workspace */
};
typedef struct modpcre_cfg_t modpcre_cfg_t;

/**
 * Internal representation of PCRE compiled patterns.
 */
struct modpcre_cpat_data_t {
    ib_module_t         *module;          /**< Pointer to this module. */
    const pcre          *cpatt;           /**< Compiled pattern */
    const pcre_extra    *edata;           /**< PCRE Study data */
    const char          *patt;            /**< Regex pattern text */
    bool                 is_dfa;          /**< Is this a DFA? */
    bool                 is_jit;          /**< Is this JIT compiled? */
    int                  dfa_ws_size;     /**< Size of DFA workspace */
};
typedef struct modpcre_cpat_data_t modpcre_cpat_data_t;

/**
 * PCRE and DFA rule data types are an alias for the compiled pattern structure.
 */
struct modpcre_operator_data_t {
    modpcre_cpat_data_t *cpdata;          /**< Compiled pattern data */
    const char          *id;              /**< ID for DFA rules */
};
typedef struct modpcre_operator_data_t modpcre_operator_data_t;

/* Instantiate a module global configuration. */
static modpcre_cfg_t modpcre_global_cfg = {
    1,                     /* study. */
    1,                     /* use_jit. */
    5000,                  /* match_limit. */
    5000,                  /* match_limit_recursion. */
    32 * 1024,             /* jit_stack_start. */
    1000 * 1024,           /* jit_stack_max. */
    WORKSPACE_SIZE_DEFAULT /* dfa_workspace_size. */
};

/* State information for a PCRE work common to all pcre operators in a tx. */
struct pcre_tx_data_t {
    pcre_jit_stack *stack;
    ib_hash_t      *dfa_workspace_hash;
    int            *ovector;    /** Array of N matches that is 3 * N long. */
    int             ovector_sz; /* The size of ovector. 3 * N. */
};
typedef struct pcre_tx_data_t pcre_tx_data_t;

#ifdef PCRE_HAVE_JIT
static void pcre_jit_stack_cleanup(void *stack) {
    assert(stack != NULL);
    pcre_jit_stack_free((pcre_jit_stack *)stack);
}
#endif


/**
 * A custom logger to log a regex pattern and a field with a message.
 *
 * @param[in] tx Transaction.
 * @param[in] level The logging level.
 * @param[in] file The file name. @c __FILE__.
 * @param[in] func The name of the function. @c __func__.
 * @param[in] line The line in the file. __LINE__.
 * @param[in] msg The message to prefix this log message with.
 * @param[in] data The operator data. This contains the pattern.
 * @param[in] subject The subject to check.
 */
static void pcre_log_tx(
    ib_tx_t                 *tx,
    ib_logger_level_t        level,
    const char              *file,
    const char              *func,
    int                      line,
    const char              *msg,
    modpcre_operator_data_t *data,
    const ib_field_t        *subject
)
{
    assert(tx != NULL);
    assert(msg != NULL);
    assert(data != NULL);
    assert(data->cpdata != NULL);
    assert(data->cpdata->patt != NULL);
    assert(subject != NULL);

    ib_status_t rc;

    char *field;
    size_t field_sz;

    rc = ib_json_encode_field(
        tx->mm,
        subject,
        false,
        &field,
        &field_sz
    );
    if (rc != IB_OK || field_sz > INT_MAX) {
        ib_log_tx_ex(
            tx,
            level,
            file,
            func,
            line,
            "%s for pattern /%s/.",
            msg,
            data->cpdata->patt
        );
    }
    else {
        ib_log_tx_ex(
            tx,
            level,
            file,
            func,
            line,
            "%s for pattern /%s/ against subject %.*s",
            msg,
            data->cpdata->patt,
            (int) field_sz,
            field
        );
    }
}

#define pcre_log_error(tx, ...) pcre_log_tx(tx, IB_LOG_ERROR, __FILE__, __func__, __LINE__, __VA_ARGS__)

#define pcre_log_debug(tx, ...) pcre_log_tx(tx, IB_LOG_DEBUG, __FILE__, __func__, __LINE__, __VA_ARGS__)

/**
 * Get or create an ib_hash_t inside of @c tx for storing dfa rule data.
 *
 * The hash is stored at the key @c HASH_NAME_STR.
 *
 * @param[in] m  PCRE module.
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
ib_status_t get_or_create_operator_data(
    const ib_module_t  *m,
    ib_tx_t            *tx,
    pcre_tx_data_t    **data
)
{
    assert(tx);

    ib_status_t     rc;
    pcre_tx_data_t *data_tmp;

    /* Get or create the hash that contains the rule data. */
    rc = ib_tx_get_module_data(tx, m, data);
    if ( (rc == IB_OK) && (*data != NULL) ) {
        return IB_OK;
    }

    data_tmp = ib_mm_alloc(tx->mm, sizeof(*data_tmp));
    if (data_tmp == NULL) {
        return IB_EALLOC;
    }

    /* Create the DFA Hash. */
    {
        rc = ib_hash_create(&data_tmp->dfa_workspace_hash, tx->mm);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_tx_set_module_data(tx, m, data_tmp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Create capture buffer that DFA and PCRE may use. */
    {
        /* We could statically allocate this but it is "cheap" to leave
         * dynamic and allows us to tweak match size later.
         * For now it remains at 10. */
        data_tmp->ovector_sz = 10 * 3;
        data_tmp->ovector = (int *)ib_mm_alloc(
            tx->mm,
            data_tmp->ovector_sz*sizeof(*data_tmp->ovector)
        );

        if (data_tmp->ovector == NULL) {
            return IB_EALLOC;
        }
    }

    /* Initialize the PCRE JIT Stack for the TX. */
#ifdef PCRE_HAVE_JIT
    {
        modpcre_cfg_t  *config;

        rc = ib_context_module_config(tx->ctx, m, &config);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Cannot fetch module config for pcre.");
            return rc;
        }

        data_tmp->stack = pcre_jit_stack_alloc(
            config->jit_stack_start,
            config->jit_stack_max
        );
        /* A null stack is extremely unexpected, but not fatal.
         * JIT can use a callstack in a threadsafe way. */
        if (data_tmp->stack == NULL) {
            ib_log_info_tx(
                tx,
                "Could not allocate a pcre JIT stack: min=%d max=%d",
                (int)config->jit_stack_start,
                (int)config->jit_stack_max
            );
        }
        else {
            rc = ib_mm_register_cleanup(
                tx->mm,
                pcre_jit_stack_cleanup,
                data_tmp->stack
            );
            if (rc != IB_OK) {
                pcre_jit_stack_free(data_tmp->stack);
                return rc;
            }
        }
    }
#endif

    *data = data_tmp;

    return IB_OK;
}

/**
 * An adapter function to allow freeing of pcre_extra data via mpool callbacks.
 */
static void pcre_free_study_wrapper(void *edata)
{
    if (edata != NULL) {
#ifdef HAVE_PCRE_FREE_STUDY
        pcre_free_study((pcre_extra *)edata);
#else
        pcre_free(edata);
#endif
    }
}

/**
 * Given cpdata, populate cdata and cdata_sz.
 */
static ib_status_t compile_pattern(
    ib_engine_t         *ib,
    modpcre_cpat_data_t *cpdata,
    ib_mm_t               mm,
    const char           *patt,
    const char          **errptr,
    int                  *erroffset
)
{
    /* How cpatt is produced. */
    const int compile_flags = PCRE_DOTALL | PCRE_DOLLAR_ENDONLY;

    pcre *cpatt;

    /* Common to all code, compile. */
    cpatt = pcre_compile(patt, compile_flags, errptr, erroffset, NULL);
    if (*errptr != NULL) {
        ib_log_error(ib, "Error compiling PCRE pattern \"%s\": %s at offset %d",
                     patt, *errptr, *erroffset);
        return IB_EINVAL;
    }
    ib_mm_register_cleanup(mm, pcre_free, cpatt);

    /* Alias cpatt as the read-only cpatt value. */
    cpdata->cpatt = cpatt;

    /* Copy pattern. */
    cpdata->patt = ib_mm_strdup(mm, patt);
    if (cpdata->patt == NULL) {
        ib_log_error(ib, "Failed to duplicate pattern string: %s", patt);
        return IB_EALLOC;
    }

    return IB_OK;
}

/**
 * Internal compilation of the modpcre pattern.
 *
 * @param[in] ib IronBee engine for logging.
 * @param[in] mm The memory manager to allocate memory out of.
 * @param[in] config Module configuration
 * @param[in] is_dfa Set to true for DFA
 * @param[out] pcpdata Pointer to new struct containing the compilation.
 * @param[in] patt The uncompiled pattern to match.
 * @param[out] errptr Pointer to an error message describing the failure.
 * @param[out] erroffset The location of the failure, if this fails.
 *
 * @returns IronBee status. IB_EINVAL if the pattern is invalid,
 *          IB_EALLOC if memory allocation fails or IB_OK.
 */
static ib_status_t pcre_compile_internal(
    ib_module_t          *module,
    ib_engine_t          *ib,
    ib_mm_t               mm,
    const modpcre_cfg_t  *config,
    bool                  is_dfa,
    modpcre_cpat_data_t **pcpdata,
    const char           *patt,
    const char          **errptr,
    int                  *erroffset
)
{
    assert(ib != NULL);
    assert(config != NULL);
    assert(pcpdata != NULL);
    assert(patt != NULL);

    /* Pattern data structure we'll create */
    modpcre_cpat_data_t *cpdata;

    ib_status_t ib_rc;

#ifdef PCRE_HAVE_JIT
    /* Are we using JIT? */
    bool use_jit;

    if (config->use_jit == 0) {
        use_jit = false;
    }
    else {
        use_jit = !is_dfa;
    }

    /* Do we want to be using JIT? */
    const bool want_jit = use_jit;
#else
    const bool use_jit = false;
#endif /* PCRE_HAVE_JIT */

    cpdata = (modpcre_cpat_data_t *)ib_mm_calloc(mm, sizeof(*cpdata), 1);
    if (cpdata == NULL) {
        ib_log_error(ib,
                     "Failed to allocate cpdata of size: %zd",
                     sizeof(*cpdata));
        return IB_EALLOC;
    }

    cpdata->module = module;

    /* Populate cpdata->cpatt and cpdata->patt. */
    ib_rc = compile_pattern(ib, cpdata, mm, patt, errptr, erroffset);
    if (ib_rc != IB_OK) {
        return ib_rc;
    }

    /* How do we study the pattern? */
    if (config->study) {
        pcre_extra *edata;

#ifdef PCRE_HAVE_JIT
        if (use_jit) {
            edata = pcre_study(cpdata->cpatt, PCRE_STUDY_JIT_COMPILE, errptr);
            if (*errptr != NULL)  {
                use_jit = false;
                ib_log_warning(ib, "PCRE-JIT study failed: %s", *errptr);
            }
            ib_mm_register_cleanup(mm, pcre_free_study_wrapper, edata);
            cpdata->edata = edata;
        }
        else
#endif
        {
            edata = pcre_study(cpdata->cpatt, 0, errptr);
            if (*errptr != NULL)  {
                ib_log_error(ib, "PCRE study failed: %s", *errptr);
                return IB_EINVAL;
            }
            ib_mm_register_cleanup(mm, pcre_free_study_wrapper, edata);
            cpdata->edata = edata;
        }

        /* If we successfully built an edata value above, tweak it.
         * NOTE: We edit the edata pointer as cbdata->edata is const. */
        if (edata != NULL) {
            /* Set the PCRE limits for non-DFA patterns */
            if (! is_dfa) {
                edata->flags |=
                    (PCRE_EXTRA_MATCH_LIMIT | PCRE_EXTRA_MATCH_LIMIT_RECURSION);
                edata->match_limit =
                    (unsigned long)config->match_limit;
                edata->match_limit_recursion =
                    (unsigned long)config->match_limit_recursion;
                cpdata->dfa_ws_size = 0;
            }
            else {
                edata->match_limit = 0U;
                edata->match_limit_recursion = 0U;
                cpdata->dfa_ws_size = (int)config->dfa_workspace_size;
            }
        }
    } /* Close if (config->study) */
#ifdef PCRE_HAVE_JIT
    else if (use_jit) {
        ib_log_warning(ib, "PCRE: Disabling JIT because study disabled");
        use_jit = false;
    }
#endif

#ifdef PCRE_HAVE_JIT
    /* The check to see if JIT compilation was a success changed in 8.20RC1
       now uses pcre_fullinfo see doc/pcrejit.3 */
    if (use_jit) {
        int rc;
        int pcre_jit_ret;

        rc = pcre_fullinfo(
            cpdata->cpatt,
            cpdata->edata,
            PCRE_INFO_JIT,
            &pcre_jit_ret);
        /* Error calling pcre_fullinfo. This should not happen. */
        if (rc != 0) {
            ib_log_error(ib, "PCRE-JIT failed to get pcre_fullinfo");
            use_jit = false;
        }
        /* The answer. JIT != 1, jit was not used. */
        else if (pcre_jit_ret != 1) {
            ib_log_info(ib, "PCRE-JIT compiler does not support: %s", patt);
            use_jit = false;
        }
        else {
            /* pcre_jit_ret == 1. Do nothing */
        }
    }

    if (want_jit && !use_jit) {
        ib_log_info(ib, "Falling back to normal PCRE");
    }
#endif /*PCRE_HAVE_JIT*/

    cpdata->is_dfa = is_dfa;
    cpdata->is_jit = use_jit;

    /* Assert that in call cases:
     *   - if this is not jit, we don't care about edata.
     *   - if this *is* jit, edata must be defined.
     */
    assert((!cpdata->is_jit) || (cpdata->is_jit && cpdata->edata != NULL));

    ib_log_trace(ib,
                 "Compiled PCRE pattern \"%s\": "
                 "limit=%ld rlimit=%ld "
                 "dfa=%s dfa-ws-sz=%d "
                 "jit=%s",
                 patt,
                 (cpdata->edata==NULL)? 0L : cpdata->edata->match_limit,
                 (cpdata->edata==NULL)? 0L : cpdata->edata->match_limit_recursion,
                 cpdata->is_dfa ? "yes" : "no",
                 cpdata->dfa_ws_size,
                 cpdata->is_jit ? "yes" : "no");
    *pcpdata = cpdata;

    return IB_OK;
}

/**
 * Create the PCRE operator.
 *
 * @param[in] ctx Current context.
 * @param[in] mm Memory manager.
 * @param[in] parameters Unparsed string with the parameters to
 *                       initialize the operator instance.
 * @param[out] instance_data Instance data.
 * @param[in] cbdata Callback data.
 *
 * @returns IB_OK on success or IB_EALLOC on any other type of error.
 */
static
ib_status_t pcre_operator_create(
    ib_context_t *ctx,
    ib_mm_t       mm,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx           != NULL);
    assert(parameters    != NULL);
    assert(instance_data != NULL);

    ib_engine_t *ib = ib_context_get_engine(ctx);
    assert(ib != NULL);

    modpcre_cpat_data_t *cpdata = NULL;
    modpcre_operator_data_t *operator_data = NULL;
    ib_module_t *module;
    modpcre_cfg_t *config;
    ib_status_t rc;
    const char *errptr;
    int erroffset;

    if (parameters == NULL) {
        ib_log_error(ib, "No pattern for operator.");
        return IB_EINVAL;
    }

    /* Get my module object */
    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error getting pcre module object: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Get the context configuration */
    rc = ib_context_module_config(ctx, module, &config);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error getting pcre module configuration: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Compile the pattern.  Note that the rule data is an alias for
     * the compiled pattern type */
    rc = pcre_compile_internal(module,
                               ib,
                               mm,
                               config,
                               false,
                               &cpdata,
                               parameters,
                               &errptr,
                               &erroffset);
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate a rule data object, populate it */
    operator_data = ib_mm_alloc(mm, sizeof(*operator_data));
    if (operator_data == NULL) {
        return IB_EALLOC;
    }
    operator_data->cpdata = cpdata;
    operator_data->id = NULL;           /* Not needed for rx rules */

    /* Rule data is an alias for the compiled pattern data */
    *(modpcre_operator_data_t **)instance_data = operator_data;

    return rc;
}

/**
 * Set the matches into the given field name as .0, .1, .2 ... .9.
 *
 * @param[in] tx Current transaction.
 * @param[in] capture Collection to capture to.
 * @param[in] ovector The vector of integer pairs of matches from PCRE.
 * @param[in] matches The number of matches.
 * @param[in] subject The matched-against string data.
 *
 * @returns IB_OK or IB_EALLOC.
 */
static
ib_status_t pcre_set_matches(
    const ib_tx_t *tx,
    ib_field_t    *capture,
    int           *ovector,
    int            matches,
    const char    *subject
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(capture != NULL);
    assert(ovector != NULL);

    ib_status_t rc;
    int i;

    rc = ib_capture_clear(capture);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error clearing captures: %s",
                        ib_status_to_string(rc));
    }

    /* We have a match! Now populate TX:0-9 in tx->data. */
    for (i = 0; i < matches; ++i)
    {
        /* The length of the match. */
        size_t match_len;

        /* The first character in the match. */
        const char *match_start;

        /* Field name */
        const char *name;

        /* Holder for a copy of the field value when creating a new field. */
        ib_bytestr_t *bs;

        /* Field holder. */
        ib_field_t *field;

        /* Readability. Mark the start and length of the string. */
        match_start = subject+ovector[i*2];
        match_len = ovector[i*2+1] - ovector[i*2];

        /* Create a byte-string representation */
        rc = ib_bytestr_dup_mem(&bs,
                                tx->mm,
                                (const uint8_t*)match_start,
                                match_len);
        if (rc != IB_OK) {
            return rc;
        }

        /* Create a field to hold the byte-string */
        name = ib_capture_name(i);
        rc = ib_field_create(&field, tx->mm, name, strlen(name),
                             IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bs));
        if (rc != IB_OK) {
            return rc;
        }

        /* Add it to the capture collection */
        rc = ib_capture_set_item(capture, i, tx->mm, field);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/* State information for a DFA's work. */
struct dfa_workspace_t {
    int    *workspace;  /**< An array of integers that DFA is using. */
    int     wscount;    /**< The count in dfa_workspace_t::workspace. */
    int     options;    /**< Options used in previous calls. */

    //! The size of the string stored at dfa_workspace_t::partial.
    size_t  partial_sz;

    /**
     * A partial match data for DFA capturing operator.
     *
     * This is NULL initially, otherwise equal to the contents of the
     * previous partial match.
     *
     * This is cleared when the partial match is completed and reported.
     */
    const char *partial;
};
typedef struct dfa_workspace_t dfa_workspace_t;

/**
 * Append the range @a ovector[0] to @a ovector[1] into @a operator_data.
 *
 * This is used when a partial match is found. The resulting
 * data is recorded into @a operator_data and emitted
 * when the partial match is matched in later
 * calls to the dfa stream operator.
 *
 * @param[in] tx The transaction.
 * @param[in] ovector The vector in which the first two elements
 *            are the partial match values.
 * @param[in] subject The text for which @a ovector[0] and @a ovector[1] are
 *            offsets into, per the pcre API documentation.
 * @param[in] dfa_workspace Contains previous partial match data, if any.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On memory errors.
 */
ib_status_t pcre_dfa_record_partial(
    ib_tx_t         *tx,
    int             *ovector,
    const char      *subject,
    dfa_workspace_t *dfa_workspace
)
{
    assert(tx != NULL);
    assert(ovector != NULL);
    assert(subject != NULL);
    assert(dfa_workspace != NULL);

    const size_t subject_len = ovector[1] - ovector[0];
    char *partial =
        ib_mm_alloc(
            tx->mm,
            sizeof(*partial) *
            (subject_len + dfa_workspace->partial_sz));

    if (partial == NULL) {
        return IB_EALLOC;
    }

    memcpy(partial, dfa_workspace->partial, dfa_workspace->partial_sz);
    memcpy(
        partial + dfa_workspace->partial_sz,
        subject + ovector[0],
        subject_len);

    /* Commit results back. */
    dfa_workspace->partial = partial;
    dfa_workspace->partial_sz += subject_len;

    return IB_OK;
}

/**
 * Clear the partially data written by pcre_dfa_record_partial().
 *
 * @param[in] dfa_workspace The workspace whose partial record to clear.
 *
 */
void pcre_dfa_clear_partial(
    dfa_workspace_t *dfa_workspace
)
{
    assert(dfa_workspace != NULL);

    dfa_workspace->partial = NULL;
    dfa_workspace->partial_sz = 0;
}

/**
 * Set the matches from a multi-match dfa as a list in the CAPTURE
 * collection (all with "0" key).
 *
 * @param[in] tx Current transaction.
 * @param[in] capture Collection to capture to.
 * @param[in] ovector The vector of integer pairs of matches from PCRE.
 * @param[in] matches The number of matches.
 * @param[in] subject The matched-against string data.
 * @param[in] operator_data Operator context data. Contains previous match.
 * @param[in] dfa_workspace Contains previous partial match data, if any.
 *
 * @returns IB_OK or IB_EALLOC.
 */
static
ib_status_t pcre_dfa_set_match(
    ib_tx_t                 *tx,
    ib_field_t              *capture,
    int                     *ovector,
    int                      matches,
    const char              *subject,
    modpcre_operator_data_t *operator_data,
    dfa_workspace_t         *dfa_workspace
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(capture != NULL);
    assert(ovector != NULL);
    assert(dfa_workspace != NULL);

    size_t        match_len;   /* Length of a match length. */
    const char   *match_start; /* The start of the match in the subject. */
    ib_bytestr_t *bs;          /* Copy the match into this byte string. */
    ib_field_t   *field;       /* Wrap the bytestring into this field. */
    const char   *name;        /* Name the field this name. */
    ib_status_t   rc;          /* Status. */

    /* NOTE: When you read this code, realize that
     *       the largest DFA match is in ovector[0] to ovector[1].
     *       Thus, copying ``subject[ovector[0]]`` to ``subject[ovector[1]]``
     *       encompasses the entire referenced match.
     */

    /* If there is a partial match, it is the prefix of all these matches.
     *
     * This then-block constructs a single subject beginning with the
     * partial match and ending with the text of the longest current
     * match.
     */
    if (dfa_workspace->partial != NULL && dfa_workspace->partial_sz > 0) {
        char *new_subject;
        int  *new_ovector;

        /* Allocate new_subject. */
        new_subject = ib_mm_alloc(
            tx->mm,
            sizeof(*new_subject) *
            (ovector[1] - ovector[0] + dfa_workspace->partial_sz));
        if (new_subject == NULL) {
            return IB_EALLOC;
        }

        /* Allocate new_ovector. */
        new_ovector = ib_mm_alloc(tx->mm, sizeof(*ovector) * matches * 2);
        if (new_ovector == NULL) {
            return IB_EALLOC;
        }

        /* Populate new_subject. */
        memcpy(new_subject, dfa_workspace->partial, dfa_workspace->partial_sz);
        memcpy(
            new_subject + dfa_workspace->partial_sz,
            subject,
            ovector[1]);

        /* Populate new_ovector.  */
        for (int i = 0; i < matches; ++i) {
            new_ovector[i] = 0;
            ++i;
            new_ovector[i] = ovector[i] + dfa_workspace->partial_sz;
        }

        /* Re-point our inputs to the new, larger values. */
        subject                   = new_subject;
        ovector                   = new_ovector;
        pcre_dfa_clear_partial(dfa_workspace);
    }
    /* A small optimization:
     * Instead of copying all byte strings from the subject,
     * we create a copy of subject that we own and alias into it.
     */
     else {
        subject = ib_mm_memdup(
            tx->mm,
            subject,
            sizeof(*subject) * ovector[1]);
    }

    /* Readability. Mark the start and length of the string. */
    match_start = subject + ovector[0];
    match_len   = ovector[1] - ovector[0];

    /* Create a byte string copy representation */
    rc = ib_bytestr_alias_mem(
        &bs,
        tx->mm,
        (const uint8_t*)match_start,
        match_len);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create a field to hold the byte-string */
    name = ib_capture_name(0);
    rc = ib_field_create(
        &field,
        tx->mm,
        IB_S2SL(name),
        IB_FTYPE_BYTESTR,
        ib_ftype_bytestr_in(bs));
    if (rc != IB_OK) {
        return rc;
    }

    /* Add it to the capture collection */
    rc = ib_capture_add_item(capture, field);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Internal method to call pcre_jit_exec() or pcre_exec().
 *
 * The arguments are *almost* the same as pcre_exec() except
 * the first two are replaced with this module's
 * @ref modpcre_cpat_data_t data. This struct contains
 * the pattern, the extra data, and a boolean indicating if this
 * is a JIT pattern or not.
 *
 * If the cpdata->is_jit is true, and PCRE_HAVE_JIT and
 * HAVE_PCRE_JIT_EXEC are defined, then pcre_jit_exec() is used
 * to evaluate the pattern.
 *
 * @param[in] cpdata Module struct holding the patter, extra data and
 *            the is_jit boolean.
 * @param[in] stack JIT execution stack. May be NULL to disable jit exec.
 * @param[in] subject Same as pcre_exec().
 * @param[in] length Same as pcre_exec().
 * @param[in] startoffset Same as pcre_exec().
 * @param[in] options Same as pcre_exec().
 * @param[out] ovector Same as pcre_exec().
 * @param[out] ovecsize Same as pcre_exec().
 *
 * @returns the same value pcre_exec() and pcre_jit_exec().
 *
 * @sa pcre_jit_exec()
 * @sa pcre_exec()
 *
 */
static int pcre_exec_internal(
    const modpcre_cpat_data_t *cpdata,
    pcre_jit_stack            *stack,
    const char                *subject,
    int                        length,
    int                        startoffset,
    int                        options,
    int                       *ovector,
    int                        ovecsize
)
{
#ifdef PCRE_HAVE_JIT
#ifdef HAVE_PCRE_JIT_EXEC
    if (cpdata->is_jit && stack != NULL) {
        return pcre_jit_exec(
            cpdata->cpatt,
            cpdata->edata,
            subject,
            length,
            startoffset,
            options,
            ovector,
            ovecsize,
            stack
        );
    }
    else
#endif /* HAVE_PCRE_JIT_EXEC */
#endif /* PCRE_HAVE_JIT */
    {
        return pcre_exec(
            cpdata->cpatt,
            cpdata->edata,
            subject,
            length,
            startoffset,
            options,
            ovector,
            ovecsize
        );
    }
}


/**
 * Return an error string that describes the failure.
 *
 * @param[in] rc The return code from pcre_exec().
 *
 * @returns A string constant describing the error code.
 */
const char* pcre_error_str(int rc)
{
    switch(rc)
    {
    case PCRE_ERROR_NOMATCH: /* (-1) */
        return "PCRE_ERROR_NOMATCH: No match.";

    case PCRE_ERROR_NULL: /* (-2) */
        return "PCRE_ERROR_NULL: An argument to pcre_exec() was null.";

    case PCRE_ERROR_BADOPTION: /* (-3) */
        return "PCRE_ERROR_BADOPTION: "
               "An unrecognized bit was set in the options argument.";

    case PCRE_ERROR_BADMAGIC: /* (-4) */
        return "PCRE_ERROR_BADMAGIC: Magic number is missing or corrupt.";

    case PCRE_ERROR_UNKNOWN_OPCODE: /* (-5) */
        return "PCRE_ERROR_UNKNOWN_OPCODE: PCRE bug / corrupted pattern.";

    case PCRE_ERROR_NOMEMORY: /* (-6) */
        return "PCRE_ERROR_NOMEMORY: "
               "pcre could not acquire memory. Too many backreferences?";

    case PCRE_ERROR_NOSUBSTRING: /* (-7) */
        return "PCRE_ERROR_NOSUBSTRING: Not used by pcre_exec.";

    case PCRE_ERROR_MATCHLIMIT: /* (-8) */
        return "PCRE_ERROR_MATCHLIMIT: "
               "Backtracking limit was reached. Increase PcreMatchLimit?";

    case PCRE_ERROR_CALLOUT: /* (-9) */
        return "PCRE_ERROR_CALLOUT: Callout function failure.";

    case PCRE_ERROR_BADUTF8: /* (-10) */
        return "PCRE_ERROR_BADUTF8: Invalid UTF-8 in match subject.";

    case PCRE_ERROR_BADUTF8_OFFSET: /* (-11) */
        return "PCRE_ERROR_BADUTF8_OFFSET: "
               "Not pointing at the beginning of a UTF-8 character.";

    case PCRE_ERROR_PARTIAL: /* (-12) */

        return "PCRE_ERROR_PARTIAL: Partial match.";

    case PCRE_ERROR_BADPARTIAL: /* (-13) */
        return "PCRE_ERROR_BADPARTIAL: Unused error code.";

    case PCRE_ERROR_INTERNAL: /* (-14) */
        return "PCRE_ERROR_INTERNAL: PCRE bug or pattern corruption.";

    case PCRE_ERROR_BADCOUNT: /* (-15) */
        return "PCRE_ERROR_BADCOUNT: ovecsize is negative.";

    case PCRE_ERROR_RECURSIONLIMIT: /* (-21) */
        return "PCRE_ERROR_RECURSIONLIMIT: "
               "Recursion limit reached. Increase PcreMatchLimitRecursion?";

    case PCRE_ERROR_BADNEWLINE: /* (-23) */
        return "PCRE_ERROR_BADNEWLINE: "
               "Invalid combination of PCRE_NEWLINE_xxx options.";

#ifdef PCRE_ERROR_BADOFFSET
    case PCRE_ERROR_BADOFFSET: /* (-24) */
        return "PCRE_ERROR_BADOFFSET: startoffset < 0 or > value length.";
#endif

#ifdef PCRE_ERROR_SHORTUTF8
    case PCRE_ERROR_SHORTUTF8: /* (-25) */
        return "PCRE_ERROR_SHORTUTF8: "
               "Subject ends with a trucated UTF-8 character.";
#endif

#ifdef PCRE_ERROR_RECURSELOOP
    case PCRE_ERROR_RECURSELOOP: /* (-26) */
        return "PCRE_ERROR_RECURSELOOP: Recursion loop is detected.";
#endif

#ifdef PCRE_ERROR_JIT_STACKLIMIT
    case PCRE_ERROR_JIT_STACKLIMIT: /* (-27) */
        return "PCRE_ERROR_JIT_STACKLIMIT: "
               "Stack limit reached. Increase PcreJitStackMax?";
#endif

#ifdef PCRE_ERROR_BADMODE
    case PCRE_ERROR_BADMODE: /* (-28) */
        return "PCRE_ERROR_BADMODE: "
               "8, 16 or 32 bit pattern passed to wrong library function.";
#endif

#ifdef PCRE_ERROR_BADENDIANNESS
    case PCRE_ERROR_BADENDIANNESS: /* (-29) */
        return "PCRE_ERROR_BADENDIANNESS: Reloaded pattern is wrong endianess.";
#endif

#ifdef PCRE_ERROR_JIT_BADOPTION
    case PCRE_ERROR_JIT_BADOPTION: /* No number given. */
        return "PCRE_ERROR_JIT_BADOPTION: "
               "Match mode doesn't match compile mode.";
#endif

#ifdef PCRE_ERROR_BADLENGTH
    case PCRE_ERROR_BADLENGTH: /* (-32) */
        return "PCRE_ERROR_BADLENGTH: pcre_exec length argument is negative.";
#endif

       /* These error codes are documented as not being used. */
    case -16:
    case -20:
    case -22:
    case -30:
        return "Unused error code returned from pcre_exec.";
    }

    if (rc >= 0) {
        return "No error.";
    }

    return "Unexpected error code.";
}

/**
 * @brief Execute the PCRE operator
 *
 * @param[in] tx Current transaction.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] cbdata Callback data.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t pcre_operator_execute(
    ib_tx_t          *tx,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *instance_data,
    void             *cbdata
)
{
    assert(instance_data != NULL);
    assert(tx            != NULL);

    int matches;
    ib_status_t ib_rc;
    const char *subject = NULL;
    size_t subject_len = 0;
    const ib_bytestr_t *bytestr;
    modpcre_operator_data_t *operator_data =
        (modpcre_operator_data_t *)instance_data;
    pcre_tx_data_t *tx_data;


    assert(operator_data->cpdata->is_dfa == false);

    if (! field) {
        ib_log_error_tx(tx, "pcre operator received NULL field.");
        return IB_EINVAL;
    }

    if (field->type == IB_FTYPE_NULSTR) {
        ib_rc = ib_field_value(field, ib_ftype_nulstr_out(&subject));
        if (ib_rc != IB_OK) {
            return ib_rc;
        }

        if (subject != NULL) {
            subject_len = strlen(subject);
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_rc = ib_field_value(field, ib_ftype_bytestr_out(&bytestr));
        if (ib_rc != IB_OK) {
            return ib_rc;
        }

        if (bytestr != NULL) {
            subject_len = ib_bytestr_length(bytestr);
            subject = (const char *) ib_bytestr_const_ptr(bytestr);
        }
    }
    else {
        return IB_EINVAL;
    }

    if (subject == NULL) {
        subject = "";
    }

    ib_rc = get_or_create_operator_data(
        operator_data->cpdata->module,
        tx,
        &tx_data
    );
    if (ib_rc != IB_OK) {
        return ib_rc;
    }

    matches = pcre_exec_internal(
        operator_data->cpdata,
        tx_data->stack,
        subject,
        subject_len,
        0, /* Starting offset. */
        0, /* Options. */
        tx_data->ovector,
        tx_data->ovector_sz
    );

    if (matches > 0) {
        if (capture != NULL) {
            pcre_set_matches(tx, capture, tx_data->ovector, matches, subject);
        }
        ib_rc = IB_OK;
        *result = 1;
    }
    else if (matches == PCRE_ERROR_NOMATCH) {
        ib_rc = IB_OK;
        *result = 0;
    }
    else {
        ib_log_error_tx(
            tx,
            "Failure matching against: %s",
            pcre_error_str(matches));

        /* Some other error occurred. Set the status to false return the
         * error. */
        ib_rc = IB_EUNKNOWN;
        *result = 0;
    }

    return ib_rc;
}

/**
 * Set the ID of a DFA rule.
 *
 * @param[in] mm Memory manager to use for allocations.
 * @param[in,out] operator_data DFA rule object to store ID into.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on memory failure.
 */
static
ib_status_t dfa_id_set(
    ib_mm_t                  mm,
    modpcre_operator_data_t *operator_data
)
{
    assert(operator_data != NULL);

    /* We compute the length of the string buffer as such:
     * +2 for the 0x prefix.
     * +1 for the \0 string terminations.
     * +16 for encoding 8 bytes (64 bits) as hex-pairs (2 chars / byte).
     */
    size_t id_sz = 16 + 2 + 1;
    char *id;
    id = ib_mm_alloc(mm, id_sz+1);
    if (id == NULL) {
        return IB_EALLOC;
    }

    snprintf(id, id_sz, "%p", operator_data);
    operator_data->id = id;

    return IB_OK;
}

/**
 * Create the PCRE operator.
 *
 * @param[in] ctx Current context.
 * @param[in] mm Memory manager.
 * @param[in] parameters Unparsed string with the parameters to
 *                       initialize the operator instance.
 * @param[out] instance_data Instance data.
 * @param[in] cbdata Callback data.
 *
 * @returns IB_OK on success or IB_EALLOC on any other type of error.
 */
static
ib_status_t dfa_operator_create(
    ib_context_t *ctx,
    ib_mm_t       mm,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx           != NULL);
    assert(parameters    != NULL);
    assert(instance_data != NULL);

    ib_engine_t *ib   = ib_context_get_engine(ctx);
    assert(ib != NULL);

    modpcre_cpat_data_t     *cpdata;
    modpcre_operator_data_t *operator_data;
    ib_module_t             *module;
    modpcre_cfg_t           *config;
    ib_status_t              rc;
    const char              *errptr;
    int                      erroffset;

    /* Get my module object */
    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error getting pcre module object: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Get the context configuration */
    rc = ib_context_module_config(ctx, module, &config);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error getting pcre module configuration: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    rc = pcre_compile_internal(module,
                               ib,
                               mm,
                               config,
                               true,
                               &cpdata,
                               parameters,
                               &errptr,
                               &erroffset);

    if (rc != IB_OK) {
        ib_log_error(ib, "Error parsing DFA operator pattern \"%s\":%s",
                     parameters, ib_status_to_string(rc));
        return rc;
    }

    /* Allocate a rule data object, populate it */
    operator_data = ib_mm_alloc(mm, sizeof(*operator_data));
    if (operator_data == NULL) {
        return IB_EALLOC;
    }
    operator_data->cpdata = cpdata;
    rc = dfa_id_set(mm, operator_data);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error creating ID for DFA: %s",
                     ib_status_to_string(rc));
        return rc;
    }
    ib_log_debug3(ib, "Compiled DFA id=\"%s\" operator pattern \"%s\"",
                  operator_data->id, parameters);

    *(modpcre_operator_data_t **)instance_data = operator_data;
    return IB_OK;
}

/**
 * Create the per-transaction data for use with the dfa operator.
 *
 * @param[in] data Previously created per-tx data.
 * @param[in,out] tx Transaction to store the value in.
 * @param[in] cpatt_data Compiled pattern data
 * @param[in] id The operator identifier used to get it's workspace.
 * @param[out] workspace Created.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on an allocation error.
 */
static
ib_status_t alloc_dfa_tx_data(
    pcre_tx_data_t             *data,
    ib_tx_t                    *tx,
    const modpcre_cpat_data_t  *cpatt_data,
    const char                 *id,
    dfa_workspace_t           **workspace
)
{
    assert(data != NULL);
    assert(tx != NULL);
    assert(id != NULL);
    assert(workspace != NULL);

    ib_hash_t       *hash;
    ib_status_t      rc;
    dfa_workspace_t *ws;
    size_t           size;

    *workspace = NULL;

    hash = data->dfa_workspace_hash;

    ws = (dfa_workspace_t *)ib_mm_alloc(tx->mm, sizeof(*ws));
    if (ws == NULL) {
        return IB_EALLOC;
    }

    ws->partial    = NULL;
    ws->partial_sz = 0;
    ws->options    = 0;
    ws->wscount    = cpatt_data->dfa_ws_size;
    size           = sizeof(*(ws->workspace)) * (ws->wscount);
    ws->workspace  = (int *)ib_mm_alloc(tx->mm, size);
    if (ws->workspace == NULL) {
        return IB_EALLOC;
    }

    rc = ib_hash_set(hash, id, ws);
    if (rc != IB_OK) {
        return rc;
    }

    *workspace = ws;
    return IB_OK;
}

/**
 * Return the per-transaction data for use with the dfa operator.
 *
 * @param[in] m PCRE module.
 * @param[in] tx Transaction to store the value in.
 * @param[in] id The operator identifier used to get it's workspace.
 * @param[out] workspace Retrieved.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_ENOENT if the structure does not exist. Call alloc_dfa_tx_data then.
 *   - IB_EALLOC on an allocation error.
 */
static
ib_status_t get_dfa_tx_data(
    pcre_tx_data_t     *tx_data,
    const char         *id,
    dfa_workspace_t   **workspace
)
{
    assert(tx_data != NULL);
    assert(id != NULL);
    assert(workspace != NULL);

    ib_status_t     rc;

    rc = ib_hash_get(tx_data->dfa_workspace_hash, workspace, id);
    if (rc != IB_OK) {
        *workspace = NULL;
    }

    return rc;
}

/**
 * @brief Common code for executing the dfa operator.
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] module The module structure.
 * @param[in] is_phase If true, not streaming.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t dfa_operator_execute_common(
    ib_tx_t           *tx,
    void              *instance_data,
    const ib_field_t  *field,
    ib_field_t        *capture,
    ib_num_t          *result,
    const ib_module_t *module,
    bool              is_phase
)
{
    assert(tx            != NULL);
    assert(instance_data != NULL);
    assert(module        != NULL);

    /* The return code for pcre_dfa_exec().
     * If this is negative, it is a status code.
     * If this is > 0 it is the number of matches found + 1.
     * If this is == 0, there are not matches captured, but
     * the pattern matched.
     */
    int                      matches;
    ib_status_t              ib_rc;
    const char              *subject;
    size_t                   subject_len;
    size_t                   start_offset;
    int                      match_count;
    const ib_bytestr_t      *bytestr;
    pcre_tx_data_t          *tx_data;
    dfa_workspace_t         *dfa_workspace;
    modpcre_operator_data_t *operator_data =
        (modpcre_operator_data_t *)instance_data;
    const char              *id = operator_data->id;

    assert(module != NULL);
    assert(operator_data->cpdata->is_dfa == true);

    if (field == NULL) {
        ib_log_error_tx(tx, "dfa operator received NULL field.");
        return IB_EINVAL;
    }

    ib_rc = get_or_create_operator_data(module, tx, &tx_data);
    if (ib_rc != IB_OK) {
        return ib_rc;
    }

    /* Extract the subject from the field. */
    if (field->type == IB_FTYPE_NULSTR) {
        ib_rc = ib_field_value(field, ib_ftype_nulstr_out(&subject));
        if (ib_rc != IB_OK) {
            return ib_rc;
        }

        subject_len = strlen(subject);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_rc = ib_field_value(field, ib_ftype_bytestr_out(&bytestr));
        if (ib_rc != IB_OK) {
            return ib_rc;;
        }

        subject_len = ib_bytestr_length(bytestr);
        subject     = (const char *) ib_bytestr_const_ptr(bytestr);
    }
    else {
        ib_log_error_tx(tx, "dfa operator can only operate on string types.");
        return IB_EINVAL;
    }

    if (subject == NULL) {
        ib_log_debug_tx(tx, "Subject is empty for DFA op. No match.");
        *result = 0;
        return IB_OK;
    }

    /* Get the per-tx-per-operator workspace data for this rule data id. */
    ib_rc = get_dfa_tx_data(tx_data, id, &dfa_workspace);
    if (is_phase && ib_rc == IB_OK) {
        /* Phase rules always clear the restart flag on subsequent runs.
         * NOTE: Phase rules do not need to have pcre_dfa_clear_partial()
         *       called.
         */
        dfa_workspace->options &= (~PCRE_DFA_RESTART);
    }
    else if (ib_rc == IB_ENOENT) {
        /* First time we are called, clear the captures. */
        if (capture != NULL) {
            ib_rc = ib_capture_clear(capture);
            if (ib_rc != IB_OK) {
                ib_log_error_tx(
                    tx,
                    "Failed to clear captures: %s",
                    ib_status_to_string(ib_rc));
            }
        }

        ib_rc = alloc_dfa_tx_data(
            tx_data,
            tx,
            operator_data->cpdata,
            id,
            &dfa_workspace);
        if (ib_rc != IB_OK) {
            return ib_rc;
        }

        dfa_workspace->options = PCRE_PARTIAL_SOFT;

    }
    else if (ib_rc != IB_OK) {
        /* Not ok, not ENOENT, then fail. */
        return ib_rc;
    }

    /* Used in situations of multiple matches.
     * Specifies where in the subject pcre_dfa_exec() should start matching. */
    start_offset = 0;

    /* The total count of matches found. */
    match_count = 0;

    /* Perform the match.
     * If capturing is not requested, then there is no need to find
     * multiple matches, and this do-while will not iterate more than once.
     * If capturing is requested, and a match was found, this loop will
     * iterate more than once until no more matches are found. */
    do {
        matches = pcre_dfa_exec(
            operator_data->cpdata->cpatt,
            operator_data->cpdata->edata,
            subject,
            subject_len,
            start_offset, /* Starting offset. */
            dfa_workspace->options,
            tx_data->ovector,
            tx_data->ovector_sz,
            dfa_workspace->workspace,
            dfa_workspace->wscount);

        /* Check that we have matches. */
        if (matches >= 0) {

            /* Log if the match is zero length. */
            if (tx_data->ovector[0] == tx_data->ovector[1]) {
                pcre_log_debug(
                    tx,
                    "Match of zero length",
                    operator_data,
                    field
                );
            }

            /* If matches == 0, there were too many matches to report
             * all of them. Handle what we can match and continue.
             */
            if (matches == 0) {
                pcre_log_debug(
                    tx,
                    "DFA match overflow.",
                    operator_data,
                    field
                );
                ib_log_debug_tx(
                    tx,
                    "Only handling the longest %d matches.",
                    MATCH_MAX
                );
                matches = MATCH_MAX;
            }

            match_count += matches;

            /* Catch bugs that cause infinite matching. */
            if (match_count > 1000) {
                    pcre_log_error(
                        tx,
                        "DFA match overflow",
                        operator_data,
                        field
                    );
                    return IB_OK;
            }

            /* If we are to capture the values, it means 2 things:
             *
             * 1. We will iterate at the end of this do-while loop,
             *    and so must update start_offset using the longest
             *    matching substring.
             * 2. We must record the captured values. */
            if (capture) {

                start_offset = tx_data->ovector[1];

                ib_rc = pcre_dfa_set_match(
                    tx,
                    capture,
                    tx_data->ovector,
                    matches,
                    subject,
                    operator_data,
                    dfa_workspace);
                if (ib_rc != IB_OK) {
                    pcre_log_error(
                        tx,
                        "Failed to set dfa match",
                        operator_data,
                        field
                    );

                    /* Return OK. Do not cause rules to stop processing. */
                    return IB_OK;
                }
            }
            /* After a match clear the restart partial flag. */
            dfa_workspace->options &= (~PCRE_DFA_RESTART);
        }
        else if (matches == PCRE_ERROR_PARTIAL && ! is_phase) {
            /* Start recording into operator_data the buffer. */
            ib_rc = pcre_dfa_record_partial(
                tx,
                tx_data->ovector,
                subject,
                dfa_workspace);
            if (ib_rc != IB_OK) {
                pcre_log_error(
                    tx,
                    "Failed to set record partial dfa match",
                    operator_data,
                    field
                );

                /* Return OK. Do not cause rules to stop processing. */
                return IB_OK;
            }
            /* Set the restart partial flag. */
            dfa_workspace->options |= PCRE_DFA_RESTART;
        }
    } while (capture && (matches >= 0) && start_offset < subject_len);

    if (match_count > 0) {
        *result = 1;
    }
    else if (matches == PCRE_ERROR_NOMATCH) {
        *result = 0;
    }
    else if (matches == PCRE_ERROR_PARTIAL) {
        *result = 0;
    }
    else {
        /* Some other error occurred. Set the status to false but
         * do not return an error because we do not want to stop rule
         * processing. */
        *result = 0;
        pcre_log_error(
            tx,
            "Unexpected return code from DFA",
            operator_data,
            field
        );
        ib_log_error_tx(
            tx,
            "Unexpected return code from DFA match: %d",
            matches
        );
    }

    return IB_OK;
}

/**
 * @brief Execute the dfa stream operator.
 *
 * @param[in] tx Current transaction.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] cbdata Callback data. An @ref ib_module_t.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t dfa_phase_operator_execute(
    ib_tx_t          *tx,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *instance_data,
    void             *cbdata
)
{
    assert(tx            != NULL);
    assert(instance_data != NULL);
    assert(cbdata        != NULL);

    return dfa_operator_execute_common(
        tx,
        instance_data,
        field,
        capture,
        result,
        (const ib_module_t *)cbdata,
        true
    );
}

/**
 * @brief Execute the dfa stream operator.
 *
 * @param[in] tx Current transaction.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] cbdata Callback data. An @ref ib_module_t.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t dfa_stream_operator_execute(
    ib_tx_t          *tx,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *instance_data,
    void             *cbdata
)
{
    assert(tx            != NULL);
    assert(instance_data != NULL);
    assert(cbdata        != NULL);

    return dfa_operator_execute_common(
        tx,
        instance_data,
        field,
        capture,
        result,
        (const ib_module_t *)cbdata,
        false
    );
}

/* -- Module Routines -- */

static IB_CFGMAP_INIT_STRUCTURE(config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".study",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        study
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".use_jit",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        use_jit
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        match_limit
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit_recursion",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        match_limit_recursion
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".jit_stack_start",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        jit_stack_start
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".jit_stack_max",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        jit_stack_max
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".dfa_workspace_size",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        dfa_workspace_size
    ),
    IB_CFGMAP_INIT_LAST
};

/**
 * Handle on/off directives.
 *
 * @param[in] cp Config parser
 * @param[in] name Directive name
 * @param[in] onoff on/off flag
 * @param[in] cbdata Callback data (ignored)
 *
 * @returns Status code
 */
static ib_status_t handle_directive_onoff(ib_cfgparser_t *cp,
                                          const char *name,
                                          int onoff,
                                          void *cbdata)
{
    assert(cp != NULL);
    assert(name != NULL);
    assert(cp->ib != NULL);

    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    ib_module_t *module = NULL;
    modpcre_cfg_t *config = NULL;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    const char *pname;

    /* Get my module object */
    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error getting %s module object: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    /* Get my module configuration */
    rc = ib_context_module_config(ctx, module, (void *)&config);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error getting %s module configuration: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    if (strcasecmp("PcreStudy", name) == 0) {
        pname = MODULE_NAME_STR ".study";
    }
    else if (strcasecmp("PcreUseJit", name) == 0) {
        pname = MODULE_NAME_STR ".use_jit";
    }
    else {
        ib_cfg_log_error(cp, "Unhandled directive \"%s\"", name);
        return IB_EINVAL;
    }
    rc = ib_context_set_num(ctx, pname, onoff);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error setting \"%s\" to %s for \"%s\": %s",
                         pname, onoff ? "true" : "false", name,
                         ib_status_to_string(rc));
    }
    return IB_OK;
}

/**
 * Handle single parameter directives.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t handle_directive_param(ib_cfgparser_t *cp,
                                          const char *name,
                                          const char *p1,
                                          void *cbdata)
{
    assert(cp != NULL);
    assert(name != NULL);
    assert(p1 != NULL);
    assert(cp->ib != NULL);

    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    ib_module_t *module = NULL;
    modpcre_cfg_t *config = NULL;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    const char *pname;
    ib_num_t value;

    /* Get my module object */
    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error getting %s module object: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    /* Get my module configuration */
    rc = ib_context_module_config(ctx, module, (void *)&config);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error getting %s module configuration: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    /* p1 should be a number */
    rc = ib_type_atoi(p1, 0, &value);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error converting \"%s\" to a number for \"%s\": %s",
                         p1, name, ib_status_to_string(rc));
        return rc;
    }

    if (strcasecmp("PcreMatchLimit", name) == 0) {
        pname = "pcre.match_limit";
    }
    else if (strcasecmp("PcreMatchLimitRecursion", name) == 0) {
        pname = "pcre.match_limit_recursion";
    }
    else if (strcasecmp("PcreJitStackStart", name) == 0) {
        pname = "pcre.jit_stack_start";
    }
    else if (strcasecmp("PcreJitStackMax", name) == 0) {
        pname = "pcre.jit_stack_max";
    }
    else if (strcasecmp("PcreDfaWorkspaceSize", name) == 0) {
        pname = "pcre.dfa_workspace_size";
    }
    else {
        ib_cfg_log_error(cp, "Unhandled directive \"%s\"", name);
        return IB_EINVAL;
    }
    rc = ib_context_set_num(ctx, pname, value);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error setting \"%s\" to %ld for \"%s\": %s",
                         pname, (long int)value, name, ib_status_to_string(rc));
    }
    return IB_OK;
}

/**! Constant used as cbdata for value filters. */
static const char *c_filter_rx_value = "FilterValue";
/**! Constant used as cbdata for name filters. */
static const char *c_filter_rx_name = "FilterName";

/**
 * Create function for all rx filters.
 *
 * @param[in] mm Memory manager.
 * @param[in] regex Regular expression.
 * @param[out] instance_data Will be set to compiled pattern data.
 * @param[in] cbdata Module.
 * @return
 * - IB_OK on success.
 * - Any return of pcre_compile_internal().
 **/
static
ib_status_t filter_rx_create(
    ib_mm_t     mm,
    const char *regex,
    void       *instance_data,
    void       *cbdata
)
{
    assert(regex != NULL);
    assert(instance_data != NULL);
    assert(cbdata != NULL);

    ib_module_t *m = (ib_module_t *)cbdata;
    ib_engine_t *ib = m->ib;
    modpcre_cpat_data_t *cpdata;
    const char *error;
    int error_offset;
    ib_status_t rc;

    rc = pcre_compile_internal(
        m,
        ib,
        mm,
        &modpcre_global_cfg,
        false,
        &cpdata,
        regex,
        &error,
        &error_offset
    );
    if (rc != IB_OK) {
        return rc;
    }

    *(modpcre_cpat_data_t **)instance_data = cpdata;

    return IB_OK;
}

/**
 * Execute function for all rx filters.
 *
 * @param[in] mm Memory manager.
 * @param[in] fin Input field; expected to be a list to filter.
 * @param[in] fout Output field; list of matching subfields.
 * @param[in] instance_data Compiled pattern data.
 * @param[in] cbdata @ref c_filter_rx_value or @ref c_filter_rx_name.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EOTHER on insanity error.
 * - IB_EINVAL on unexpected input type.
 * - IB_EUNKNOWN on unexpected pcre failure.
 **/
static
ib_status_t filter_rx_execute(
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *instance_data,
    void              *cbdata
)
{
    assert(fin != NULL);
    assert(fout != NULL);
    assert(instance_data != NULL);
    assert(cbdata != NULL);

    bool filter_value;
    const ib_list_t *collection;
    ib_list_t *result;
    ib_field_t *result_field;
    ib_status_t rc;
    pcre_jit_stack *stack;
    const ib_list_node_t *node;
    const modpcre_cpat_data_t *cpdata =
        (const modpcre_cpat_data_t *)instance_data;

    if ((const char *)cbdata == c_filter_rx_value) {
        filter_value = true;
    }
    else if ((const char *)cbdata == c_filter_rx_name) {
        filter_value = false;
    }
    else {
        return IB_EOTHER;
    }

    rc = ib_field_value_type(
        fin,
        ib_ftype_list_out(&collection),
        IB_FTYPE_LIST
    );
    if (rc != IB_OK) {
        return rc;
    }

#ifdef PCRE_HAVE_JIT
    if (cpdata->is_jit) {

        assert(cpdata->edata != NULL);

        stack = pcre_jit_stack_alloc(32*1024, 1000*1024);

        if (stack != NULL) {
            rc = ib_mm_register_cleanup(mm, pcre_jit_stack_cleanup, stack);
            if (rc != IB_OK) {
                pcre_jit_stack_free(stack);
                return rc;
            }
        }
    }
    else
#endif
    {
        stack = NULL;
    }

    rc = ib_list_create(&result, mm);
    if (rc != IB_OK) {
        return rc;
    }

    IB_LIST_LOOP_CONST(collection, node) {
        const ib_field_t *subfield = ib_list_node_data_const(node);
        const char *subject;
        size_t subject_len;
        int pcre_rc;

        if (filter_value) {
            const ib_bytestr_t *bs;
            rc = ib_field_value_type(
                subfield,
                ib_ftype_bytestr_out(&bs),
                IB_FTYPE_BYTESTR
            );
            if (rc == IB_EINVAL) {
                /* Not a bytestr. */
                continue;
            }
            else if (rc != IB_OK) {
                return rc;
            }

            subject = (const char *)ib_bytestr_const_ptr(bs);
            subject_len = ib_bytestr_length(bs);
        }
        else {
            subject = subfield->name;
            subject_len = subfield->nlen;
        }

        pcre_rc = pcre_exec_internal(
            cpdata,
            stack,
            subject, subject_len,
            0, 0,
            NULL, 0
        );

        if (pcre_rc == PCRE_ERROR_NOMATCH) {
            continue;
        }
        if (pcre_rc < 0) {
            return IB_EUNKNOWN;
        }

        rc = ib_list_push(result, (void *)subfield);
        if (rc != IB_OK) {
            return rc;
        }
    }

    rc = ib_field_create_no_copy(
        &result_field,
        mm,
        fin->name, fin->nlen,
        IB_FTYPE_LIST,
        ib_ftype_list_mutable_in(result)
    );
    if (rc != IB_OK) {
        return rc;
    }

    *fout = result_field;

    return IB_OK;
}

static IB_DIRMAP_INIT_STRUCTURE(directive_map) = {
    IB_DIRMAP_INIT_ONOFF(
        "PcreStudy",
        handle_directive_onoff,
        NULL
    ),
    IB_DIRMAP_INIT_ONOFF(
        "PcreUseJit",
        handle_directive_onoff,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "PcreMatchLimit",
        handle_directive_param,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "PcreMatchLimitRecursion",
        handle_directive_param,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "PcreJitStackStart",
        handle_directive_param,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "PcreJitStackMax",
        handle_directive_param,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "PcreDfaWorkspaceSize",
        handle_directive_param,
        NULL
    ),
    IB_DIRMAP_INIT_LAST
};

static ib_status_t modpcre_init(ib_engine_t *ib,
                                ib_module_t *m,
                                void        *cbdata)
{
    assert(ib != NULL);
    assert(m != NULL);

    ib_status_t rc;

    /* Register operators. */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "pcre",
        IB_OP_CAPABILITY_CAPTURE,
        pcre_operator_create, NULL,
        NULL, NULL,
        pcre_operator_execute, m
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* An alias of pcre. The same callbacks are registered. */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "rx",
        IB_OP_CAPABILITY_CAPTURE,
        pcre_operator_create, NULL,
        NULL, NULL,
        pcre_operator_execute, m
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register a pcre operator that uses pcre_dfa_exec to match streams. */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "dfa",
        IB_OP_CAPABILITY_CAPTURE,
        dfa_operator_create, NULL,
        NULL, NULL,
        dfa_phase_operator_execute, m
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_operator_stream_create_and_register(
        NULL,
        ib,
        "dfa",
        IB_OP_CAPABILITY_CAPTURE,
        dfa_operator_create, NULL,
        NULL, NULL,
        dfa_stream_operator_execute, m
    );
    if (rc != IB_OK) {
        return rc;
    }


    /* Regexp based selection. */
    rc = ib_transformation_create_and_register(
        NULL,
        ib,
        "filterValueRx",
        true,
        filter_rx_create, m,
        NULL, NULL,
        filter_rx_execute, (void *)c_filter_rx_value
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_transformation_create_and_register(
        NULL,
        ib,
        "filterNameRx",
        true,
        filter_rx_create, m,
        NULL, NULL,
        filter_rx_execute, (void *)c_filter_rx_name
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,            /**< Default metadata */
    MODULE_NAME_STR,                      /**< Module name */
    IB_MODULE_CONFIG(&modpcre_global_cfg),/**< Global config data */
    config_map,                           /**< Configuration field map */
    directive_map,                        /**< Config directive map */
    modpcre_init,                         /**< Initialize function */
    NULL,                                 /**< Callback data */
    NULL,                                 /**< Finish function */
    NULL,                                 /**< Callback data */
);
