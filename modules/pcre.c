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
 * @brief IronBee &mdash; HTP Module
 *
 * This module adds a PCRE based matcher named "pcre".
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/bytestr.h>
#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/provider.h>
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

/* Name that a hash of tx-specific data is stored under in @c tx->data. */
#define MODULE_DATA_STR    MODULE_NAME_STR "_DATA"

/* How many matches will PCRE find and populate. */
#define MATCH_MAX 10

/* PCRE can use an independent stack or the machine stack.
 * If PCRE_JIT_STACK is true (conditional on PCRE_HAVE_JIT being true)
 * then pcrejit will use an independent stack. If PCRE_JIT_STACK is not
 * defined then the machine stack will be used.  */
#ifdef PCRE_HAVE_JIT
#define PCRE_JIT_STACK
#define PCRE_JIT_MIN_STACK_SZ 32*1024
#define PCRE_JIT_MAX_STACK_SZ 512*1024
#endif

/* Port forward the pcre constant PCRE_PARTIAL as PCRE_PARTIAL_SOFT. */
#ifndef PCRE_PARTIAL_SOFT
#define PCRE_PARTIAL_SOFT PCRE_PARTIAL
#endif

/**
 * From pcreapi man page.
 */
#define WORKSPACE_SIZE_MIN 20

/**
 * Build a reasonable buffer size.
 */
#define WORKSPACE_SIZE_DEFAULT (WORKSPACE_SIZE_MIN * 10)

typedef struct modpcre_cfg_t modpcre_cfg_t;
typedef struct modpcre_cpatt_t modpcre_cpatt_t;
typedef struct modpcre_cpatt_t pcre_rule_data_t;

/* Define the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Module Configuration Structure.
 */
struct modpcre_cfg_t {
    ib_num_t       study;                 /**< Study compiled regexs */
    ib_num_t       match_limit;           /**< Match limit */
    ib_num_t       match_limit_recursion; /**< Match recursion depth limit */
};

/**
 * Internal representation of PCRE compiled patterns.
 */
struct modpcre_cpatt_t {
    pcre          *cpatt;                 /**< Compiled pattern */
    size_t        cpatt_sz;               /**< Size of cpatt. */
    pcre_extra    *edata;                 /**< PCRE Study data */
    size_t        study_data_sz;          /**< Size of edata->study_data. */
    const char    *patt;                  /**< Regex pattern text */
    int           is_jit;                 /**< Is this JIT compiled? */
};

/**
 * Internal representation of PCRE compiled patterns for DFA execution.
 */
struct dfa_rule_data_t {
    pcre          *cpatt;                 /**< Compiled pattern */
    size_t        cpatt_sz;               /**< Size of cpatt. */
    pcre_extra    *edata;                 /**< PCRE Study data */
    size_t        study_data_sz;          /**< Size of edata->study_data. */
    const char    *patt;                  /**< Regex pattern text */
    const char    *id;                    /**< An id. */
};
typedef struct dfa_rule_data_t dfa_rule_data_t;

/* Instantiate a module global configuration. */
static modpcre_cfg_t modpcre_global_cfg = {
    1,    /* study */
    5000, /* match_limit */
    5000  /* match_limit_recursion */
};

/**
 * Internal compilation of the dfa pattern.
 *
 * The major difference in this compilation from that of a normal pcre pattern
 * is that it does not use PCRE_JIT because it is intended for use
 * on streaming data. Streaming data is delivered in chunks
 * and partial matches are found. Doing partial matches and resumes
 * disable JIT optimizations and some a few other normal optimizations.
 *
 * @param[in] pool The memory pool to allocate memory out of.
 * @param[out] dfa_cpatt Struct containing the compilation.
 * @param[in] patt The uncompiled pattern to match.
 * @param[out] errptr Pointer to an error message describing the failure.
 * @param[out] erroffset The location of the failure, if this fails.
 * @returns IronBee status. IB_EINVAL if the pattern is invalid,
 *          IB_EALLOC if memory allocation fails or IB_OK.
 */
static ib_status_t dfa_compile_internal(ib_mpool_t *pool,
                                        dfa_rule_data_t **dfa_cpatt,
                                        const char *patt,
                                        const char **errptr,
                                        int *erroffset)
{
    IB_FTRACE_INIT();

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

    /* How edata is produced if we are not using JIT. */
    const int study_flags = 0;

    cpatt = pcre_compile(patt, compile_flags, errptr, erroffset, NULL);

    if (*errptr != NULL) {
        ib_util_log_error("PCRE compile error for \"%s\": %s at offset %d",
                          patt, *errptr, *erroffset);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    edata = pcre_study(cpatt, study_flags, errptr);

    if(*errptr != NULL)  {
        pcre_free(cpatt);
        ib_util_log_error("PCRE study failed: %s", *errptr);
    }

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
     * back to the output variable dfa_cpatt.
     */

    *dfa_cpatt = (dfa_rule_data_t *)ib_mpool_alloc(pool, sizeof(**dfa_cpatt));
    if (*dfa_cpatt == NULL) {
        pcre_free(cpatt);
        pcre_free(edata);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    (*dfa_cpatt)->cpatt_sz = cpatt_sz;
    (*dfa_cpatt)->study_data_sz = study_data_sz;

    /* Copy pattern. */
    (*dfa_cpatt)->patt  = ib_mpool_strdup(pool, patt);
    if ((*dfa_cpatt)->patt == NULL) {
        pcre_free(cpatt);
        pcre_free(edata);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy compiled pattern. */
    (*dfa_cpatt)->cpatt = ib_mpool_memdup(pool, cpatt, cpatt_sz);
    pcre_free(cpatt);
    if ((*dfa_cpatt)->cpatt == NULL) {
        pcre_free(edata);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy extra data (study data). */
    if (edata != NULL) {

        /* Copy edata. */
        (*dfa_cpatt)->edata = ib_mpool_memdup(pool, edata, sizeof(*edata));

        if ((*dfa_cpatt)->edata == NULL) {
            pcre_free(edata);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Copy edata->study_data. */
        (*dfa_cpatt)->edata->study_data = ib_mpool_memdup(pool,
                                                           edata->study_data,
                                                           study_data_sz);
        pcre_free(edata);
        if ((*dfa_cpatt)->edata->study_data == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    else {
        (*dfa_cpatt)->edata = NULL;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Internal compilation of the modpcre pattern.
 *
 * @param[in] ib IronBee engine for logging.
 * @param[in] pool The memory pool to allocate memory out of.
 * @param[out] pcre_cpatt Struct containing the compilation.
 * @param[in] patt The uncompiled pattern to match.
 * @param[out] errptr Pointer to an error message describing the failure.
 * @param[out] erroffset The location of the failure, if this fails.
 *
 * @returns IronBee status. IB_EINVAL if the pattern is invalid,
 *          IB_EALLOC if memory allocation fails or IB_OK.
 */
static ib_status_t pcre_compile_internal(ib_engine_t *ib,
                                         ib_mpool_t *pool,
                                         modpcre_cpatt_t **pcre_cpatt,
                                         const char *patt,
                                         const char **errptr,
                                         int *erroffset)
{
    IB_FTRACE_INIT();

    /* Compiled pattern. */
    pcre *cpatt = NULL;

    /* Compiled pattern size. Used to copy cpatt. */
    size_t cpatt_sz;

    /* Extra data structure. This contains the study_data pointer. */
    pcre_extra *edata = NULL;

    /* Size of edata->study_data. */
    size_t study_data_sz;

    /* Is the compiled regex jit-compiled? This impacts how it is executed. */
    int is_jit;

    /* How cpatt is produced. */
    const int compile_flags = PCRE_DOTALL | PCRE_DOLLAR_ENDONLY;
#ifdef PCRE_HAVE_JIT
    /* Determine the success of a call. */
    int rc;

    /* Determines if the pcre compilation was successful with pcre_jit. */
    int pcre_jit_ret;

    /* How edata is produced if we are using JIT. */
    const int study_flags = PCRE_STUDY_JIT_COMPILE;
#else

    /* How edata is produced if we are not using JIT. */
    const int study_flags = 0;
#endif /* PCRE_HAVE_JIT */

    cpatt = pcre_compile(patt, compile_flags, errptr, erroffset, NULL);

    if (*errptr != NULL) {
        ib_util_log_error("PCRE compile error for \"%s\": %s at offset %d",
                          patt, *errptr, *erroffset);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    edata = pcre_study(cpatt, study_flags, errptr);

#ifdef PCRE_HAVE_JIT
    if(*errptr != NULL)  {
        pcre_free(cpatt);
        ib_util_log_error("PCRE-JIT study failed: %s", *errptr);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* The check to see if JIT compilation was a success changed in 8.20RC1
       now uses pcre_fullinfo see doc/pcrejit.3 */
    rc = pcre_fullinfo(cpatt, edata, PCRE_INFO_JIT, &pcre_jit_ret);
    if (rc != 0) {
        ib_log_error(ib, "PCRE-JIT failed to get pcre_fullinfo");
        is_jit = 0;
    }
    else if (pcre_jit_ret != 1) {
        ib_log_info(ib, "PCRE-JIT compiler does not support: %s", patt);
        ib_log_info(ib, "It will fallback to the normal PCRE");
        is_jit = 0;
    }
    else { /* Assume pcre_jit_ret == 1. */
        is_jit = 1;
    }
#else
    if(*errptr != NULL)  {
        pcre_free(cpatt);
        ib_log_info(ib, "PCRE study failed: %s", *errptr);
    }
    is_jit = 0;
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
     * back to the output variable pcre_cpatt.
     */

    *pcre_cpatt = (modpcre_cpatt_t *)ib_mpool_alloc(pool, sizeof(**pcre_cpatt));
    if (*pcre_cpatt == NULL) {
        pcre_free(cpatt);
        pcre_free(edata);
        ib_log_error(ib,
                     "Failed to allocate pcre_cpatt of size: %zd",
                     sizeof(**pcre_cpatt));
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    (*pcre_cpatt)->is_jit = is_jit;
    (*pcre_cpatt)->cpatt_sz = cpatt_sz;
    (*pcre_cpatt)->study_data_sz = study_data_sz;

    /* Copy pattern. */
    (*pcre_cpatt)->patt  = ib_mpool_strdup(pool, patt);
    if ((*pcre_cpatt)->patt == NULL) {
        pcre_free(cpatt);
        pcre_free(edata);
        ib_log_error(ib, "Failed to duplicate pattern string: %s", patt);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy compiled pattern. */
    (*pcre_cpatt)->cpatt = ib_mpool_memdup(pool, cpatt, cpatt_sz);
    pcre_free(cpatt);
    if ((*pcre_cpatt)->cpatt == NULL) {
        pcre_free(edata);
        ib_log_error(ib, "Failed to duplicate pattern of size: %zd", cpatt_sz);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy extra data (study data). */
    if (edata != NULL) {

        /* Copy edata. */
        (*pcre_cpatt)->edata = ib_mpool_memdup(pool, edata, sizeof(*edata));

        if ((*pcre_cpatt)->edata == NULL) {
            pcre_free(edata);
            ib_log_error(ib, "Failed to duplicate edata.");
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Copy edata->study_data. */
        (*pcre_cpatt)->edata->study_data = ib_mpool_memdup(pool,
                                                           edata->study_data,
                                                           study_data_sz);
        pcre_free(edata);
        if ((*pcre_cpatt)->edata->study_data == NULL) {
            ib_log_error(ib, "Failed to study data of size: %zd", study_data_sz);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    else {
        (*pcre_cpatt)->edata = NULL;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Matcher Interface -- */

/**
 * @param[in] mpr Provider object. This is unused.
 * @param[in] pool The memory pool to allocate memory out of.
 * @param[out] pcpatt When the pattern is successfully compiled
 *             a modpcre_cpatt_t* is stored in *pcpatt.
 * @param[in] patt The uncompiled pattern to match.
 * @param[out] errptr Pointer to an error message describing the failure.
 * @param[out] erroffset The location of the failure, if this fails.
 *
 * @returns IronBee status. IB_EINVAL if the pattern is invalid,
 *          IB_EALLOC if memory allocation fails or IB_OK.
 */
static ib_status_t modpcre_compile(ib_provider_t *mpr,
                                   ib_mpool_t *pool,
                                   void *pcpatt,
                                   const char *patt,
                                   const char **errptr,
                                   int *erroffset)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    rc = pcre_compile_internal(mpr->ib,
                               pool,
                               (modpcre_cpatt_t **)pcpatt,
                               patt,
                               errptr,
                               erroffset);

    IB_FTRACE_RET_STATUS(rc);

}

/**
 * Provider instance match using pcre_exec.
 *
 * @param[in] mpr Provider instance.
 * @param[in] cpatt Callback data of type modpcre_cpatt_t *.
 * @param[in] flags Flags used in rule compilation.
 * @param[in] data The subject to be checked.
 * @param[in] dlen The length of @a data.
 * @param[out] ctx User data for creation.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on memory allocation failures.
 *   - IB_ENOENT if a match was not found.
 *   - IB_EINVAL if an unexpected error is returned by @c pcre_exec.
 */
static ib_status_t modpcre_match_compiled(ib_provider_t *mpr,
                                          void *cpatt,
                                          ib_flags_t flags,
                                          const uint8_t *data,
                                          size_t dlen,
                                          void *ctx)
{
    IB_FTRACE_INIT();
    modpcre_cpatt_t *pcre_cpatt = (modpcre_cpatt_t *)cpatt;
    int ec;
    const int ovector_sz = 30;
    int *ovector = (int *) malloc(ovector_sz*sizeof(*ovector));

    if (ovector == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ec = pcre_exec(pcre_cpatt->cpatt, pcre_cpatt->edata,
                   (const char *)data, dlen,
                   0, 0, ovector, ovector_sz);

    free(ovector);

    if (ec >= 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (ec == PCRE_ERROR_NOMATCH) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

static ib_status_t modpcre_add_pattern(ib_provider_inst_t *pi,
                                       void *cpatt)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static ib_status_t modpcre_add_pattern_ex(ib_provider_inst_t *mpi,
                                          void *patterns,
                                          const char *patt,
                                          ib_void_fn_t callback,
                                          void *arg,
                                          const char **errptr,
                                          int *erroffset)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static ib_status_t modpcre_match(ib_provider_inst_t *mpi,
                                 ib_flags_t flags,
                                 const uint8_t *data,
                                 size_t dlen, void *ctx)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

static IB_PROVIDER_IFACE_TYPE(matcher) modpcre_matcher_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,

    /* Provider Interface */
    modpcre_compile,
    modpcre_match_compiled,

    /* Provider Instance Interface */
    modpcre_add_pattern,
    modpcre_add_pattern_ex,
    modpcre_match
};

/**
 * @brief Create the PCRE operator.
 * @param[in] ib The IronBee engine (unused)
 * @param[in] ctx The current IronBee context (unused)
 * @param[in] rule Parent rule to the operator
 * @param[in,out] pool The memory pool into which @c op_inst->data
 *                will be allocated.
 * @param[in] pattern The regular expression to be built.
 * @param[out] op_inst The operator instance that will be populated by
 *             parsing @a pattern.
 *
 * @returns IB_OK on success or IB_EALLOC on any other type of error.
 */
static ib_status_t pcre_operator_create(ib_engine_t *ib,
                                        ib_context_t *ctx,
                                        const ib_rule_t *rule,
                                        ib_mpool_t *pool,
                                        const char *pattern,
                                        ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    const char* errptr;
    int erroffset;
    pcre_rule_data_t *rule_data = NULL;
    ib_status_t rc;

    if (pattern == NULL) {
        ib_log_error(ib, "No pattern for %s operator", op_inst->op->name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    rc = pcre_compile_internal(ib,
                               pool,
                               &rule_data,
                               pattern,
                               &errptr,
                               &erroffset);

    if (rc==IB_OK) {
        op_inst->data = rule_data;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Deinitialize the rule.
 * @param[in,out] op_inst The instance of the operator to be deallocated.
 *                Operator data is allocated out of the memory pool for
 *                IronBee so we do not destroy the operator here.
 *                pool for IronBee and need not be freed by us.
 * @returns IB_OK always.
 */
static ib_status_t pcre_operator_destroy(ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    /* Nop */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Set the matches into the given field name as .0, .1, .2 ... .9.
 *
 * @param[in] ib The IronBee engine to log to.
 * @param[in] tx The transaction to store the values into (tx->dpi).
 * @param[in] ovector The vector of integer pairs of matches from PCRE.
 * @param[in] matches The number of matches.
 * @param[in] subject The matched-against string data.
 *
 * @returns IB_OK or IB_EALLOC.
 */
static ib_status_t pcre_set_matches(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    int *ovector,
                                    int matches,
                                    const char *subject)
{
    IB_FTRACE_INIT();

    /* IronBee status. */
    ib_status_t rc;

    /* Iterator. */
    int i;

    rc = ib_data_capture_clear(tx);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error clearing captures: %s",
                        ib_status_to_string(rc));
    }

    /* We have a match! Now populate TX:0-9 in tx->dpi. */
    ib_log_debug2_tx(tx, "REGEX populating %d matches", matches);
    for (i=0; i<matches; i++)
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

        /* If debugging this, copy the string value out and print it to the
         * log. This could be dangerous as there could be non-character
         * values in the match. */
        ib_log_debug2_tx(tx, "REGEX Setting #%d=%.*s",
                         i, (int)match_len, match_start);

        /* Create a byte-string representation */
        rc = ib_bytestr_dup_mem(&bs,
                                tx->mp,
                                (const uint8_t*)match_start,
                                match_len);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Create a field to hold the byte-string */
        name = ib_data_capture_name(i);
        rc = ib_field_create(&field, tx->mp, name, strlen(name),
                             IB_FTYPE_BYTESTR, ib_ftype_bytestr_in(bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add it to the capture collection */
        rc = ib_data_capture_set_item(tx, i, field);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Execute the rule.
 *
 * @param[in] ib Ironbee engine
 * @param[in] tx The transaction.
 * @param[in] rule The parent rule
 * @param[in,out] data User data. A @c pcre_rule_data_t.
 * @param[in] flags Operator instance flags
 * @param[in] field The field content.
 * @param[out] result The result.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t pcre_operator_execute(ib_engine_t *ib,
                                         ib_tx_t *tx,
                                         const ib_rule_t *rule,
                                         void *data,
                                         ib_flags_t flags,
                                         ib_field_t *field,
                                         ib_num_t *result)
{
    IB_FTRACE_INIT();

    assert(ib!=NULL);
    assert(tx!=NULL);
    assert(tx->dpi!=NULL);
    assert(data!=NULL);

    int matches;
    ib_status_t ib_rc;
    const int ovecsize = 3 * MATCH_MAX;
    int *ovector = (int *)malloc(ovecsize*sizeof(*ovector));
    const char* subject = NULL;
    size_t subject_len = 0;
    const ib_bytestr_t* bytestr;
    pcre_rule_data_t *rule_data = (pcre_rule_data_t *)data;
    pcre_extra *edata = NULL;
#ifdef PCRE_JIT_STACK
    pcre_jit_stack *jit_stack = pcre_jit_stack_alloc(PCRE_JIT_MIN_STACK_SZ,
                                                     PCRE_JIT_MAX_STACK_SZ);
#endif

    if (ovector==NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    if (field->type == IB_FTYPE_NULSTR) {
        ib_rc = ib_field_value(field, ib_ftype_nulstr_out(&subject));
        if (ib_rc != IB_OK) {
            free(ovector);
            IB_FTRACE_RET_STATUS(ib_rc);
        }

        if (subject != NULL) {
            subject_len = strlen(subject);
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_rc = ib_field_value(field, ib_ftype_bytestr_out(&bytestr));
        if (ib_rc != IB_OK) {
            free(ovector);
            IB_FTRACE_RET_STATUS(ib_rc);
        }

        if (bytestr != NULL) {
            subject_len = ib_bytestr_length(bytestr);
            subject = (const char *) ib_bytestr_const_ptr(bytestr);
        }
    }
    else {
        free(ovector);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (subject == NULL) {
        subject     = "";
    }

    /* Debug block. Escapes a string and prints it to the log.
     * Memory is freed. */
    if (ib_log_get_level(ib) >= 9) {

        /* Worst case, we can have a string that is 4x larger.
         * Consider if a string of 0xF7 is passed.  That single character
         * will expand to a string of 4 printed characters +1 for the \0
         * character. */
        char *debug_str = ib_util_hex_escape(subject, subject_len);

        if ( debug_str != NULL ) {
            ib_log_debug3_tx(tx, "Matching against: %s", debug_str);
            free( debug_str );
        }
    }

#ifdef PCRE_JIT_STACK
    /* Log if we expected jit, but did not get it. */
    if (rule_data->is_jit && jit_stack == NULL) {
        ib_log_debug(ib,
                     "Failed to allocate a jit stack for a jit-compiled rule. "
                     "Not using jit for this call.");
        edata = NULL;
    }

    /* If the study data is NULL or size zero, don't use it. */
    else if (rule_data->edata == NULL || rule_data->study_data_sz <= 0) {
        edata = NULL;
    }

    /* Only if we get here do we use the study data (edata) in the rule_data. */
    else {
        edata = rule_data->edata;
        pcre_assign_jit_stack(rule_data->edata, NULL, jit_stack);
    }

#endif

    matches = pcre_exec(rule_data->cpatt,
                        edata,
                        subject,
                        subject_len,
                        0, /* Starting offset. */
                        0, /* Options. */
                        ovector,
                        ovecsize);

#ifdef PCRE_JIT_STACK
    if (jit_stack != NULL) {
        pcre_jit_stack_free(jit_stack);
    }
#endif

    if (matches > 0) {
        if (ib_flags_all(rule->flags, IB_RULE_FLAG_CAPTURE)) {
            pcre_set_matches(ib, tx, ovector, matches, subject);
        }
        ib_rc = IB_OK;
        *result = 1;
    }
    else if (matches == PCRE_ERROR_NOMATCH) {

        if (ib_log_get_level(ib) >= 7) {
            char* tmp_c = malloc(subject_len+1);
            memcpy(tmp_c, subject, subject_len);
            tmp_c[subject_len] = '\0';
            /* No match. Return false to the caller (*result = 0). */
            ib_log_debug2_tx(tx, "No match for [%s] using pattern [%s].",
                        tmp_c,
                        rule_data->patt);
            free(tmp_c);
        }


        ib_rc = IB_OK;
        *result = 0;
    }
    else {
        /* Some other error occurred. Set the status to false and
        report the error. */
        ib_rc = IB_EUNKNOWN;
        *result = 0;
    }

    free(ovector);
    IB_FTRACE_RET_STATUS(ib_rc);
}

/**
 * @brief Create the PCRE operator.
 * @param[in] ib The IronBee engine (unused)
 * @param[in] ctx The current IronBee context (unused)
 * @param[in] rule Parent rule to the operator
 * @param[in,out] pool The memory pool into which @c op_inst->data
 *                will be allocated.
 * @param[in] pattern The regular expression to be built.
 * @param[out] op_inst The operator instance that will be populated by
 *             parsing @a pattern.
 *
 * @returns IB_OK on success or IB_EALLOC on any other type of error.
 */
static ib_status_t dfa_operator_create(ib_engine_t *ib,
                                        ib_context_t *ctx,
                                        const ib_rule_t *rule,
                                        ib_mpool_t *pool,
                                        const char *pattern,
                                        ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    const char* errptr;
    int erroffset;
    dfa_rule_data_t *rule_data = NULL;
    ib_status_t rc;

    rc = dfa_compile_internal(pool,
                              &rule_data,
                              pattern,
                              &errptr,
                              &erroffset);

    if (rc==IB_OK) {
        ib_log_debug(ib, "Compiled DFA operator pattern: %s", pattern);

        /* We compute the length of the string buffer as such:
         * +2 for the 0x prefix.
         * +1 for the \0 string terminations.
         * +16 for encoding 8 bytes (64 bits) as hex-pairs (2 chars / byte).
         */
        size_t id_sz = 16 + 2 + 1;
        char *id;
        id = ib_mpool_alloc(pool, id_sz);

        snprintf(id, id_sz, "%p", op_inst);
        rule_data->id = id;
        ib_log_debug(ib, "Created DFA operator with ID %s.", id);
        op_inst->data = rule_data;
    }
    else {
        ib_log_error(ib, "Failed to parse DFA operator pattern: %s", pattern);
    }


    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Get or create an ib_hash_t inside of @c tx->data for storing dfa rule data.
 *
 * The hash is stored at the key @c MODULE_DATA_STR.
 *
 * @param[in] tx The transaction containing @c tx->data which holds
 *            the @a rule_data object.
 * @param[out] rule_data The fetched or created rule data hash. This is set
 *             to NULL on failure.
 *
 * @return
 *   - IB_OK on success.
 *   - IB_EALLOC on allocation failure
 */
static ib_status_t get_or_create_rule_data_hash(ib_tx_t *tx,
                                                ib_hash_t **rule_data)
{
    IB_FTRACE_INIT();

    assert(tx);
    assert(tx->mp);

    ib_status_t rc;

    /* Get or create the hash that contains the rule data. */
    rc = ib_hash_get(tx->data, rule_data, MODULE_DATA_STR);

    if (rc == IB_OK && *rule_data != NULL) {
        ib_log_debug2_tx(tx,
                         "Found rule data hash in tx data named "
                         MODULE_DATA_STR);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug2_tx(tx, "Rule data hash did not exist in tx data.");
    ib_log_debug2_tx(tx, "Creating rule data hash " MODULE_DATA_STR);

    rc = ib_hash_create(rule_data, tx->mp);
    if (rc != IB_OK) {
        ib_log_debug2_tx(tx,
                         "Failed to create hash " MODULE_DATA_STR ": %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_hash_set(tx->data, MODULE_DATA_STR, *rule_data);
    if (rc != IB_OK) {
        ib_log_debug2_tx(tx,
                         "Failed to store hash " MODULE_DATA_STR ": %d", rc);
        *rule_data = NULL;
    }

    ib_log_debug2_tx(tx,
                     "Returning rule hash " MODULE_DATA_STR " at %p.",
                     *rule_data);

    IB_FTRACE_RET_STATUS(rc);

}

struct dfa_workspace_t {
    int *workspace;
    int wscount;
};
typedef struct dfa_workspace_t dfa_workspace_t;

/**
 * Create the per-transaction data for use with the dfa operator.
 *
 * @param[in,out] tx Transaction to store the value in.
 * @param[in] id The operator identifier used to get it's workspace.
 * @param[out] workspace Created.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on an allocation error.
 */
static ib_status_t alloc_dfa_tx_data(ib_tx_t *tx,
                                     const char *id,
                                     dfa_workspace_t **workspace)
{
    IB_FTRACE_INIT();

    assert(tx);
    assert(tx->mp);
    assert(id);
    assert(workspace);

    ib_hash_t *rule_data;
    ib_status_t rc;

    rc = get_or_create_rule_data_hash(tx, &rule_data);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    *workspace = (dfa_workspace_t *)ib_mpool_alloc(tx->mp, sizeof(**workspace));
    if (*workspace == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    (*workspace)->wscount = WORKSPACE_SIZE_DEFAULT;
    (*workspace)->workspace =
        (int *)ib_mpool_alloc(tx->mp,
                             sizeof(*((*workspace)->workspace)) *
                                (*workspace)->wscount);
    if ((*workspace)->workspace == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_hash_set(rule_data, id, *workspace);
    if (rc != IB_OK) {
        *workspace = NULL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Return the per-transaction data for use with the dfa operator.
 *
 * @param[in,out] tx Transaction to store the value in.
 * @param[in] id The operator identifier used to get it's workspace.
 * @param[out] workspace Retrieved.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_ENOENT if the structure does not exist. Call alloc_dfa_tx_data then.
 *   - IB_EALLOC on an allocation error.
 */
static ib_status_t get_dfa_tx_data(ib_tx_t *tx,
                                   const char *id,
                                   dfa_workspace_t **workspace)
{
    IB_FTRACE_INIT();

    assert(tx);
    assert(tx->mp);
    assert(id);
    assert(workspace);

    ib_hash_t *rule_data;
    ib_status_t rc;

    rc = get_or_create_rule_data_hash(tx, &rule_data);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_hash_get(rule_data, workspace, id);
    if (rc != IB_OK) {
        *workspace = NULL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Execute the rule.
 *
 * @param[in] ib Ironbee engine
 * @param[in] tx The transaction.
 * @param[in] rule The parent rule
 * @param[in,out] data User data. A @c pcre_rule_data_t.
 * @param[in] flags Operator instance flags
 * @param[in] field The field content.
 * @param[out] result The result.
 *
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t dfa_operator_execute(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        const ib_rule_t *rule,
                                        void *data,
                                        ib_flags_t flags,
                                        ib_field_t *field,
                                        ib_num_t *result)
{
    IB_FTRACE_INIT();
    assert(tx);
    assert(data);


    int matches;
    ib_status_t ib_rc;
    const int ovecsize = 3 * MATCH_MAX;
    dfa_rule_data_t *rule_data;
    int *ovector;
    const char* subject;
    size_t subject_len;
    const ib_bytestr_t* bytestr;
    dfa_workspace_t *dfa_workspace;
    int options; /* dfa exec options. */

    ovector = (int *)malloc(ovecsize*sizeof(*ovector));
    if (ovector==NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Pull out the rule data. */
    rule_data = (dfa_rule_data_t *)data;


    if (field->type == IB_FTYPE_NULSTR) {
        ib_rc = ib_field_value(field, ib_ftype_nulstr_out(&subject));
        if (ib_rc != IB_OK) {
            free(ovector);
            IB_FTRACE_RET_STATUS(ib_rc);
        }

        subject_len = strlen(subject);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_rc = ib_field_value(field, ib_ftype_bytestr_out(&bytestr));
        if (ib_rc != IB_OK) {
            free(ovector);
            IB_FTRACE_RET_STATUS(ib_rc);
        }

        subject_len = ib_bytestr_length(bytestr);
        subject = (const char *) ib_bytestr_const_ptr(bytestr);
    }
    else {
        free(ovector);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Debug block. Escapes a string and prints it to the log.
     * Memory is freed. */
    if (ib_log_get_level(ib) >= 9) {

        /* Worst case, we can have a string that is 4x larger.
         * Consider if a string of 0xF7 is passed.  That single character
         * will expand to a string of 4 printed characters +1 for the \0
         * character. */
        char *debug_str = ib_util_hex_escape(subject, subject_len);

        if ( debug_str != NULL ) {
            ib_log_debug3_tx(tx, "Matching against: %s", debug_str);
            free( debug_str );
        }
    }

    /* Get the per-tx workspace data for this rule data id. */
    ib_rc = get_dfa_tx_data(tx, rule_data->id, &dfa_workspace);
    if (ib_rc == IB_ENOENT) {
        options = PCRE_PARTIAL_SOFT;

        ib_rc = alloc_dfa_tx_data(tx, rule_data->id, &dfa_workspace);
        if (ib_rc != IB_OK) {
            free(ovector);
            ib_log_error_tx(tx, "Unexpected error creating tx storage "
                                "for dfa operator %s",
                                rule_data->id);
            IB_FTRACE_RET_STATUS(ib_rc);
        }

        ib_log_debug_tx(tx,
                       "Created DFA workspace at %p for id %s.",
                        dfa_workspace,
                        rule_data->id);
    }
    else if (ib_rc == IB_OK) {
        options = PCRE_PARTIAL_SOFT | PCRE_DFA_RESTART;
        ib_log_debug_tx(tx,
                        "Reusing existing DFA workspace %p for id %s.",
                        dfa_workspace,
                        rule_data->id);
    }
    else {
        free(ovector);
        ib_log_error_tx(tx,
                        "Unexpected error fetching dfa data "
                        "for dfa operator %s",
                        rule_data->id);
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Actually do the DFA match. */
    matches = pcre_dfa_exec(rule_data->cpatt,
                            rule_data->edata,
                            subject,
                            subject_len,
                            0, /* Starting offset. */
                            options,
                            ovector,
                            ovecsize,
                            dfa_workspace->workspace,
                            dfa_workspace->wscount);

    if (matches >= 0) {
        ib_rc = IB_OK;
        *result = 1;
    }
    else if (matches == PCRE_ERROR_PARTIAL) {
        ib_log_debug2_tx(tx, "Partial match found, but not a full match.");
        ib_rc = IB_OK;
        *result = 0;
    }
    else if (matches == PCRE_ERROR_NOMATCH) {

        if (ib_log_get_level(ib) >= 7) {
            char* tmp_c = malloc(subject_len+1);
            memcpy(tmp_c, subject, subject_len);
            tmp_c[subject_len] = '\0';
            /* No match. Return false to the caller (*result = 0). */
            ib_log_debug2_tx(tx, "No match for [%s] using pattern [%s].",
                        tmp_c,
                        rule_data->patt);
            free(tmp_c);
        }

        ib_rc = IB_OK;
        *result = 0;
    }
    else {
        /* Some other error occurred. Set the status to false and
        report the error. */
        ib_rc = IB_EUNKNOWN;
        *result = 0;
    }

    free(ovector);
    IB_FTRACE_RET_STATUS(ib_rc);
}

/**
 * @brief Destroy the dfa operator
 *
 * @param[in,out] op_inst The operator instance
 *
 * @returns IB_OK
 */
static ib_status_t dfa_operator_destroy(ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();

    /* Nop - Memory released by mpool. */

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* -- Module Routines -- */

static ib_status_t modpcre_init(ib_engine_t *ib,
                                ib_module_t *m,
                                void        *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Register as a matcher provider. */
    rc = ib_provider_register(ib,
                              IB_PROVIDER_TYPE_MATCHER,
                              MODULE_NAME_STR,
                              NULL,
                              &modpcre_matcher_iface,
                              NULL);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     MODULE_NAME_STR
                     ": Error registering pcre matcher provider: "
                     "%s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug(ib, "PCRE Status: compiled=\"%d.%d %s\" loaded=\"%s\"",
        PCRE_MAJOR, PCRE_MINOR, IB_XSTRINGIFY(PCRE_DATE), pcre_version());

    /* Register operators. */
    ib_operator_register(ib,
                         "pcre",
                         (IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE),
                         pcre_operator_create,
                         NULL,
                         pcre_operator_destroy,
                         NULL,
                         pcre_operator_execute,
                         NULL);

    /* An alias of pcre. The same callbacks are registered. */
    ib_operator_register(ib,
                         "rx",
                         (IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE),
                         pcre_operator_create,
                         NULL,
                         pcre_operator_destroy,
                         NULL,
                         pcre_operator_execute,
                         NULL);

    /* Register a pcre operator that uses pcre_dfa_exec to match streams. */
    ib_operator_register(ib,
                         "dfa",
                         (IB_OP_FLAG_PHASE | IB_OP_FLAG_STREAM),
                         dfa_operator_create,
                         NULL,
                         dfa_operator_destroy,
                         NULL,
                         dfa_operator_execute,
                         NULL);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_CFGMAP_INIT_STRUCTURE(modpcre_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".study",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        study
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
    IB_CFGMAP_INIT_LAST
};

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,            /**< Default metadata */
    MODULE_NAME_STR,                      /**< Module name */
    IB_MODULE_CONFIG(&modpcre_global_cfg),/**< Global config data */
    modpcre_config_map,                   /**< Configuration field map */
    NULL,                                 /**< Config directive map */
    modpcre_init,                         /**< Initialize function */
    NULL,                                 /**< Callback data */
    NULL,                                 /**< Finish function */
    NULL,                                 /**< Callback data */
    NULL,                                 /**< Context open function */
    NULL,                                 /**< Callback data */
    NULL,                                 /**< Context close function */
    NULL,                                 /**< Callback data */
    NULL,                                 /**< Context destroy function */
    NULL                                  /**< Callback data */
);

