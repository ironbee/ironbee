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
 * @brief IronBee --- HTP Module
 *
 * This module adds a PCRE based matcher named "pcre".
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
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/util.h>

#include <pcre.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        pcre
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* How many matches will PCRE find and populate. */
#define MATCH_MAX 10

/* PCRE can use an independent stack or the machine stack.
 * If PCRE_JIT_STACK is true (conditional on PCRE_HAVE_JIT being true)
 * then pcrejit will use an independent stack. If PCRE_JIT_STACK is not
 * defined then the machine stack will be used.  */
#ifdef PCRE_HAVE_JIT
#define PCRE_JIT_STACK
const int PCRE_JIT_STACK_START_MULT = 32;
const int PCRE_JIT_STACK_MAX_MULT   = 512;
#endif

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
typedef struct modpcre_cfg_t {
    ib_num_t       study;                 /**< Bool: Study compiled regexs */
    ib_num_t       use_jit;               /**< Bool: Use JIT if available */
    ib_num_t       match_limit;           /**< Match limit */
    ib_num_t       match_limit_recursion; /**< Match recursion depth limit */
    ib_num_t       jit_stack_start;       /**< Starting JIT stack size */
    ib_num_t       jit_stack_max;         /**< Max JIT stack size */
    ib_num_t       dfa_workspace_size;    /**< Size of DFA workspace */
} modpcre_cfg_t;

/**
 * Internal representation of PCRE compiled patterns.
 */
typedef struct modpcre_cpat_data_t {
    pcre                *cpatt;           /**< Compiled pattern */
    size_t               cpatt_sz;        /**< Size of cpatt. */
    pcre_extra          *edata;           /**< PCRE Study data */
    size_t               study_data_sz;   /**< Size of edata->study_data. */
    const char          *patt;            /**< Regex pattern text */
    bool                 is_dfa;          /**< Is this a DFA? */
    bool                 is_jit;          /**< Is this JIT compiled? */
    int                  jit_stack_start; /**< Starting JIT stack size */
    int                  jit_stack_max;   /**< Max JIT stack size */
    int                  dfa_ws_size;     /**< Size of DFA workspace */
} modpcre_cpat_data_t;

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
    1,                      /* study */
    1,                      /* use_jit */
    5000,                   /* match_limit */
    5000,                   /* match_limit_recursion */
    0,                      /* jit_stack_start; 0 means auto */
    0,                      /* jit_stack_max; 0 means auto */
    WORKSPACE_SIZE_DEFAULT  /* dfa_workspace_size */
};

/**
 * Internal compilation of the modpcre pattern.
 *
 * @param[in] ib IronBee engine for logging.
 * @param[in] pool The memory pool to allocate memory out of.
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
static ib_status_t pcre_compile_internal(ib_engine_t *ib,
                                         ib_mpool_t *pool,
                                         const modpcre_cfg_t *config,
                                         bool is_dfa,
                                         modpcre_cpat_data_t **pcpdata,
                                         const char *patt,
                                         const char **errptr,
                                         int *erroffset)
{
    assert(ib != NULL);
    assert(pool != NULL);
    assert(config != NULL);
    assert(pcpdata != NULL);
    assert(patt != NULL);

    /* Pattern data structure we'll create */
    modpcre_cpat_data_t *cpdata;

    /* Compiled pattern. */
    pcre *cpatt = NULL;

    /* Compiled pattern size. Used to copy cpatt. */
    size_t cpatt_sz;

    /* Extra data structure. This contains the study_data pointer. */
    pcre_extra *edata = NULL;

    /* Size of edata->study_data. */
    size_t study_data_sz;

    /* How cpatt is produced. */
    const int compile_flags = PCRE_DOTALL | PCRE_DOLLAR_ENDONLY;

    /* Are we using JIT? */
    bool use_jit = !is_dfa;

#ifdef PCRE_HAVE_JIT
    if (config->use_jit == 0) {
        use_jit = false;
    }

    /* Do we want to be using JIT? */
    const bool want_jit = use_jit;
#else
    use_jit = false;
#endif /* PCRE_HAVE_JIT */

    cpatt = pcre_compile(patt, compile_flags, errptr, erroffset, NULL);

    if (*errptr != NULL) {
        ib_log_error(ib, "Error compiling PCRE pattern \"%s\": %s at offset %d",
                     patt, *errptr, *erroffset);
        return IB_EINVAL;
    }

    if (config->study) {
        if (use_jit) {
#ifdef PCRE_HAVE_JIT
            edata = pcre_study(cpatt, PCRE_STUDY_JIT_COMPILE, errptr);
            if (*errptr != NULL)  {
                pcre_free(cpatt);
                use_jit = false;
                ib_log_warning(ib, "PCRE-JIT study failed: %s", *errptr);
            }
#endif
        }
        else {
            edata = pcre_study(cpatt, 0, errptr);
            if (*errptr != NULL)  {
                pcre_free(cpatt);
                ib_log_error(ib, "PCRE study failed: %s", *errptr);
            }
        }
    }
    else if (use_jit) {
        ib_log_warning(ib, "PCRE: Disabling JIT because study disabled");
        use_jit = false;
    }

#ifdef PCRE_HAVE_JIT
    /* The check to see if JIT compilation was a success changed in 8.20RC1
       now uses pcre_fullinfo see doc/pcrejit.3 */
    if (use_jit) {
        int rc;
        int pcre_jit_ret;

        rc = pcre_fullinfo(cpatt, edata, PCRE_INFO_JIT, &pcre_jit_ret);
        if (rc != 0) {
            ib_log_error(ib, "PCRE-JIT failed to get pcre_fullinfo");
            use_jit = false;
        }
        else if (pcre_jit_ret != 1) {
            ib_log_info(ib, "PCRE-JIT compiler does not support: %s", patt);
            use_jit = false;
        }
        else { /* Assume pcre_jit_ret == 1. */
            /* Do nothing */
        }
    }
    if (want_jit && !use_jit) {
        ib_log_info(ib, "Falling back to normal PCRE");
    }
#endif /*PCRE_HAVE_JIT*/

    /* Compute the size of the populated values of cpatt. */
    pcre_fullinfo(cpatt, edata, PCRE_INFO_SIZE, &cpatt_sz);
    if (edata != NULL) {
        pcre_fullinfo(cpatt, edata, PCRE_INFO_STUDYSIZE, &study_data_sz);
    }
    else {
        study_data_sz = 0;
    }

    /**
     * Below is only allocation and copy operations to pass the PCRE results
     * back to the output variable cpdata.
     */

    cpdata = (modpcre_cpat_data_t *)ib_mpool_calloc(pool, sizeof(*cpdata), 1);
    if (cpdata == NULL) {
        pcre_free(cpatt);
        pcre_free(edata);
        ib_log_error(ib,
                     "Failed to allocate cpdata of size: %zd",
                     sizeof(*cpdata));
        return IB_EALLOC;
    }

    cpdata->is_dfa = is_dfa;
    cpdata->is_jit = use_jit;
    cpdata->cpatt_sz = cpatt_sz;
    cpdata->study_data_sz = study_data_sz;

    /* Copy pattern. */
    cpdata->patt = ib_mpool_strdup(pool, patt);
    if (cpdata->patt == NULL) {
        pcre_free(cpatt);
        pcre_free(edata);
        ib_log_error(ib, "Failed to duplicate pattern string: %s", patt);
        return IB_EALLOC;
    }

    /* Copy compiled pattern. */
    cpdata->cpatt = ib_mpool_memdup(pool, cpatt, cpatt_sz);
    pcre_free(cpatt);
    if (cpdata->cpatt == NULL) {
        pcre_free(edata);
        ib_log_error(ib, "Failed to duplicate pattern of size: %zd", cpatt_sz);
        return IB_EALLOC;
    }

    /* Copy extra data (study data). */
    if (edata != NULL) {

        /* Copy edata. */
        cpdata->edata = ib_mpool_memdup(pool, edata, sizeof(*edata));
        if (cpdata->edata == NULL) {
            pcre_free(edata);
            ib_log_error(ib, "Failed to duplicate edata.");
            return IB_EALLOC;
        }

        /* Copy edata->study_data. */
        if (edata->study_data != NULL) {
            cpdata->edata->study_data =
                ib_mpool_memdup(pool, edata->study_data, study_data_sz);

            if (cpdata->edata->study_data == NULL) {
                ib_log_error(ib, "Failed to study data of size: %zd",
                             study_data_sz);
                pcre_free(edata);
                return IB_EALLOC;
            }
        }
        pcre_free(edata);
    }
    else {
        cpdata->edata = ib_mpool_calloc(pool, 1, sizeof(*edata));
        if (cpdata->edata == NULL) {
            pcre_free(edata);
            return IB_EALLOC;
        }
    }

    /* Set the PCRE limits for non-DFA patterns */
    if (! is_dfa) {
        cpdata->edata->flags |=
            (PCRE_EXTRA_MATCH_LIMIT | PCRE_EXTRA_MATCH_LIMIT_RECURSION);
        cpdata->edata->match_limit =
            (unsigned long)config->match_limit;
        cpdata->edata->match_limit_recursion =
            (unsigned long)config->match_limit_recursion;
        cpdata->dfa_ws_size = 0;
    }
    else {
        cpdata->edata->match_limit = 0U;
        cpdata->edata->match_limit_recursion = 0U;
        cpdata->dfa_ws_size = (int)config->dfa_workspace_size;
    }

    /* Set stack limits for JIT */
    if (cpdata->is_jit) {
#ifdef PCRE_HAVE_JIT
        if (config->jit_stack_start == 0U) {
            cpdata->jit_stack_start =
                PCRE_JIT_STACK_START_MULT * config->match_limit_recursion;
        }
        else {
            cpdata->jit_stack_start = (int)config->jit_stack_start;
        }
        if (config->jit_stack_max == 0U) {
            cpdata->jit_stack_max =
                PCRE_JIT_STACK_MAX_MULT * config->match_limit_recursion;
        }
        else {
            cpdata->jit_stack_max = (int)config->jit_stack_max;
        }
#endif
    }
    else {
        cpdata->jit_stack_start = 0;
        cpdata->jit_stack_max = 0;
    }

    ib_log_trace(ib,
                 "Compiled PCRE pattern \"%s\": "
                 "limit=%ld rlimit=%ld "
                 "dfa=%s dfa-ws-sz=%d "
                 "jit=%s jit-stack: start=%d max=%d",
                 patt,
                 cpdata->edata->match_limit,
                 cpdata->edata->match_limit_recursion,
                 cpdata->is_dfa ? "yes" : "no",
                 cpdata->dfa_ws_size,
                 cpdata->is_jit ? "yes" : "no",
                 cpdata->jit_stack_start,
                 cpdata->jit_stack_max);
    *pcpdata = cpdata;

    return IB_OK;
}

/**
 * Create the PCRE operator.
 *
 * @param[in] ctx Current context.
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
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx           != NULL);
    assert(parameters    != NULL);
    assert(instance_data != NULL);

    ib_engine_t *ib = ib_context_get_engine(ctx);
    ib_mpool_t *pool = ib_context_get_mpool(ctx);
    assert(ib != NULL);
    assert(pool != NULL);

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
    rc = pcre_compile_internal(ib,
                               pool,
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
    operator_data = ib_mpool_alloc(pool, sizeof(*operator_data));
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
                                tx->mp,
                                (const uint8_t*)match_start,
                                match_len);
        if (rc != IB_OK) {
            return rc;
        }

        /* Create a field to hold the byte-string */
        name = ib_capture_name(i);
        rc = ib_field_create(&field, tx->mp, name, strlen(name),
                             IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bs));
        if (rc != IB_OK) {
            return rc;
        }

        /* Add it to the capture collection */
        rc = ib_capture_set_item(capture, i, tx->mp, field);
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
        ib_mpool_alloc(
            tx->mp,
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
        new_subject = ib_mpool_alloc(
            tx->mp,
            sizeof(*new_subject) *
            (ovector[1] - ovector[0] + dfa_workspace->partial_sz));
        if (new_subject == NULL) {
            return IB_EALLOC;
        }

        /* Allocate new_ovector. */
        new_ovector = ib_mpool_alloc(tx->mp, sizeof(*ovector) * matches * 2);
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
        dfa_workspace->partial_sz = 0;
        dfa_workspace->partial    = NULL;
    }
    /* A small optimization:
     * Instead of copying all byte strings from the subject,
     * we create a copy of subject that we own and alias into it.
     */
     else {
        subject = ib_mpool_memdup(
            tx->mp,
            subject,
            sizeof(*subject) * ovector[1]);
    }

    /* Readability. Mark the start and length of the string. */
    match_start = subject + ovector[0];
    match_len   = ovector[1] - ovector[0];

    /* Create a byte string copy representation */
    rc = ib_bytestr_alias_mem(
        &bs,
        tx->mp,
        (const uint8_t*)match_start,
        match_len);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create a field to hold the byte-string */
    name = ib_capture_name(0);
    rc = ib_field_create(
        &field,
        tx->mp,
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
 * @brief Execute the PCRE operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static
ib_status_t pcre_operator_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(instance_data != NULL);
    assert(tx            != NULL);

    int matches;
    ib_status_t ib_rc;
    const int ovector_sz = 3 * MATCH_MAX;
    int *ovector = (int *)malloc(ovector_sz*sizeof(*ovector));
    const char *subject = NULL;
    size_t subject_len = 0;
    const ib_bytestr_t *bytestr;
    modpcre_operator_data_t *operator_data =
        (modpcre_operator_data_t *)instance_data;
    pcre_extra *edata = NULL;
#ifdef PCRE_JIT_STACK
    pcre_jit_stack *jit_stack = NULL;
#endif

    assert(operator_data->cpdata->is_dfa == false);

    if (ovector==NULL) {
        return IB_EALLOC;
    }

    if (field->type == IB_FTYPE_NULSTR) {
        ib_rc = ib_field_value(field, ib_ftype_nulstr_out(&subject));
        if (ib_rc != IB_OK) {
            free(ovector);
            return ib_rc;
        }

        if (subject != NULL) {
            subject_len = strlen(subject);
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_rc = ib_field_value(field, ib_ftype_bytestr_out(&bytestr));
        if (ib_rc != IB_OK) {
            free(ovector);
            return ib_rc;
        }

        if (bytestr != NULL) {
            subject_len = ib_bytestr_length(bytestr);
            subject = (const char *) ib_bytestr_const_ptr(bytestr);
        }
    }
    else {
        free(ovector);
        return IB_EINVAL;
    }

    if (subject == NULL) {
        subject     = "";
    }

    if (operator_data->cpdata->is_jit) {
#ifdef PCRE_JIT_STACK
        jit_stack = pcre_jit_stack_alloc(operator_data->cpdata->jit_stack_start,
                                         operator_data->cpdata->jit_stack_max);
        if (jit_stack == NULL) {
            ib_log_warning(ib,
                "Failed to allocate a jit stack for a jit-compiled rule.  "
                "Not using jit for this call."
            );
        }
        /* If the study data is NULL or size zero, don't use it. */
        else if (operator_data->cpdata->study_data_sz > 0) {
            edata = operator_data->cpdata->edata;
        }
        if (edata != NULL) {
            pcre_assign_jit_stack(edata, NULL, jit_stack);
        }
#else
        edata = NULL;
#endif
    }
    else if (operator_data->cpdata->study_data_sz > 0) {
        edata = operator_data->cpdata->edata;
    }
    else {
        edata = NULL;
    }

    matches = pcre_exec(operator_data->cpdata->cpatt,
                        edata,
                        subject,
                        subject_len,
                        0, /* Starting offset. */
                        0, /* Options. */
                        ovector,
                        ovector_sz);

#ifdef PCRE_JIT_STACK
    if (jit_stack != NULL) {
        pcre_jit_stack_free(jit_stack);
    }
#endif

    if (matches > 0) {
        if (capture != NULL) {
            pcre_set_matches(tx, capture, ovector, matches, subject);
        }
        ib_rc = IB_OK;
        *result = 1;
    }
    else if (matches == PCRE_ERROR_NOMATCH) {
        ib_rc = IB_OK;
        *result = 0;
    }
    else {
        /* Some other error occurred. Set the status to false return the
         * error. */
        ib_rc = IB_EUNKNOWN;
        *result = 0;
    }

    free(ovector);
    return ib_rc;
}

/**
 * Set the ID of a DFA rule.
 *
 * @param[in] mp Memory pool to use for allocations.
 * @param[in,out] operator_data DFA rule object to store ID into.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on memory failure.
 */
static
ib_status_t dfa_id_set(
    ib_mpool_t              *mp,
    modpcre_operator_data_t *operator_data
)
{
    assert(mp            != NULL);
    assert(operator_data != NULL);

    /* We compute the length of the string buffer as such:
     * +2 for the 0x prefix.
     * +1 for the \0 string terminations.
     * +16 for encoding 8 bytes (64 bits) as hex-pairs (2 chars / byte).
     */
    size_t id_sz = 16 + 2 + 1;
    char *id;
    id = ib_mpool_alloc(mp, id_sz+1);
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
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx           != NULL);
    assert(parameters    != NULL);
    assert(instance_data != NULL);

    ib_engine_t *ib   = ib_context_get_engine(ctx);
    ib_mpool_t  *pool = ib_context_get_mpool(ctx);
    assert(ib != NULL);
    assert(pool != NULL);

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

    rc = pcre_compile_internal(ib,
                               pool,
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
    operator_data = ib_mpool_alloc(pool, sizeof(*operator_data));
    if (operator_data == NULL) {
        return IB_EALLOC;
    }
    operator_data->cpdata = cpdata;
    rc = dfa_id_set(pool, operator_data);
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
ib_status_t get_or_create_operator_data_hash(
    const ib_module_t  *m,
    ib_tx_t            *tx,
    ib_hash_t         **hash
)
{
    assert(tx);
    assert(tx->mp);

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
 * Create the per-transaction data for use with the dfa operator.
 *
 * @param[in] m PCRE module.
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
    const ib_module_t          *m,
    ib_tx_t                    *tx,
    const modpcre_cpat_data_t  *cpatt_data,
    const char                 *id,
    dfa_workspace_t           **workspace
)
{
    assert(tx != NULL);
    assert(tx->mp != NULL);
    assert(id != NULL);
    assert(workspace != NULL);

    ib_hash_t       *hash;
    ib_status_t      rc;
    dfa_workspace_t *ws;
    size_t           size;

    *workspace = NULL;
    rc = get_or_create_operator_data_hash(m, tx, &hash);
    if (rc != IB_OK) {
        return rc;
    }

    ws = (dfa_workspace_t *)ib_mpool_alloc(tx->mp, sizeof(*ws));
    if (ws == NULL) {
        return IB_EALLOC;
    }

    ws->partial    = NULL;
    ws->partial_sz = 0;
    ws->options    = 0;
    ws->wscount    = cpatt_data->dfa_ws_size;
    size           = sizeof(*(ws->workspace)) * (ws->wscount);
    ws->workspace  = (int *)ib_mpool_alloc(tx->mp, size);
    if (ws->workspace == NULL) {
        return IB_EALLOC;
    }

    rc = ib_hash_set(hash, id, ws);
    if (rc == IB_OK) {
        *workspace = ws;
    }

    return rc;
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
    const ib_module_t  *m,
    ib_tx_t            *tx,
    const char         *id,
    dfa_workspace_t   **workspace
)
{
    assert(tx != NULL);
    assert(tx->mp != NULL);
    assert(id != NULL);
    assert(workspace != NULL);

    ib_hash_t   *hash;
    ib_status_t  rc;

    rc = get_or_create_operator_data_hash(m, tx, &hash);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_hash_get(hash, workspace, id);
    if (rc != IB_OK) {
        *workspace = NULL;
    }

    return rc;
}

/**
 * @brief Execute the dfa stream operator.
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] module The module structure.
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

    static const int ovector_sz = 3 * MATCH_MAX;

    /* The return code for pcre_dfa_exec().
     * If this is negative, it is a status code.
     * If this is > 0 it is the number of matches found + 1.
     * If this is == 0, there are not matches captured, but
     * the pattern matched.
     */
    int                      matches;
    ib_status_t              ib_rc;
    int                     *ovector;
    const char              *subject;
    size_t                   subject_len;
    size_t                   start_offset;
    int                      match_count;
    const ib_bytestr_t      *bytestr;
    dfa_workspace_t         *dfa_workspace;
    modpcre_operator_data_t *operator_data =
        (modpcre_operator_data_t *)instance_data;
    const char              *id = operator_data->id;

    assert(module != NULL);
    assert(operator_data->cpdata->is_dfa == true);

    ovector = (int *)malloc(ovector_sz*sizeof(*ovector));
    if (ovector==NULL) {
        return IB_EALLOC;
    }

    /* Extract the subject from the field. */
    if (field->type == IB_FTYPE_NULSTR) {
        ib_rc = ib_field_value(field, ib_ftype_nulstr_out(&subject));
        if (ib_rc != IB_OK) {
            goto return_rc;
        }

        subject_len = strlen(subject);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_rc = ib_field_value(field, ib_ftype_bytestr_out(&bytestr));
        if (ib_rc != IB_OK) {
            goto return_rc;
        }

        subject_len = ib_bytestr_length(bytestr);
        subject     = (const char *) ib_bytestr_const_ptr(bytestr);
    }
    else {
        ib_rc = IB_EINVAL;
        goto return_rc;
    }

    if (subject_len == 0 || subject == NULL) {
        ib_log_debug_tx(tx, "Subject is empty for DFA op. No match.");
        *result = 0;
        ib_rc = IB_OK;
        goto return_rc;
    }

    /* Get the per-tx-per-operator workspace data for this rule data id. */
    ib_rc = get_dfa_tx_data(module, tx, id, &dfa_workspace);
    if (is_phase && ib_rc == IB_OK) {
        /* Phase rules always clear the restart flag on subsequent runs. */
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
            module,
            tx,
            operator_data->cpdata,
            id,
            &dfa_workspace);
        if (ib_rc != IB_OK) {
            goto return_rc;
        }

        dfa_workspace->options = PCRE_PARTIAL_SOFT;

    }
    else if (ib_rc != IB_OK) {
        /* Not ok, not NOENT, then fail. */
        goto return_rc;
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
            ovector,
            ovector_sz,
            dfa_workspace->workspace,
            dfa_workspace->wscount);

        /* Assume we want to restart. */
        dfa_workspace->options |= PCRE_DFA_RESTART;

        /* Check that we have matches. */
        if (matches >= 0) {

            /* If the match is zero in length, turn off restart and
             * do not capture. */
            if (ovector[0] == ovector[1]) {
                dfa_workspace->options &= (~PCRE_DFA_RESTART);
            }
            /* If the match is non-zero in length, process the match. */
            else {

                /* If matches == 0, there were too many matches to report
                 * all of them. Handle what we can match and continue.
                 */
                if (matches == 0) {
                    ib_log_debug_tx(
                        tx,
                        "DFA Match over flow. "
                        "Only handling the longest %d matches.",
                        MATCH_MAX);
                    matches = MATCH_MAX;
                }

                match_count += matches;

                /* If we are to capture the values, it means 2 things:
                 *
                 * 1. We will iterate at the end of this do-while loop,
                 *    and so must update start_offset using the longest
                 *    matching substring.
                 * 2. We must record the captured values. */
                if (capture) {

                    start_offset = ovector[1];

                    ib_rc = pcre_dfa_set_match(
                        tx,
                        capture,
                        ovector,
                        matches,
                        subject,
                        operator_data,
                        dfa_workspace);
                    if (ib_rc != IB_OK) {
                        goto return_rc;
                    }

                    /* Handle corner case where a match completes on a buffer
                     * boundary. This can stall this loop unless handled. */
                    if (ovector[1] == 0) {
                        dfa_workspace->options &= (~PCRE_DFA_RESTART);
                    }
                }
            }
        }
        else if (matches == PCRE_ERROR_PARTIAL) {
            /* Start recording into operator_data the buffer. */
            ib_rc = pcre_dfa_record_partial(
                tx,
                ovector,
                subject,
                dfa_workspace);
            if (ib_rc != IB_OK) {
                goto return_rc;
            }
        }
    } while (capture && (matches >= 0) && start_offset < subject_len);

    if (match_count > 0) {
        ib_rc = IB_OK;
        *result = 1;
    }
    else if (matches == PCRE_ERROR_NOMATCH) {
        ib_rc = IB_OK;
        *result = 0;
    }
    else if (matches == PCRE_ERROR_PARTIAL) {
        ib_rc = IB_OK;
        *result = 0;
    }
    else {
        ib_log_error_tx(
            tx,
            "Unexpected return code from DFA mach: %d",
            matches);
        /* Some other error occurred. Set the status to false and
         * return the error. */
        ib_rc = IB_EUNKNOWN;
        *result = 0;
    }

return_rc:
    free(ovector);
    return ib_rc;
}

/**
 * @brief Execute the dfa stream operator.
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data. An @ref ib_module_t.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t dfa_phase_operator_execute(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
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
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data. An @ref ib_module_t.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t dfa_stream_operator_execute(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
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
    rc = ib_string_to_num(p1, 0, &value);
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
