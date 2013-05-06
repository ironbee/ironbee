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
 * @brief IronBee --- SQLi Module based on libinjection.
 *
 * This module utilizes libinjection to implement SQLi detection. The
 * libinjection library is the work of Nick Galbreath.
 *
 * http://www.client9.com/projects/libinjection/
 *
 * Transformations:
 *   - normalizeSqli: Normalize SQL routine from libinjection.
 *
 * Operators:
 *   - is_sqli: Returns true if the data contains SQL injection.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/rule_engine.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <modp_ascii.h>
#include <sqli_fingerprints.h>
#include <sqlparse.h>
#include <sqlparse_private.h>

#include <assert.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        sqli
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/* Private configuration structure. */
typedef struct sqli_config_t {
    int fold;
} sqli_config_t;

/* Normalization function prototype. */
typedef int (*sqli_tokenize_fn_t)(sfilter * sf, stoken_t * sout);

/*********************************
 * Transformations
 *********************************/

static
ib_status_t sqli_normalize_tfn(ib_engine_t *ib,
                               ib_mpool_t *mp,
                               void *tfn_data,
                               const ib_field_t *field_in,
                               ib_field_t **field_out,
                               ib_flags_t *pflags)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(field_in != NULL);
    assert(field_out != NULL);
    assert(pflags != NULL);

    sqli_config_t *cfg = (sqli_config_t *)tfn_data;
    sqli_tokenize_fn_t tokenize_fn;
    sfilter sf;
    stoken_t current;
    ib_bytestr_t *bs_in;
    ib_bytestr_t *bs_out;
    const char *buf_in;
    char *buf_in_start;
    size_t buf_in_len;
    char *buf_out;
    char *buf_out_end;
    size_t buf_out_len;
    size_t lead_len = 0;
    char prev_token_type;
    ib_status_t rc;

    /* Currently only bytestring types are supported.
     * Other types will just get passed through. */
    if (field_in->type != IB_FTYPE_BYTESTR) {
        return ib_field_copy(field_out, mp, field_in->name, field_in->nlen, field_in);
    }

    /* Extract the underlying incoming value. */
    rc = ib_field_value(field_in, ib_ftype_bytestr_mutable_out(&bs_in));
    if (rc != IB_OK) {
        return rc;
    }

    /* Create a buffer big enough (double) to allow for normalization. */
    buf_in = (const char *)ib_bytestr_const_ptr(bs_in);
    buf_out = buf_out_end = (char *)ib_mpool_calloc(mp, 2, ib_bytestr_length(bs_in));
    if (buf_out == NULL) {
        return IB_EALLOC;
    }

    /* As SQL can be injected into a string, the normalization
     * needs to start after the first quote character if one
     * exists.
     *
     * First try single quote, then double, then none.
     *
     * TODO: Handle returning multiple transformations:
     *       1) Straight normalization
     *       2) Normalization as if with single quotes (starting point
     *          should be based on straight normalization)
     *       3) Normalization as if with double quotes (starting point
     *          should be based on straight normalization)
     */
    buf_in_start = memchr(buf_in, CHAR_SINGLE, ib_bytestr_length(bs_in));
    if (buf_in_start == NULL) {
        buf_in_start = memchr(buf_in, CHAR_DOUBLE, ib_bytestr_length(bs_in));
    }
    if (buf_in_start == NULL) {
        buf_in_start = (char *)buf_in;
        buf_in_len = ib_bytestr_length(bs_in);
    }
    else {
        ++buf_in_start; /* After the quote. */
        buf_in_len = ib_bytestr_length(bs_in) - (buf_in_start - buf_in);
    }

    /* Copy the leading string if one exists. */
    if (buf_in_start != buf_in) {
        lead_len = buf_in_start - buf_in;
        memcpy(buf_out, buf_in, lead_len);
        buf_out_end += lead_len;
    }

    /* Copy the normalized tokens as a space separated list. Since
     * the tokenizer does not backtrack, and the normalized values
     * are always equal to or less than the original length, the
     * tokens are written back to the beginning of the original
     * buffer.
     */
    tokenize_fn = cfg->fold ? filter_fold : sqli_tokenize;
    sfilter_reset(&sf, buf_in_start, buf_in_len);
    buf_out_len = 0;
    prev_token_type = 0;
    while (tokenize_fn(&sf, &current)) {
        size_t token_len = strlen(current.val);
        ib_log_debug2(ib, "SQLi TOKEN: %c \"%s\"", current.type, current.val);

        /* Add in the space if required. */
        if ((buf_out_end != buf_out) &&
            (current.type != 'o') &&
            (prev_token_type != 'o') &&
            (current.type != ',') &&
            (*(buf_out_end - 1) != ','))
        {
            *buf_out_end = ' ';
            buf_out_end += 1;
            ++buf_out_len;
        }

        /* Copy the token value. */
        memcpy(buf_out_end, current.val, token_len);
        buf_out_end += token_len;
        buf_out_len += token_len;

        prev_token_type = current.type;
    }

    /* Mark as modified. */
    *pflags = IB_TFN_FMODIFIED;

    /* Create the output field wrapping bs_out. */
    buf_out_len += lead_len;
    rc = ib_bytestr_alias_mem(&bs_out, mp, (uint8_t *)buf_out, buf_out_len);
    if (rc != IB_OK) {
        return rc;
    }
    return ib_field_create(field_out, mp,
                           field_in->name, field_in->nlen,
                           IB_FTYPE_BYTESTR,
                           ib_ftype_bytestr_mutable_in(bs_out));
}

/*********************************
 * Operators
 *********************************/

static
ib_status_t sqli_op_create(
    ib_context_t  *ctx,
    const char    *parameters,
    void         **instance_data,
    void          *cbdata
)
{
    ib_engine_t *ib = ib_context_get_engine(ctx);

    if (parameters == NULL) {
        ib_log_error(ib, "Missing parameter for operator sqli");
        return IB_EINVAL;
    }

    // TODO: Support loading external patterns.
    if (strcmp("default", parameters) != 0) {
        ib_log_notice(ib,
                      "SQLi external data not yet supported for sqli."
                      " Use \"default\" for now.");
    }

    return IB_OK;
}

static
ib_status_t sqli_op_execute(
    ib_tx_t    *tx,
    void       *instance_data,
    ib_field_t *field,
    ib_field_t *capture,
    ib_num_t   *result,
    void       *cbdata
)
{
    assert(tx     != NULL);
    assert(field  != NULL);
    assert(result != NULL);

    sfilter sf;
    ib_bytestr_t *bs;
    ib_status_t rc;

    *result = 0;

    /* Currently only bytestring types are supported.
     * Other types will just get passed through. */
    if (field->type != IB_FTYPE_BYTESTR) {
        return IB_OK;
    }

    rc = ib_field_value(field, ib_ftype_bytestr_mutable_out(&bs));
    if (rc != IB_OK) {
        return rc;
    }

    /* Run through libinjection. */
    // TODO: Support alternative SQLi pattern lookup
    if (is_sqli(&sf, (const char *)ib_bytestr_const_ptr(bs), ib_bytestr_length(bs), is_sqli_pattern)) {
        ib_log_debug_tx(tx, "Matched SQLi pattern: %s", sf.pat);
        *result = 1;
    }

    return IB_OK;
}



/*********************************
 * Module Functions
 *********************************/

/* Called to initialize a module (on load). */
static ib_status_t sqli_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    assert(ib != NULL);
    assert(m != NULL);

    ib_mpool_t *pool = ib_engine_pool_config_get(ib);
    sqli_config_t *sqli_config;
    ib_status_t rc;

    ib_log_debug(ib, "Initializing %s module.", MODULE_NAME_STR);

    /* Register normalizeSqli transformation. */
    sqli_config = (sqli_config_t *)ib_mpool_calloc(pool, 1, sizeof(*sqli_config));
    if (sqli_config == NULL) {
        return IB_EALLOC;
    }
    rc = ib_tfn_register(ib, "normalizeSqli", sqli_normalize_tfn,
                         IB_TFN_FLAG_NONE, sqli_config);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register normalizeSqliFold transformation. */
    sqli_config = (sqli_config_t *)ib_mpool_calloc(pool, 1, sizeof(*sqli_config));
    if (sqli_config == NULL) {
        return IB_EALLOC;
    }
    sqli_config->fold = 1;
    rc = ib_tfn_register(ib, "normalizeSqliFold", sqli_normalize_tfn,
                         IB_TFN_FLAG_NONE, sqli_config);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register is_sqli operator. */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "is_sqli",
        IB_OP_CAPABILITY_NON_STREAM,
        sqli_op_create, NULL,
        NULL, NULL,
        sqli_op_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/* Called to finish a module (on unload). */
static ib_status_t sqli_fini(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    ib_log_debug(ib, "Finish %s module.", MODULE_NAME_STR);

    return IB_OK;
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    NULL,                                /* Config directive map */
    sqli_init,                           /* Initialize function */
    NULL,                                /* Callback data */
    sqli_fini,                           /* Finish function */
    NULL,                                /* Callback data */
    NULL,                                /* Context open function */
    NULL,                                /* Callback data */
    NULL,                                /* Context close function */
    NULL,                                /* Callback data */
    NULL,                                /* Context destroy function */
    NULL                                 /* Callback data */
);
