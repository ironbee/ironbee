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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "core_private.h"

#include <ironbee/bytestr.h>
#include <ironbee/capture.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/ipset.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>

/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
typedef ib_status_t (*num_compare_fn_t)(ib_num_t n1, ib_num_t n2, ib_num_t *result);

/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
typedef ib_status_t (*float_compare_fn_t)(ib_float_t n1, ib_float_t n2, ib_num_t *result);

/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t num_gt(ib_num_t n1, ib_num_t n2, ib_num_t *result)
{
    *result = ( n1 > n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t num_lt(ib_num_t n1, ib_num_t n2, ib_num_t *result)
{
    *result = ( n1 < n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t num_ge(ib_num_t n1, ib_num_t n2, ib_num_t *result)
{
    *result = ( n1 >= n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t num_le(ib_num_t n1, ib_num_t n2, ib_num_t *result)
{
    *result = ( n1 <= n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t num_eq(ib_num_t n1, ib_num_t n2, ib_num_t *result)
{
    *result = ( n1 == n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t num_ne(ib_num_t n1, ib_num_t n2, ib_num_t *result)
{
    *result = ( n1 != n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t float_gt(ib_float_t n1, ib_float_t n2, ib_num_t *result)
{
    *result = ( n1 > n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t float_lt(ib_float_t n1, ib_float_t n2, ib_num_t *result)
{
    *result = ( n1 < n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t float_ge(ib_float_t n1, ib_float_t n2, ib_num_t *result)
{
    *result = ( n1 >= n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t float_le(ib_float_t n1, ib_float_t n2, ib_num_t *result)
{
    *result = ( n1 <= n2 );
    return IB_OK;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t float_eq(ib_float_t n1, ib_float_t n2, ib_num_t *result)
{
    return IB_EINVAL;
}
/**
 * Perform a comparison of two inputs and store the boolean result in result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
static ib_status_t float_ne(ib_float_t n1, ib_float_t n2, ib_num_t *result)
{
    return IB_EINVAL;
}

/**
 * Allocate a buffer and unescape operator arguments.
 * @param[in] ib IronBee engine used for logging.
 * @param[in] mp Memory pool that @a str_unesc will be allocated out of.
 * @param[in] str The parameter string to be unescaped.
 * @param[out] str_unesc On a successful unescaping, a new buffer allocated
 *             out of @a mp will be assigned to @a *str_unesc with the
 *             unescaped string in it.
 * @param[out] str_unesc_len On success *str_unesc_len is assigned the length
 *             of the unescaped string. Note that this may be different
 *             that strlen(*str_unesc) because \\x00 will place a NULL
 *             in the middle of @a *str_unesc.
 *             This string should be wrapped in an ib_bytestr_t.
 * @returns IB_OK on success. IB_EALLOC on error. IB_EINVAL if @a str
 *          was unable to be unescaped.
 */
static ib_status_t unescape_op_args(ib_engine_t *ib,
                                    ib_mpool_t *mp,
                                    char **str_unesc,
                                    size_t *str_unesc_len,
                                    const char *str)
{
    assert(mp!=NULL);
    assert(ib!=NULL);
    assert(str!=NULL);
    assert(str_unesc!=NULL);
    assert(str_unesc_len!=NULL);

    ib_status_t rc;
    const size_t str_len = strlen(str);

    /* Temporary unescaped string holder. */
    char* tmp_unesc = ib_mpool_alloc(mp, str_len+1);
    size_t tmp_unesc_len;

    if ( tmp_unesc == NULL ) {
        return IB_EALLOC;
    }

    rc = ib_util_unescape_string(tmp_unesc,
                                 &tmp_unesc_len,
                                 str,
                                 str_len,
                                 IB_UTIL_UNESCAPE_NULTERMINATE);

    if ( rc != IB_OK ) {
        ib_log_notice(ib, "Failed to unescape string: %s", str);
        return rc;
    }

    /* Commit changes on success. */
    *str_unesc = tmp_unesc;
    *str_unesc_len = tmp_unesc_len;

    return IB_OK;
}

/**
 * Instance data for numop.
 */
struct numop_instance_data_t {
    /* One of these two fields must be NULL */
    const ib_field_t *f; /**< Number field to compare to. */
    const ib_var_expand_t *expand; /**< Var expand to compare to. */
};
typedef struct numop_instance_data_t numop_instance_data_t;

/**
 * Create function for the "str" family of operators
 *
 * @param[in]  ctx The current IronBee context
 * @param[in]  parameters Constant parameters
 * @param[out] instance_data Instance Data.
 * @param[in]  cbdata Callback data.
 *
 * @returns Status code
 */
static
ib_status_t strop_create(
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);

    ib_engine_t *ib = ib_context_get_engine(ctx);
    ib_mpool_t *mp = ib_context_get_mpool(ctx);
    assert(ib != NULL);
    assert(mp != NULL);

    ib_status_t rc;
    char *str;
    size_t str_len;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    rc = unescape_op_args(ib, mp, &str, &str_len, parameters);
    if (rc != IB_OK) {
        return rc;
    }

    ib_var_expand_t *data;
    // @todo Catch and report error_message and error_offset.
    rc = ib_var_expand_acquire(
        &data,
        mp,
        str, str_len,
        ib_engine_var_config_get(ib),
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    *(ib_var_expand_t **)instance_data = data;

    return IB_OK;
}

/**
 * Execute function for the "streq" operator
 *
 * @param[in]  tx Current transaction.
 * @param[in]  instance_data Instance data.
 * @param[in]  field Field value
 * @param[in]  capture Collection to capture to.
 * @param[out] result Pointer to number in which to store the result
 * @param[in]  cbdata Callback data.
 *
 * @returns Status code
 */
static
ib_status_t op_streq_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(tx != NULL);
    assert(instance_data != NULL);
    assert(field != NULL);
    assert(result != NULL);

    ib_status_t rc;
    bool         case_insensitive;
    case_insensitive = (cbdata != NULL);

    const char  *expanded;
    size_t       expanded_length;

    /* Expand the string */
    rc = ib_var_expand_execute(
        (const ib_var_expand_t *)instance_data,
        &expanded, &expanded_length,
        tx->mp,
        tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Handle NUL-terminated strings and byte strings */
    if (field->type == IB_FTYPE_NULSTR) {
        const char *fval;
        rc = ib_field_value(field, ib_ftype_nulstr_out(&fval));
        if (rc != IB_OK) {
            return rc;
        }

        if (case_insensitive) {
            *result = (strncasecmp(fval, expanded, expanded_length) == 0);
        }
        else {
            *result = (strncmp(fval, expanded, expanded_length) == 0);
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *value;
        size_t                len;

        rc = ib_field_value(field, ib_ftype_bytestr_out(&value));
        if (rc != IB_OK) {
            return rc;
        }

        if (ib_bytestr_const_ptr(value) == NULL) {
            /* Null matches nothing. */
            *result = 0;
            return IB_OK;
        }

        len = ib_bytestr_length(value);

        if (len == expanded_length) {
            if (case_insensitive) {
                *result = 1;
                const char *v = (const char *)ib_bytestr_const_ptr(value);
                for (size_t i = 0; i < len; ++i) {
                    if (tolower(expanded[i]) != tolower(v[i])) {
                        *result = 0;
                        break;
                    }
                }
            }
            else {
                *result = (
                    memcmp(ib_bytestr_const_ptr(value), expanded, len) == 0
                );
            }
        }
        else {
            *result = 0;
        }
    }
    else {
        return IB_EINVAL;
    }

    if (capture != NULL && *result) {
        rc = ib_capture_clear(capture);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_capture_set_item(capture, 0, tx->mp, field);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Execute function for the "contains" operator
 *
 * @param[in]  tx Current transaction.
 * @param[in]  instance_data Instance data.
 * @param[in]  field Field value
 * @param[in]  capture Collection to capture to.
 * @param[out] result Pointer to number in which to store the result
 * @param[in]  cbdata Callback data.
 *
 * @returns Status code
 */
static
ib_status_t op_contains_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(tx != NULL);
    assert(instance_data != NULL);
    assert(field != NULL);
    assert(result != NULL);

    ib_status_t  rc = IB_OK;

    const char  *expanded;
    size_t       expanded_length;

    /* Expand the string */
    rc = ib_var_expand_execute(
        (const ib_var_expand_t *)instance_data,
        &expanded, &expanded_length,
        tx->mp,
        tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    if (field->type == IB_FTYPE_NULSTR) {
        const char *s;
        rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            return rc;
        }

        if (memmem(s, strlen(s), expanded, expanded_length) == NULL) {
            *result = 0;
        }
        else {
            *result = 1;
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *str;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&str));
        if (rc != IB_OK) {
            return rc;
        }
        if (ib_bytestr_const_ptr(str) == NULL) {
            /* Null matches nothing. */
            *result = 0;
            return IB_OK;
        }

        *result = (
            ib_strstr_ex(
                (const char *)ib_bytestr_const_ptr(str),
                ib_bytestr_length(str),
                expanded, expanded_length
            ) != NULL
        );
    }
    else {
        return IB_EINVAL;
    }

    if (capture != NULL && *result) {
        ib_field_t *f;
        const char *name;

        rc = ib_capture_clear(capture);
        if (rc != IB_OK) {
            return rc;
        }

        name = ib_capture_name(0);
        rc = ib_field_create_bytestr_alias(
            &f,
            tx->mp,
            name, strlen(name),
            (uint8_t *)expanded,
            strlen(expanded)
        );
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_capture_set_item(capture, 0, tx->mp, f);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}


/**
 * Create function for the "match" and "imatch" operators.
 *
 * @param[in]  ctx           The current IronBee context (unused).
 * @param[in]  parameters    Parameters.
 * @param[out] instance_data Instance_data.
 * @param[in]  cbdata        Callback data.
 *
 * @returns
 * - IB_OK if no failure.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a parameters as space separated list.
 */
static
ib_status_t op_match_create(
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx           != NULL);
    assert(instance_data != NULL);

    ib_status_t  rc;
    ib_hash_t   *set;
    bool         case_insensitive;
    char        *copy;
    size_t       copy_len;

    ib_engine_t *ib = ib_context_get_engine(ctx);
    ib_mpool_t *mp = ib_context_get_mpool(ctx);
    assert(ib != NULL);
    assert(mp != NULL);

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    case_insensitive = (cbdata != NULL);

    /* Make a copy of the parameters to operate on. */
    rc = unescape_op_args(ib, mp, &copy, &copy_len, parameters);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    if (case_insensitive) {
        rc = ib_hash_create_nocase(&set, mp);
    }
    else {
        rc = ib_hash_create(&set, mp);
    }
    if (rc != IB_OK) {
        assert(rc == IB_EALLOC); /* Guaranteed by hash. */
        return IB_EALLOC;
    }

    /* Fill set. */
    {
        const char *p;   /* Current parameter. */
        const char *end; /* End of all copy. */
        const char *n;   /* Next space. */

        end = copy + strlen(copy);
        p = copy;
        n = p;
        while (n < end) {
            n = memchr(p, ' ', end - p);
            if (n == NULL) {
                n = end;
            }

            rc = ib_hash_set_ex(set, p, n - p, (void *)1);
            if (rc != IB_OK) {
                assert(rc == IB_EALLOC); /* Guaranteed by hash. */
                return IB_EALLOC;
            }

            p = n + 1; /* Skip space. */
        }
    }

    /* Done */
    *(ib_hash_t **)instance_data = set;

    return IB_OK;
}

/**
 * Execute function for the "match" and "imatch" operators.
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK if no failure, regardless of match status.
 * - IB_EUNKNOWN on unexpected field error.
 * - IB_EINVAL on incompatible field, i.e., other than string or bytestring.
 * - IB_EALLOC on allocation failure.
 */
static
ib_status_t op_match_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(tx            != NULL);
    assert(instance_data != NULL);
    assert(field         != NULL);
    assert(result        != NULL);

    ib_status_t        rc;
    const ib_hash_t   *set;
    const char        *s;
    size_t             length;
    void              *v;

    set = (const ib_hash_t *)instance_data;

    if (field->type == IB_FTYPE_NULSTR) {
        rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            return rc;
        }

        if (s == NULL) {
            ib_log_error_tx(tx, "Failed to get NULSTR from field");
            return IB_EUNKNOWN;
        }
        length = strlen(s);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            return rc;
        }

        assert(bs != NULL);

        s = (const char *)ib_bytestr_const_ptr(bs);
        length = ib_bytestr_length(bs);
    }
    else {
        return IB_EINVAL;
    }

    /* NULL matches nothing. */
    if (s == NULL) {
        *result = 0;
    }
    else {
        rc = ib_hash_get_ex(set, &v, s, length);
        if (rc != IB_ENOENT && rc != IB_OK) {
            ib_log_error_tx(
                tx,
                "Unexpected hash get error: %s",
                ib_status_to_string(rc)
            );
            return IB_EUNKNOWN;
        }
        *result = (rc == IB_OK);
    }

    return IB_OK;
}

/**
 * Create function for the "ipmatch" operator
 *
 * @param[in]  ctx           The current IronBee context (unused).
 * @param[in]  parameters    Parameters.
 * @param[out] instance_data Instance data.
 * @param[in]  cbdata        Callback data.
 *
 * @returns
 * - IB_OK if no failure.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a parameters as IP addresses or networks.
 */
static
ib_status_t op_ipmatch_create(
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
    ib_mpool_t *mp = ib_context_get_mpool(ctx);
    assert(ib != NULL);
    assert(mp != NULL);

    ib_status_t        rc             = IB_OK;
    char              *copy           = NULL;
    size_t             copy_len       = 0;
    char              *p              = NULL;
    size_t             num_parameters = 0;
    ib_ipset4_entry_t *entries        = NULL;
    size_t             i              = 0;
    ib_ipset4_t       *ipset          = NULL;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    /* Make a copy of the parameters to operate on. */
    rc = unescape_op_args(ib, mp, &copy, &copy_len, parameters);
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error unescaping rule parameters '%s'", parameters
        );
        return IB_EALLOC;
    }

    ipset = ib_mpool_alloc(mp, sizeof(*ipset));
    if (ipset == NULL) {
        return IB_EALLOC;
    }

    /* Count the number of parameters. */
    for (p = copy; *p != '\0';) {
        while (*p == ' ') {++p;}
        if (*p != '\0') {
            ++num_parameters;
            while (*p && *p != ' ') {++p;}
        }
    }

    entries = ib_mpool_alloc(mp, num_parameters * sizeof(*entries));
    if (entries == NULL) {
        return IB_EALLOC;
    }

    /* Fill entries. */
    i = 0;
    for (p = strtok(copy, " ");  p != NULL;  p = strtok(NULL, " ") ) {
        assert(i < num_parameters);
        entries[i].data = NULL;
        rc = ib_ip4_str_to_net(p, &entries[i].network);
        if (rc == IB_EINVAL) {
            rc = ib_ip4_str_to_ip(p, &(entries[i].network.ip));
            if (rc == IB_OK) {
                entries[i].network.size = 32;
            }
        }
        if (rc != IB_OK) {
            ib_log_error(ib, "Error parsing: %s", p);
            return rc;
        }

        ++i;
    }
    assert(i == num_parameters);

    rc = ib_ipset4_init(
        ipset,
        NULL, 0,
        entries, num_parameters
    );
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error initializing internal data: %s",
            ib_status_to_string(rc)
        );
        return rc;
    }

    /* Done */
    *(ib_ipset4_t **)instance_data = ipset;

    return IB_OK;
}

/**
 * Execute function for the "ipmatch" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK if no failure, regardless of match status.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a field as IP address.
 */
static
ib_status_t op_ipmatch_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(tx            != NULL);
    assert(instance_data != NULL);
    assert(field         != NULL);
    assert(result        != NULL);

    ib_status_t        rc               = IB_OK;
    const ib_ipset4_t *ipset            = NULL;
    ib_ip4_t           ip               = 0;
    const char        *ipstr            = NULL;
    char               ipstr_buffer[17] = "\0";

    ipset = (const ib_ipset4_t *)instance_data;

    if (field->type == IB_FTYPE_NULSTR) {
        rc = ib_field_value(field, ib_ftype_nulstr_out(&ipstr));
        if (rc != IB_OK) {
            return rc;
        }

        if (ipstr == NULL) {
            ib_log_error_tx(tx, "Failed to get NULSTR from field.");
            return IB_EUNKNOWN;
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            return rc;
        }
        assert(bs != NULL);
        if (ib_bytestr_const_ptr(bs) == NULL) {
            /* Null matches nothing. */
            *result = 0;
            return IB_OK;
        }

        if (ib_bytestr_length(bs) > 16) {
            return IB_EINVAL;
        }

        strncpy(
            ipstr_buffer,
            (const char *)ib_bytestr_const_ptr(bs),
            ib_bytestr_length(bs)
        );
        ipstr_buffer[ib_bytestr_length(bs)] = '\0';
        ipstr = ipstr_buffer;
    }
    else {
        return IB_EINVAL;
    }

    rc = ib_ip4_str_to_ip(ipstr, &ip);
    if (rc != IB_OK) {
        ib_log_info_tx(tx, "Could not parse as IP: %s", ipstr);
        return rc;
    }

    rc = ib_ipset4_query(ipset, ip, NULL, NULL, NULL);
    if (rc == IB_ENOENT) {
        *result = 0;
    }
    if (rc != IB_OK) {
        return rc;
    }
    *result = 1;
    if (capture != NULL && *result) {
        rc = ib_capture_clear(capture);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_capture_set_item(capture, 0, tx->mp, field);
        if (rc != IB_OK) {
            return rc;
        }
    }
    return IB_OK;
}


/**
 * Create function for the "ipmatch6" operator
 *
 * @param[in]  ctx           The current IronBee context (unused).
 * @param[in]  parameters    Parameters.
 * @param[out] instance_data Instance_data.
 * @param[in]  cbdata        Callback data.
 *
 * @returns
 * - IB_OK if no failure.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a parameters as IP addresses or networks.
 */
static
ib_status_t op_ipmatch6_create(
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
    ib_mpool_t *mp = ib_context_get_mpool(ctx);
    assert(ib != NULL);
    assert(mp != NULL);

    ib_status_t        rc             = IB_OK;
    char              *copy           = NULL;
    size_t             copy_len       = 0;
    char              *p              = NULL;
    size_t             num_parameters = 0;
    ib_ipset6_entry_t *entries        = NULL;
    size_t             i              = 0;
    ib_ipset6_t       *ipset          = NULL;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    /* Make a copy of the parameters to operate on. */
    rc = unescape_op_args(ib, mp, &copy, &copy_len, parameters);
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error unescaping rule parameters \"%s\"", parameters
        );
        return IB_EALLOC;
    }

    ipset = ib_mpool_alloc(mp, sizeof(*ipset));
    if (ipset == NULL) {
        return IB_EALLOC;
    }

    /* Count the number of parameters. */
    for (p = copy; *p != '\0';) {
        while (*p == ' ') {++p;}
        if (*p != '\0') {
            ++num_parameters;
            while (*p && *p != ' ') {++p;}
        }
    }

    entries = ib_mpool_alloc(mp, num_parameters * sizeof(*entries));
    if (entries == NULL) {
        return IB_EALLOC;
    }

    /* Fill entries. */
    i = 0;
    for (p = strtok(copy, " ");  p != NULL;  p = strtok(NULL, " ") ) {
        assert(i < num_parameters);
        entries[i].data = NULL;
        rc = ib_ip6_str_to_net(p, &entries[i].network);
        if (rc == IB_EINVAL) {
            rc = ib_ip6_str_to_ip(p, &(entries[i].network.ip));
            if (rc == IB_OK) {
                entries[i].network.size = 128;
            }
        }
        if (rc != IB_OK) {
            ib_log_error(ib, "Error parsing: %s", p);
            return rc;
        }

        ++i;
    }
    assert(i == num_parameters);

    rc = ib_ipset6_init(
        ipset,
        NULL, 0,
        entries, num_parameters
    );
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error initializing internal data: %s",
            ib_status_to_string(rc)
        );
        return rc;
    }

    /* Done */
    *(ib_ipset6_t **)instance_data = ipset;

    return IB_OK;
}

/**
 * Execute function for the "ipmatch6" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK if no failure, regardless of match status.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a field as IP address.
 */
static
ib_status_t op_ipmatch6_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(tx            != NULL);
    assert(instance_data != NULL);
    assert(field         != NULL);
    assert(result        != NULL);

    ib_status_t        rc               = IB_OK;
    const ib_ipset6_t *ipset            = NULL;
    ib_ip6_t           ip               = {{0, 0, 0, 0}};
    const char        *ipstr            = NULL;
    char               ipstr_buffer[41] = "\0";

    ipset = (const ib_ipset6_t *)instance_data;

    if (field->type == IB_FTYPE_NULSTR) {
        rc = ib_field_value(field, ib_ftype_nulstr_out(&ipstr));
        if (rc != IB_OK) {
            return rc;
        }

        if (ipstr == NULL) {
            ib_log_error_tx(tx, "Failed to get NULSTR from field.");
            return IB_EUNKNOWN;
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            return rc;
        }

        assert(bs != NULL);
        assert(ib_bytestr_length(bs) < 41);

        if (ib_bytestr_const_ptr(bs) == NULL) {
            /* Null matches nothing. */
            *result = 0;
            return IB_OK;
        }

        strncpy(
            ipstr_buffer,
            (const char *)ib_bytestr_const_ptr(bs),
            ib_bytestr_length(bs)
        );
        ipstr_buffer[ib_bytestr_length(bs)] = '\0';
        ipstr = ipstr_buffer;
    }
    else {
        return IB_EINVAL;
    }

    rc = ib_ip6_str_to_ip(ipstr, &ip);
    if (rc != IB_OK) {
        ib_log_info_tx(tx, "Could not parse as IP: %s", ipstr);
        return rc;
    }

    rc = ib_ipset6_query(ipset, ip, NULL, NULL, NULL);
    if (rc == IB_ENOENT) {
        *result = 0;
    }
    if (rc != IB_OK) {
        return rc;
    }
    *result = 1;
    if (capture != NULL && *result) {
        rc = ib_capture_clear(capture);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_capture_set_item(capture, 0, tx->mp, field);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Expand an expansion and then convert the result to a number-type if
 * possible.  Otherwise, leave it as a string.
 *
 * If no conversion is performed because none is necessary,
 * IB_OK is returned and @a out_field is set to NULL.
 *
 * @param[in] tx Current transaction.
 * @param[in] in The expansion to expand and convert.
 * @param[out] out_field The field that the in_field is converted to.
 *             If no conversion is performed, this is set to NULL.
 * @returns
 *   - IB_OK On success.
 *   - IB_EALLOC On memory failure.
 *   - IB_EINVAL If an expandable value cannot be expanded.
 *   - Other returned by ib_var_expand_execute()
 */
static
ib_status_t expand_field_num(
    const ib_tx_t          *tx,
    const ib_var_expand_t  *in,
    ib_field_t            **out_field
)
{
    assert(tx     != NULL);
    assert(tx->mp != NULL);
    assert(in     != NULL);

    const char *expanded;
    size_t expanded_len;
    ib_field_t *tmp_field;
    ib_status_t rc;

    /* Expand */
    rc = ib_var_expand_execute(
        in,
        &expanded, &expanded_len,
        tx->mp,
        tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Wrap the string into a field and set it to the tmp_field.
     * We will now try to expand tmp_field into a number. If we
     * fail, we return tmp_field in *out_field. */
    rc = ib_field_create_bytestr_alias(
        &tmp_field,
        tx->mp,
        "expanded num", sizeof("expanded num"),
        (uint8_t *)expanded, expanded_len);
    if (rc != IB_OK) {
        return rc;
    }

    /* Attempt a num. */
    rc = ib_field_convert(
        tx->mp,
        IB_FTYPE_NUM,
        tmp_field,
        out_field);
    if (rc == IB_OK) {
        return IB_OK;
    }

    /* Attempt a float. */
    rc = ib_field_convert(
        tx->mp,
        IB_FTYPE_FLOAT,
        tmp_field,
        out_field);
    if (rc == IB_OK) {
        return IB_OK;
    }

    /* We cannot convert the expanded string. Return the string. */
    *out_field = tmp_field;
    return rc;
}

/**
 * Store a number in the capture buffer
 *
 * @param[in] mp Memory pool to use.
 * @param[in] capture Capture field.
 * @param[in] n The capture number
 * @param[in] value The actual value
 */
static
ib_status_t capture_float(
    ib_mpool_t *mp,
    ib_field_t *capture,
    int         n,
    ib_float_t  value
)
{
    assert(mp != NULL);
    assert(capture != NULL);

    ib_status_t rc;
    ib_field_t *field;
    const char *name;
    const char *str;

    name = ib_capture_name(n);

    str = ib_float_to_string(mp, value);
    if (str == NULL) {
        return IB_EALLOC;
    }
    rc = ib_field_create_bytestr_alias(
        &field,
        mp,
        name, strlen(name),
        (uint8_t *)str, strlen(str)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_capture_set_item(capture, n, mp, field);
    return rc;
}

/**
 * Store a number in the capture buffer
 *
 * @param[in] mp Memory pool to use.
 * @param[in] capture Capture field.
 * @param[in] n The capture number
 * @param[in] value The actual value
 */
static
ib_status_t capture_num(
    ib_mpool_t *mp,
    ib_field_t *capture,
    int         n,
    ib_num_t    value
)
{
    assert(mp != NULL);
    assert(capture != NULL);

    ib_status_t rc;
    ib_field_t *field;
    const char *name;
    const char *str;

    name = ib_capture_name(n);

    str = ib_num_to_string(mp, value);
    if (str == NULL) {
        return IB_EALLOC;
    }
    rc = ib_field_create_bytestr_alias(
        &field,
        mp,
        name, strlen(name),
        (uint8_t *)str, strlen(str)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_capture_set_item(capture, n, mp, field);
    return rc;
}

/**
 * Map the two input types to a set of types that they should be converted to.
 *
 * Below is conversion table with the two input types on the left,
 * an arrow (->) and the resultant flags on the right.
 *
 * - float, float   -> float
 * - float, int     -> float
 * - int, int       -> int
 *
 * The caller should try to convert the types in the given order
 * until both values can be converted successfully to an acceptable type
 * for the operator.
 *
 * @param[in] lh_field The field on the left-hand side of the operation.
 * @param[in] rh_field The field on the right-hand side of the operation.
 *            In the current Rule Language this is the parameter.
 *            Eg: @@eq "3"
 * @param[out] type This output type that the fields should be
 *             converted to.
 * @returns
 *   - IB_OK Success. type_flags is set.
 *   - IB_EINVAL An invalid type of @a lh_field or @a rh_field was observed.
 */
static ib_status_t select_math_type_conversion(
    const ib_field_t *lh_field,
    const ib_field_t *rh_field,
    ib_ftype_t *type)
{
    assert(lh_field);
    assert(rh_field);
    assert(type);

    if (lh_field->type == IB_FTYPE_FLOAT && rh_field->type == IB_FTYPE_FLOAT) {
        *type = IB_FTYPE_FLOAT;
        return IB_OK;
    }

    if (lh_field->type == IB_FTYPE_NUM && rh_field->type == IB_FTYPE_NUM) {
        *type = IB_FTYPE_NUM;
        return IB_OK;
    }

    if ((lh_field->type == IB_FTYPE_FLOAT && rh_field->type == IB_FTYPE_NUM)
        ||
        (lh_field->type == IB_FTYPE_NUM && rh_field->type == IB_FTYPE_FLOAT))
    {
        *type = IB_FTYPE_FLOAT;
        return IB_OK;
    }

    return IB_EINVAL;
}

/**
 * Convert the two in-fields into a number in the out fields.
 *
 * This function does a few important functions common to math and logic
 * operations.
 *
 *   - Expand rh_expand if rh_expand is non-NULL; otherwise use rh_in.
 *   - Finds a target type that rh_in and lh_in must be converted to.
 *   - If a float is disallowed, this applies that check and returns IB_EINVAL.
 *   - Both fields are converted and put into rh_out and lh_out.
 *     NOTE: If conversion is NOT necessary, then the original
 *           rh_in or lh_in will be placed in the output variables.
 *           This will strip the const-ness of the value.
 *           This is not relevant to how math operations
 *           use these fields, so user-beware.
 *
 * @param tx Current transaction.
 * @param rh_expand What to expand to generate the right hand side.
 * @param lh_in Left-hand operand input.
 * @param rh_in Right-hand operand input.
 * @param lh_out Left-hand operand out. This may equal @a lh_in.
 * @param rh_out Right-hand operand out. This may equal @a rh_in.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If a type cannot be converted.
 */
static
ib_status_t prepare_math_operands(
    const ib_tx_t          *tx,
    const ib_var_expand_t  *rh_expand,
    const ib_field_t       *lh_in,
    const ib_field_t       *rh_in,
    ib_field_t            **lh_out,
    ib_field_t            **rh_out
)
{
    assert(tx != NULL);
    assert(tx->mp != NULL);
    assert(lh_in != NULL);
    assert(
        (rh_in != NULL && rh_expand == NULL) ||
        (rh_in == NULL && rh_expand != NULL)
    );
    assert(lh_out != NULL);
    assert(rh_out != NULL);

    ib_ftype_t type = 0;
    ib_status_t rc;

    /* An intermediate holding place for produced fields. */
    ib_field_t *tmp_field = NULL;

    /* First, expand the right hand input. */
    if (rh_expand != NULL) {
        rc = expand_field_num(tx, rh_expand, &tmp_field);
        if (rc != IB_OK) {
            return rc;
        }
    }
    if (tmp_field) {
        *rh_out = (ib_field_t *)tmp_field;
    }
    else {
        *rh_out = (ib_field_t *)rh_in;
    }

    /* Pick a type using our type rules. */
    rc = select_math_type_conversion(lh_in, *rh_out, &type);
    if (rc != IB_OK) {
        *rh_out = NULL;
        *lh_out = NULL;
        return rc;
    }

    /* Convert rh_field. */
    rc = ib_field_convert(tx->mp, type, *rh_out, &tmp_field);
    if (rc != IB_OK){
        *rh_out = NULL;
        *lh_out = NULL;
        return rc;
    }
    else if (tmp_field) {
        *rh_out = tmp_field;
    }

    /* Convert lh_field. */
    rc = ib_field_convert(tx->mp, type, lh_in, &tmp_field);
    if (rc != IB_OK) {
        *rh_out = NULL;
        *lh_out = NULL;
        return rc;
    }
    else if (tmp_field) {
        *lh_out = tmp_field;
    }
    else {
        *lh_out = (ib_field_t *)lh_in;
    }

    return IB_OK;
}

/**
 * @param[in] tx Current transaction.
 * @param[in] instance_data numop instance data.
 * @param[in] field The field used.
 * @param[in] capture Collection to capture to.
 * @param[in] num_compare If this is an ib_num_t, use this to compare.
 * @param[in] float_compare If this is an ib_float_t, use this to compare.
 * @param[out] result The result is store here.
 */
static
ib_status_t execute_compare(
    const ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    num_compare_fn_t num_compare,
    float_compare_fn_t float_compare,
    ib_num_t *result
)
{
    assert(instance_data != NULL);
    assert(result != NULL);
    assert(tx != NULL);
    assert(tx->mp != NULL);

    const numop_instance_data_t *ndata =
        (const numop_instance_data_t *)instance_data;
    ib_status_t rc;
    ib_field_t *rh_field = NULL;
    ib_field_t *lh_field = NULL;

    rc = prepare_math_operands(
        tx,
        ndata->expand,
        field,
        ndata->f,
        &lh_field,
        &rh_field
    );
    if (rc != IB_OK) {
        return rc;
    }

    if (rh_field->type == IB_FTYPE_NUM) {
        ib_num_t param_value;
        ib_num_t value;

        /* Pull out param value for comparison. */
        rc = ib_field_value(rh_field, ib_ftype_num_out(&param_value));
        if (rc != IB_OK) {
            return rc;
        }

        /* Pull out param value for comparison. */
        rc = ib_field_value(lh_field, ib_ftype_num_out(&value));
        if (rc != IB_OK) {
            return rc;
        }

        rc = num_compare(value, param_value, result);
        if (rc != IB_OK) {
            return rc;
        }
        if (capture != NULL && *result) {
            rc = ib_capture_clear(capture);
            if (rc != IB_OK) {
                return rc;
            }
            rc = capture_num(tx->mp, capture, 0, value);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }
    else if (rh_field->type == IB_FTYPE_FLOAT) {
        ib_float_t param_value;
        ib_float_t value;

        /* Pull out param value for comparison. */
        rc = ib_field_value(rh_field, ib_ftype_float_out(&param_value));
        if (rc != IB_OK) {
            return rc;
        }

        /* Pull out param value for comparison. */
        rc = ib_field_value(lh_field, ib_ftype_float_out(&value));
        if (rc != IB_OK) {
            return rc;
        }

        /* Do the comparison */
        rc = float_compare(value, param_value, result);
        if (rc != IB_OK) {
            return rc;
        }
        if (capture != NULL && *result) {
            rc = ib_capture_clear(capture);
            if (rc != IB_OK) {
                return rc;
            }
            rc = capture_float(tx->mp, capture, 0, value);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }
    else {
        return IB_EINVAL;
    }

    return IB_OK;
}

/**
 * Execute function for the numeric "equal" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns
 *   - IB_OK Success.
 *   - IB_EINVAL Invalid conversion, such as float, or invalid input field.
 *   - IB_EOTHER Unexpected internal error.
 */
static
ib_status_t op_eq_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;

    rc = execute_compare(
        tx,
        instance_data,
        field,
        capture,
        &num_eq,
        &float_eq,
        result
    );

    return rc;
}

/**
 * Execute function for the numeric "not equal" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.

 *
 * @returns Status code
 */
static
ib_status_t op_ne_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;

    rc = execute_compare(
        tx,
        instance_data,
        field,
        capture,
        &num_ne,
        &float_ne,
        result
    );

    return rc;
}

/**
 * Execute function for the "gt" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static
ib_status_t op_gt_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;

    rc = execute_compare(
        tx,
        instance_data,
        field,
        capture,
        &num_gt,
        &float_gt,
        result
    );

    return rc;
}

/**
 * Execute function for the numeric "less-than" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.

 *
 * @returns Status code
 */
static
ib_status_t op_lt_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;

    rc = execute_compare(
        tx,
        instance_data,
        field,
        capture,
        &num_lt,
        &float_lt,
        result
    );

    return rc;
}

/**
 * Execute function for the numeric "greater than or equal to" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static
ib_status_t op_ge_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;

    rc = execute_compare(
        tx,
        instance_data,
        field,
        capture,
        &num_ge,
        &float_ge,
        result
    );

    return rc;
}

/**
 * Execute function for the "less than or equal to" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static
ib_status_t op_le_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;

    rc = execute_compare(
        tx,
        instance_data,
        field,
        capture,
        &num_le,
        &float_le,
        result
    );

    return rc;
}

/**
 * Create function for the numeric comparison operators
 *
 * @param[in]  ctx           The current IronBee context (unused).
 * @param[in]  parameters    Parameters.
 * @param[out] instance_data Instance_data.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static
ib_status_t op_numcmp_create(
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    ib_field_t *f;
    ib_status_t rc;
    ib_num_t num_value;
    ib_float_t float_value;

    char *params_unesc;
    size_t params_unesc_len;

    ib_engine_t *ib = ib_context_get_engine(ctx);
    ib_mpool_t *mp = ib_context_get_mpool(ctx);
    assert(ib != NULL);
    assert(mp != NULL);

    rc = unescape_op_args(ib, mp, &params_unesc, &params_unesc_len, parameters);
    if (rc != IB_OK) {
        ib_log_debug(ib, "Unable to unescape parameter: %s", parameters);
        return rc;
    }

    if (params_unesc == NULL) {
        return IB_EINVAL;
    }

    numop_instance_data_t *data =
        (numop_instance_data_t *)ib_mpool_alloc(mp, sizeof(*data));
    if (data == NULL) {
        return IB_EALLOC;
    }
    data->expand = NULL;
    data->f = NULL;
    *(numop_instance_data_t **)instance_data = data;

    /* Is the string expandable? */
    if (ib_var_expand_test(params_unesc, params_unesc_len)) {
        // @todo Catch error message and offset and report or log.
        ib_var_expand_t *tmp;
        rc = ib_var_expand_acquire(
            &tmp,
            mp,
            params_unesc, params_unesc_len,
            ib_engine_var_config_get(ib),
            NULL, NULL
        );
        data->expand = tmp;
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        ib_status_t num_rc;
        ib_status_t float_rc = IB_EINVAL;
        num_rc = ib_string_to_num_ex(params_unesc,
                                     params_unesc_len,
                                     0, &num_value);
        if (num_rc != IB_OK) {
            float_rc = ib_string_to_float_ex(params_unesc,
                                             params_unesc_len,
                                             &float_value);
        }

        /* If it's a valid int, all good, use it */
        if (num_rc == IB_OK) {
            rc = ib_field_create(
                &f,
                mp,
                IB_FIELD_NAME("param"),
                IB_FTYPE_NUM,
                ib_ftype_num_in(&num_value));
        }
        /* If it's a valid float, don't use it for eq and ne operators */
        else if (float_rc == IB_OK) {
            if ( (cbdata == op_eq_execute) ||
                 (cbdata == op_ne_execute) )
            {
                return IB_EINVAL;
            }
            else {
                rc = ib_field_create(
                    &f,
                    mp,
                    IB_FIELD_NAME("param"),
                    IB_FTYPE_FLOAT,
                    ib_ftype_float_in(&float_value));
            }
        }
        else {
            return IB_EINVAL;
        }

        if (rc == IB_OK) {
            data->f = f;
        }
    }

    return rc;
}

/**
 * Execute function for the "nop" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static
ib_status_t op_nop_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    ib_status_t rc;

    *result = 1;

    if (capture != NULL) {
        rc = ib_capture_clear(capture);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_capture_set_item(capture, 0, tx->mp, field);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Initialize the core operators
 **/
ib_status_t ib_core_operators_init(ib_engine_t *ib, ib_module_t *mod)
{
    ib_status_t rc;

    /*
     * String comparison operators
     */

    /* Register the string equal operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "streq",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        strop_create, NULL,
        NULL, NULL,
        op_streq_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the string equal, case-insensitive, operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "istreq",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        strop_create, NULL,
        NULL, NULL,
        op_streq_execute, (void *)1
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the string contains operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "contains",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        strop_create, NULL,
        NULL, NULL,
        op_contains_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the string match operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "match",
        IB_OP_CAPABILITY_NON_STREAM,
        op_match_create, NULL,
        NULL, NULL,
        op_match_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the case insensitive string match operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "imatch",
        IB_OP_CAPABILITY_NON_STREAM,
        op_match_create, (void *)1,
        NULL, NULL,
        op_match_execute, /* Note: same as above. */ NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the ipmatch operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "ipmatch",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_ipmatch_create, NULL,
        NULL, NULL,
        op_ipmatch_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the ipmatch6 operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "ipmatch6",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_ipmatch6_create, NULL,
        NULL, NULL,
        op_ipmatch6_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * Numeric comparison operators
     */

    /* Register the numeric equal operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "eq",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_numcmp_create, op_eq_execute,
        NULL, NULL,
        op_eq_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric not-equal operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "ne",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_numcmp_create, op_ne_execute,
        NULL, NULL,
        op_ne_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric greater-than operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "gt",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_numcmp_create, op_gt_execute,
        NULL, NULL,
        op_gt_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric less-than operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "lt",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_numcmp_create, op_lt_execute,
        NULL, NULL,
        op_lt_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric greater-than or equal to operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "ge",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_numcmp_create, op_ge_execute,
        NULL, NULL,
        op_ge_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric less-than or equal to operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "le",
        IB_OP_CAPABILITY_NON_STREAM | IB_OP_CAPABILITY_CAPTURE,
        op_numcmp_create, op_le_execute,
        NULL, NULL,
        op_le_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register NOP operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "nop",
        ( IB_OP_CAPABILITY_ALLOW_NULL |
          IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_STREAM |
          IB_OP_CAPABILITY_CAPTURE ),
        NULL, NULL, /* No create function */
        NULL, NULL, /* no destroy function */
        op_nop_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}
