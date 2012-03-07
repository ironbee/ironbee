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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee - HTP Module
 *
 * This module adds a PCRE based matcher named "pcre".
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <assert.h>
#include <ctype.h>
#include <pcre.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <time.h>


#include <ironbee/bytestr.h>
#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/provider.h>
#include <ironbee/util.h>
#include <ironbee/field.h>


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
#define PCRE_JIT_MIN_STACK_SZ 32*1024
#define PCRE_JIT_MAX_STACK_SZ 512*1024
#endif

typedef struct modpcre_cfg_t modpcre_cfg_t;
typedef struct modpcre_cpatt_t modpcre_cpatt_t;
typedef struct modpcre_cpatt_t pcre_rule_data_t;

/* Define the public module symbol. */
IB_MODULE_DECLARE();

/**
 * @internal
 * Module Configuration Structure.
 */
struct modpcre_cfg_t {
    ib_num_t       study;                 /**< Study compiled regexs */
    ib_num_t       match_limit;           /**< Match limit */
    ib_num_t       match_limit_recursion; /**< Match recursion depth limit */
};

/**
 * @internal
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

/* Instantiate a module global configuration. */
static modpcre_cfg_t modpcre_global_cfg;

/**
 * Internal compilation of the modpcre pattern.
 *
 * @param[in] pool The memory pool to allocate memory out of.
 * @param[out] pcre_cpatt Struct containing the compilation.
 * @param[in] patt The uncompiled pattern to match.
 * @param[out] errptr Pointer to an error message describing the failure.
 * @param[out] errorffset The location of the failure, if this fails.
 * @returns IronBee status. IB_EINVAL if the pattern is invalid,
 *          IB_EALLOC if memory allocation fails or IB_OK.
 */
static ib_status_t modpcre_compile_internal(ib_mpool_t *pool,
                                            modpcre_cpatt_t **pcre_cpatt,
                                            const char *patt,
                                            const char **errptr,
                                            int *erroffset)
{
    IB_FTRACE_INIT();
    pcre *cpatt = NULL;
    size_t cpatt_sz;
    pcre_extra *edata = NULL;
    size_t study_data_sz;
    int is_jit;
    const int compile_flags = PCRE_DOTALL | PCRE_DOLLAR_ENDONLY;
#ifdef PCRE_HAVE_JIT
    int rc;
    int pcre_jit_ret;
    const int study_flags = PCRE_STUDY_JIT_COMPILE;
#else
    const int study_flags = 0;
#endif /* PCRE_HAVE_JIT */

    cpatt = pcre_compile(patt, compile_flags, errptr, erroffset, NULL);

    if (*errptr != NULL) {
        ib_util_log_error(4, "PCRE compile error for \"%s\": %s at offset %d",
                          patt, *errptr, *erroffset);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    edata = pcre_study(cpatt, study_flags, errptr);

#ifdef PCRE_HAVE_JIT
    if(*errptr != NULL)  {
        pcre_free(cpatt);
        ib_util_log_error(4,"PCRE-JIT study failed : %s", *errptr);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* The check to see if JIT compilation was a success changed in 8.20RC1
       now uses pcre_fullinfo see doc/pcrejit.3 */
    rc = pcre_fullinfo(cpatt, edata, PCRE_INFO_JIT, &pcre_jit_ret);
    if (rc != 0) {
        ib_util_log_error(4,"PCRE-JIT failed to get pcre_fullinfo");
        is_jit = 0;
    }
    else if (pcre_jit_ret != 1) {
        ib_util_log_error(4,
                          "PCRE-JIT compiler does not support: %s. "
                          "It will fallback to the normal PCRE",
                          patt);
        is_jit = 0;
    }
    else {
        is_jit = 1;
    }
#else
    if(*errptr != NULL)  {
        pcre_free(cpatt);
        ib_util_log_error(4,"PCRE study failed : %s", *errptr);
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
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy compiled pattern. */
    (*pcre_cpatt)->cpatt = ib_mpool_memdup(pool, cpatt, cpatt_sz);
    pcre_free(cpatt);
    if ((*pcre_cpatt)->cpatt == NULL) {
        pcre_free(edata);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy extra data (study data). */
    if (edata != NULL) {

        /* Copy edata. */
        (*pcre_cpatt)->edata = ib_mpool_memdup(pool, edata, sizeof(*edata));

        if ((*pcre_cpatt)->edata == NULL) {
            pcre_free(edata);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Copy edata->study_data. */
        (*pcre_cpatt)->edata->study_data = ib_mpool_memdup(pool,
                                                           edata->study_data,
                                                           study_data_sz);
        pcre_free(edata);
        if ((*pcre_cpatt)->edata->study_data == NULL) {
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
 * @param[out] errorffset The location of the failure, if this fails.
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

    rc = modpcre_compile_internal(pool,
                                  (modpcre_cpatt_t **)pcpatt,
                                  patt,
                                  errptr,
                                  erroffset);

    IB_FTRACE_RET_STATUS(rc);

}

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
 * @param[in,out] pool The memory pool into which @c op_inst->data
 *                will be allocated.
 * @param[in] The regular expression to be built.
 * @param[out] op_inst The operator instance that will be populated by
 *             parsing @a pattern.
 * @returns IB_OK on success or IB_EALLOC on any other type of error.
 */
static ib_status_t pcre_operator_create(ib_engine_t *ib,
                                        ib_context_t *ctx,
                                        ib_mpool_t *pool,
                                        const char *pattern,
                                        ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    const char* errptr;
    int erroffset;
    pcre_rule_data_t *rule_data = NULL;
    ib_status_t rc;

    rc = modpcre_compile_internal(pool,
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

//! Ensure that the given field name exists in tx->dpi.
static ib_status_t ensure_field_exists(ib_engine_t *ib,
                                       ib_tx_t *tx,
                                       const char *field_name)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    assert(ib!=NULL);
    assert(tx!=NULL);
    assert(tx->dpi!=NULL);
    assert(field_name!=NULL);

    ib_field_t *ib_field;

    rc = ib_data_get(tx->dpi, field_name, &ib_field);

    if (rc == IB_ENOENT) {
        ib_data_add_list(tx->dpi, field_name, NULL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Set the matches into the given field name as .0, .1, .2 ... .9.
 *
 * @param[in] ib The IronBee engine to log to.
 * @param[in] tx The transaction to store the values into (tx->dpi).
 * @param[in] field_name The field to populate with Regex matches.
 * @param[in] ovector The vector of integer pairs of matches from PCRE.
 * @param[in] matches The number of matches.
 * @param[in] subject The matched-against string data.
 *
 * @returns IB_OK or IB_EALLOC.
 */
static ib_status_t pcre_set_matches(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    const char* field_name,
                                    int *ovector,
                                    int matches,
                                    const char *subject)
{
    IB_FTRACE_INIT();

    /* IronBee status. */
    ib_status_t rc;

    /* Iterator. */
    int i;

    /* Length of field_name. */
    const int field_name_sz = strlen(field_name);

    /* The length of the match. */
    size_t match_len;

    /* The first character in the match. */
    const char* match_start;

    /* +3 = '.', [digit], and \0. */
    char *full_field_name = malloc(field_name_sz+3);

    /* Holder to build an optional debug message in. */
    char *debug_msg;

    /* Holder for a copy of the field value when creating a new field. */
    ib_bytestr_t *field_value;

    /* Field holder. */
    ib_field_t *ib_field;

    /* Ensure the above allocations happened. */
    if (full_field_name==NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ensure_field_exists(ib, tx, field_name);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Could not ensure that field %s was a list.",
            field_name);
        free(full_field_name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* We have a match! Now populate TX.0-9 in tx->dpi. */
    for (i=0; i<matches; i++)
    {
        /* Build the field name. Typically TX.0, TX.1 ... TX.9 */
        sprintf(full_field_name, "%s.%d", field_name, i);

        /* Readability. Mark the start and length of the string. */
        match_start = subject+ovector[i*2];
        match_len = ovector[i*2+1] - ovector[i*2];

        /* If debugging this, copy the string value out and print it to the
         * log. This could be dangerous as there could be non-character
         * values in the match. */
        if (ib_log_get_level(ib) >= 7) {
            debug_msg = malloc(match_len+1);

            /* Notice: Don't provoke a crash if malloc fails. */
            if (debug_msg != NULL) {
                memcpy(debug_msg, match_start, match_len);
                debug_msg[match_len] = '\0';

                ib_log_debug(ib, 7, "REGEX Setting %s=%s.",
                            full_field_name,
                            debug_msg);

                free(debug_msg);
            }
        }

        ib_data_get(tx->dpi, full_field_name, &ib_field);

        if (ib_field == NULL) {
            ib_data_add_bytestr(tx->dpi,
                                full_field_name,
                                (uint8_t*)subject+ovector[i*2],
                                match_len,
                                NULL);
        }
        else {
            ib_bytestr_dup_mem(&field_value,
                               tx->mp,
                               (const uint8_t*)match_start,
                               match_len);
            ib_field_setv(ib_field, &field_value);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Execute the rule.
 *
 * @param[in] ib Ironbee engine
 * @param[in] tx The transaction.
 * @param[in,out] User data. A @c pcre_rule_data_t.
 * @param[in] flags Operator instance flags
 * @param[in] field The field content.
 * @param[in] result The result.
 * @returns IB_OK most times. IB_EALLOC when a memory allocation error handles.
 */
static ib_status_t pcre_operator_execute(ib_engine_t *ib,
                                         ib_tx_t *tx,
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
    const char* subject;
    size_t subject_len;
    ib_bytestr_t* bytestr;
    pcre_rule_data_t *rule_data = (pcre_rule_data_t*)data;
    pcre *regex;
    pcre_extra *regex_extra = NULL;
#ifdef PCRE_JIT_STACK
    pcre_jit_stack *jit_stack = pcre_jit_stack_alloc(PCRE_JIT_MIN_STACK_SZ,
                                                     PCRE_JIT_MAX_STACK_SZ);
#endif

    if (ovector==NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    if (field->type == IB_FTYPE_NULSTR) {
        subject = ib_field_value_nulstr(field);
        subject_len = strlen(subject);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        bytestr = ib_field_value_bytestr(field);
        subject_len = ib_bytestr_length(bytestr);
        subject = (const char*) ib_bytestr_const_ptr(bytestr);
    }
    else {
        free(ovector);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Alloc space to copy regex. */
    regex = (pcre*)malloc(rule_data->cpatt_sz);

    if (regex == NULL ) {
        free(ovector);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    memcpy(regex, rule_data->cpatt, rule_data->cpatt_sz);

    if (rule_data->study_data_sz == 0 ) {
        regex_extra = NULL;
    }
    else {
        regex_extra = (pcre_extra*) malloc(sizeof(*regex_extra));

        if (regex_extra == NULL ) {
            free(ovector);
            free(regex);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        *regex_extra = *rule_data->edata;

        if ( rule_data->study_data_sz == 0 ) {
            regex_extra->study_data = NULL;
        } else {
            regex_extra->study_data = malloc(rule_data->study_data_sz);

            if (regex_extra->study_data == NULL ) {
                free(ovector);
                if (regex_extra != NULL) {
                    free(regex_extra);
                }
                free(regex);
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }

            memcpy(regex_extra->study_data,
                   rule_data->edata->study_data,
                   rule_data->study_data_sz);
        }

        /* Put some modest limits on our regex. */
        regex_extra->match_limit = 1000;
        regex_extra->match_limit_recursion = 1000;
        regex_extra->flags = regex_extra->flags |
                            PCRE_EXTRA_MATCH_LIMIT |
                            PCRE_EXTRA_MATCH_LIMIT_RECURSION;
    }

#ifdef PCRE_JIT_STACK
    if (jit_stack == NULL) {
        if ( regex_extra != NULL ) {
            if ( regex_extra->study_data != NULL ) {
                free(regex_extra->study_data);
            }

            free(regex_extra);
        }
        free(ovector);
        free(regex);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    pcre_assign_jit_stack(regex_extra, NULL, jit_stack);
#endif

    matches = pcre_exec(regex,
                        regex_extra,
                        subject,
                        subject_len,
                        0, /* Starting offset. */
                        0, /* Options. */
                        ovector,
                        ovecsize);

#ifdef PCRE_JIT_STACK
    pcre_jit_stack_free(jit_stack);
#endif

    if (matches > 0) {
        pcre_set_matches(ib, tx, "TX", ovector, matches, subject);
        ib_rc = IB_OK;
        *result = 1;
    }
    else if (matches == PCRE_ERROR_NOMATCH) {

        if (ib_log_get_level(ib) >= 7) {
            char* tmp_c = malloc(subject_len+1);
            memcpy(tmp_c, subject, subject_len);
            tmp_c[subject_len] = '\0';
            /* No match. Return false to the caller (*result = 0). */
            ib_log_debug(ib, 7, "No match for [%s] using pattern [%s].",
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

    if ( regex_extra != NULL ) {
        if ( regex_extra->study_data != NULL ) {
            free(regex_extra->study_data);
        }

        free(regex_extra);
    }
    free(ovector);
    free(regex);
    IB_FTRACE_RET_STATUS(ib_rc);
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
        ib_log_error(ib, 3,
                     MODULE_NAME_STR
                     ": Error registering pcre matcher provider: "
                     "%d", rc);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug(ib, 4,"PCRE Status: compiled=\"%d.%d %s\" loaded=\"%s\"",
        PCRE_MAJOR, PCRE_MINOR, IB_XSTRINGIFY(PCRE_DATE), pcre_version());

    /* Register operators. */
    ib_operator_register(ib, "pcre", 0, pcre_operator_create,
                                        pcre_operator_destroy,
                                        pcre_operator_execute);

    /* An alias of pcre. The same callbacks are registered. */
    ib_operator_register(ib, "rx", 0, pcre_operator_create,
                                      pcre_operator_destroy,
                                      pcre_operator_execute);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_CFGMAP_INIT_STRUCTURE(modpcre_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".study",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        study,
        1
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        match_limit,
        5000
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".match_limit_recursion",
        IB_FTYPE_NUM,
        modpcre_cfg_t,
        match_limit_recursion,
        5000
    ),
    IB_CFGMAP_INIT_LAST
};

/**
 * @internal
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

