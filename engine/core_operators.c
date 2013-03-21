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
#include <ironbee/engine.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/ipset.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/rule_capture.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>

/**
 * Perform a comparison of two inputs and store the boolean result in @result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
typedef ib_status_t (*num_compare_fn_t)(ib_num_t n1, ib_num_t n2, ib_num_t *result);

/**
 * Perform a comparison of two inputs and store the boolean result in @result.
 * @param[in] n1 Input number 1.
 * @param[in] n2 Input number 2.
 * @param[out] result The result.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If the operation is not supported for the input types.
 */
typedef ib_status_t (*float_compare_fn_t)(ib_float_t n1, ib_float_t n2, ib_num_t *result);

/**
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
 * Perform a comparison of two inputs and store the boolean result in @result.
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
        ib_log_debug(ib, "Failed to allocate unescape string buffer.");
        return IB_EALLOC;
    }

    rc = ib_util_unescape_string(tmp_unesc,
                                 &tmp_unesc_len,
                                 str,
                                 str_len,
                                 IB_UTIL_UNESCAPE_NULTERMINATE);

    if ( rc != IB_OK ) {
        ib_log_debug(ib, "Failed to unescape string: %s", str);
        return rc;
    }

    /* Commit changes on success. */
    *str_unesc = tmp_unesc;
    *str_unesc_len = tmp_unesc_len;

    return IB_OK;
}

/**
 * Create function for the "str" family of operators
 *
 * @param[in] ib The IronBee engine (unused)
 * @param[in] ctx The current IronBee context (unused)
 * @param[in] rule Parent rule to the operator
 * @param[in,out] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters
 * @param[in,out] op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t strop_create(ib_engine_t *ib,
                                ib_context_t *ctx,
                                const ib_rule_t *rule,
                                ib_mpool_t *mp,
                                const char *parameters,
                                ib_operator_inst_t *op_inst)
{
    ib_status_t rc;
    bool expand;
    char *str;
    size_t str_len;

    if (parameters == NULL) {
        ib_log_error(ib, "Missing parameter for operator %s",
                     op_inst->op->name);
        return IB_EINVAL;
    }

    rc = unescape_op_args(ib, mp, &str, &str_len, parameters);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_data_expand_test_str(str, &expand);
    if (rc != IB_OK) {
        return rc;
    }
    if (expand) {
        op_inst->flags |= IB_OPINST_FLAG_EXPAND;
    }

    op_inst->data = str;
    return IB_OK;
}

/**
 * Execute function for the "streq" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_streq_execute(const ib_rule_exec_t *rule_exec,
                                    void *data,
                                    ib_flags_t flags,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    assert(rule_exec != NULL);
    assert(data != NULL);
    assert(field != NULL);
    assert(result != NULL);

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
    ib_status_t  rc;
    const char  *cstr = (const char *)data;
    char        *expanded;
    ib_tx_t     *tx = rule_exec->tx;
    bool         case_insensitive;

    case_insensitive = (rule_exec->rule->opinst->op->cd_execute != NULL);

    /* Expand the string */
    if ( (tx != NULL) && ( (flags & IB_OPINST_FLAG_EXPAND) != 0) ) {
        rc = ib_data_expand_str(tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        expanded = (char *)cstr;
    }

    /* Handle NUL-terminated strings and byte strings */
    if (field->type == IB_FTYPE_NULSTR) {
        const char *fval;
        rc = ib_field_value(field, ib_ftype_nulstr_out(&fval));
        if (rc != IB_OK) {
            return rc;
        }

        if (case_insensitive) {
            *result = (strcasecmp(fval, expanded) == 0);
        }
        else {
            *result = (strcmp(fval, expanded) == 0);
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *value;
        size_t                len;

        rc = ib_field_value(field, ib_ftype_bytestr_out(&value));
        if (rc != IB_OK) {
            return rc;
        }

        len = ib_bytestr_length(value);

        if (len == strlen(expanded)) {
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

    if (ib_rule_should_capture(rule_exec, *result)) {
        ib_rule_capture_clear(rule_exec);
        ib_rule_capture_set_item(rule_exec, 0, field);
    }

    return IB_OK;
}

/**
 * Execute function for the "contains" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_contains_execute(const ib_rule_exec_t *rule_exec,
                                       void *data,
                                       ib_flags_t flags,
                                       ib_field_t *field,
                                       ib_num_t *result)
{
    assert(rule_exec != NULL);
    assert(data != NULL);
    assert(field != NULL);
    assert(result != NULL);

    ib_status_t  rc = IB_OK;
    const char  *cstr = (char *)data;
    char        *expanded;
    ib_tx_t     *tx = rule_exec->tx;

    /* Expand the string */
    if ( (tx != NULL) && ( (flags & IB_OPINST_FLAG_EXPAND) != 0) ) {
        rc = ib_data_expand_str(tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        expanded = (char *)cstr;
    }

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
    if (field->type == IB_FTYPE_NULSTR) {
        const char *s;
        rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            return rc;
        }

        if (strstr(s, expanded) == NULL) {
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

        if (ib_bytestr_index_of_c(str, expanded) == -1) {
            *result = 0;
        }
        else {
            *result = 1;
        }
    }
    else {
        return IB_EINVAL;
    }

    if ( (tx != NULL) && (ib_rule_should_capture(rule_exec, *result)) ) {
        ib_field_t *f;
        const char *name;

        ib_rule_capture_clear(rule_exec);

        name = ib_rule_capture_name(rule_exec, 0);
        rc = ib_field_create_bytestr_alias(&f, rule_exec->tx->mp,
                                           name, strlen(name),
                                           (uint8_t *)expanded,
                                           strlen(expanded));
        ib_rule_capture_set_item(rule_exec, 0, f);
    }

    return rc;
}


/**
 * Create function for the "match" and "imatch" operators.
 *
 * @param[in] ib         The IronBee engine.
 * @param[in] ctx        The current IronBee context (unused).
 * @param[in] rule       Parent rule to the operator.
 * @param[in] mp         Memory pool to use for allocation.
 * @param[in] parameters Parameters (IPv4 address or networks)
 * @param[in] op_inst    Instance operator.
 *
 * @returns
 * - IB_OK if no failure.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a parameters as space separated list.
 */
static
ib_status_t op_match_create(
    ib_engine_t        *ib,
    ib_context_t       *ctx,
    const ib_rule_t    *rule,
    ib_mpool_t         *mp,
    const char         *parameters,
    ib_operator_inst_t *op_inst
)
{
    assert(ib      != NULL);
    assert(ctx     != NULL);
    assert(rule    != NULL);
    assert(mp      != NULL);
    assert(op_inst != NULL);

    ib_status_t  rc;
    ib_hash_t   *set;
    bool         case_insensitive;
    char        *copy;
    size_t       copy_len;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    case_insensitive = (op_inst->op->cd_create != NULL);

    /* Make a copy of the parameters to operate on. */
    rc = unescape_op_args(ib, mp, &copy, &copy_len, parameters);
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error unescaping rule parameters '%s'", parameters
        );
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
    op_inst->data = set;

    return IB_OK;
}

/**
 * Execute function for the "match" and "imatch" operators.
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data      Set data.
 * @param[in] flags     Operator instance flags.
 * @param[in] field     Field value.
 * @param[out] result   Pointer to number in which to store the result.
 *
 * @returns
 * - IB_OK if no failure, regardless of match status.
 * - IB_EUNKNOWN on unexpected field error.
 * - IB_EINVAL on incompatible field, i.e., other than string or bytestring.
 * - IB_EALLOC on allocation failure.
 */
static
ib_status_t op_match_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    ib_field_t           *field,
    ib_num_t             *result
)
{
    assert(rule_exec != NULL);
    assert(data      != NULL);
    assert(field     != NULL);
    assert(result    != NULL);

    ib_status_t        rc;
    const ib_hash_t   *set;
    const char        *s;
    size_t             length;
    void              *v;
    ib_tx_t           *tx;

    set = (const ib_hash_t *)data;
    tx = rule_exec->tx;

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

    return IB_OK;
}

/**
 * Create function for the "ipmatch" operator
 *
 * @param[in] ib         The IronBee engine.
 * @param[in] ctx        The current IronBee context (unused).
 * @param[in] rule       Parent rule to the operator.
 * @param[in] mp         Memory pool to use for allocation.
 * @param[in] parameters Parameters (IPv4 address or networks)
 * @param[in] op_inst    Instance operator.
 *
 * @returns
 * - IB_OK if no failure.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a parameters as IP addresses or networks.
 */
static
ib_status_t op_ipmatch_create(
    ib_engine_t        *ib,
    ib_context_t       *ctx,
    const ib_rule_t    *rule,
    ib_mpool_t         *mp,
    const char         *parameters,
    ib_operator_inst_t *op_inst
)
{
    assert(ib      != NULL);
    assert(ctx     != NULL);
    assert(rule    != NULL);
    assert(mp      != NULL);
    assert(op_inst != NULL);

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
    op_inst->data = ipset;

    return IB_OK;
}

/**
 * Execute function for the "ipmatch" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data      IP Set data.
 * @param[in] flags     Operator instance flags.
 * @param[in] field     Field value.
 * @param[out] result   Pointer to number in which to store the result.
 *
 * @returns
 * - IB_OK if no failure, regardless of match status.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a field as IP address.
 */
static
ib_status_t op_ipmatch_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    ib_field_t           *field,
    ib_num_t             *result
)
{
    assert(rule_exec != NULL);
    assert(data      != NULL);
    assert(field     != NULL);
    assert(result    != NULL);

    ib_status_t        rc               = IB_OK;
    const ib_ipset4_t *ipset            = NULL;
    ib_ip4_t           ip               = 0;
    const char        *ipstr            = NULL;
    char               ipstr_buffer[17] = "\0";
    ib_tx_t           *tx               = rule_exec->tx;

    ipset = (const ib_ipset4_t *)data;

    if (field->type == IB_FTYPE_NULSTR) {
        rc = ib_field_value(field, ib_ftype_nulstr_out(&ipstr));
        if (rc != IB_OK) {
            return rc;
        }

        if (ipstr == NULL) {
            ib_log_error_tx(tx, "Failed to get NULSTR from field");
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
        assert(ib_bytestr_length(bs) < 17);

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
    else if (rc == IB_OK) {
        *result = 1;
        if (ib_rule_should_capture(rule_exec, *result)) {
            ib_rule_capture_clear(rule_exec);
            ib_rule_capture_set_item(rule_exec, 0, field);
        }
    }
    else {
        ib_rule_log_error(rule_exec,
                          "Error searching set for ip %s: %s",
                          ipstr, ib_status_to_string(rc)
        );
        return rc;
    }
    return IB_OK;
}


/**
 * Create function for the "ipmatch6" operator
 *
 * @param[in] ib         The IronBee engine.
 * @param[in] ctx        The current IronBee context (unused).
 * @param[in] rule       Parent rule to the operator.
 * @param[in] mp         Memory pool to use for allocation.
 * @param[in] parameters Parameters (IPv6 address or networks)
 * @param[in] op_inst    Instance operator.
 *
 * @returns
 * - IB_OK if no failure.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a parameters as IP addresses or networks.
 */
static
ib_status_t op_ipmatch6_create(
    ib_engine_t        *ib,
    ib_context_t       *ctx,
    const ib_rule_t    *rule,
    ib_mpool_t         *mp,
    const char         *parameters,
    ib_operator_inst_t *op_inst
)
{
    assert(ib      != NULL);
    assert(ctx     != NULL);
    assert(rule    != NULL);
    assert(mp      != NULL);
    assert(op_inst != NULL);

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
    op_inst->data = ipset;

    return IB_OK;
}

/**
 * Execute function for the "ipmatch6" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data      IP Set data.
 * @param[in] flags     Operator instance flags.
 * @param[in] field     Field value.
 * @param[out] result   Pointer to number in which to store the result.
 *
 * @returns
 * - IB_OK if no failure, regardless of match status.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL on unable to parse @a field as IP address.
 */
static
ib_status_t op_ipmatch6_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    ib_field_t           *field,
    ib_num_t             *result
)
{
    assert(rule_exec != NULL);
    assert(data      != NULL);
    assert(field     != NULL);
    assert(result    != NULL);

    ib_status_t        rc               = IB_OK;
    const ib_ipset6_t *ipset            = NULL;
    ib_ip6_t           ip               = {{0, 0, 0, 0}};
    const char        *ipstr            = NULL;
    char               ipstr_buffer[41] = "\0";
    ib_tx_t           *tx               = rule_exec->tx;

    ipset = (const ib_ipset6_t *)data;

    if (field->type == IB_FTYPE_NULSTR) {
        rc = ib_field_value(field, ib_ftype_nulstr_out(&ipstr));
        if (rc != IB_OK) {
            return rc;
        }

        if (ipstr == NULL) {
            ib_log_error_tx(tx, "Failed to get NULSTR from field");
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
    else if (rc == IB_OK) {
        *result = 1;
        if (ib_rule_should_capture(rule_exec, *result)) {
            ib_rule_capture_clear(rule_exec);
            ib_rule_capture_set_item(rule_exec, 0, field);
        }
    }
    else {
        ib_rule_log_error(rule_exec,
                          "Error searching set for ip %s: %s",
                          ipstr, ib_status_to_string(rc)
        );
        return rc;
    }
    return IB_OK;
}

/**
 * Convert @a in_field from a string by expanding it to an expanded
 * string and then convert that string to a number-type if
 * possible. Otherwise, leave it as a string.
 *
 * If no conversion is performed because none is necessary,
 * IB_OK is returned and @a out_field is set to NULL.
 *
 * @param[in] rule_exec Rule execution.
 * @param[in] flags Execution flags. If IB_OPINST_FLAG_EXPAND is not set,
 *            then no expansion will be attempted.
 * @param[in] in_field The field to expand and attempt to convert.
 * @param[out] out_field The field that the in_field is converted to.
 *             If no conversion is performed, this is set to NULL.
 * @returns
 *   - IB_OK On success.
 *   - IB_EALLOC On memory failure.
 *   - IB_EINVAL If an expandable value cannot be expanded.
 *   - Other returned by ib_data_expand_str.
 */
static ib_status_t expand_field(
    const ib_rule_exec_t *rule_exec,
    const ib_flags_t flags,
    const ib_field_t *in_field,
    ib_field_t **out_field)
{
    assert(rule_exec);
    assert(rule_exec->tx);
    assert(rule_exec->tx->mp);
    assert(in_field);

    const char *original;
    char *expanded;
    ib_field_t *tmp_field;
    ib_status_t rc;

    /* No conversion required. */
    if ( ! (flags & IB_OPINST_FLAG_EXPAND) ) {
        *out_field = NULL;
        return IB_OK;
    }

    /* Get the string from the field */
    rc = ib_field_value(in_field, ib_ftype_nulstr_out(&original));
    if (rc != IB_OK) {
        return rc;
    }

    /* Expand the string */
    rc = ib_data_expand_str(rule_exec->tx->data, original, false, &expanded);
    if (rc != IB_OK) {
        return rc;
    }

    /* Wrap the string into a field and set it to the tmp_field.
     * We will not try to expand tmp_field into a number. If we
     * fail, we return tmp_field in *out_field. */
    rc = ib_field_create_alias(
        &tmp_field,
        rule_exec->tx->mp,
        in_field->name,
        in_field->nlen,
        IB_FTYPE_NULSTR,
        ib_ftype_nulstr_in(expanded));
    if (rc != IB_OK) {
        return rc;
    }

    /* Attempt a num. */
    rc = ib_field_convert(
        rule_exec->tx->mp,
        IB_FTYPE_NUM,
        tmp_field,
        out_field);
    if (rc == IB_OK) {
        return IB_OK;
    }

    /* Attempt a float. */
    rc = ib_field_convert(
        rule_exec->tx->mp,
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
 * @param[in] rule_exec Rule execution object
 * @param[in] capture The capture number
 * @param[in] value The actual value
 */
static ib_status_t capture_float(const ib_rule_exec_t *rule_exec,
                                 int capture,
                                 ib_float_t value)
{
    assert(rule_exec != NULL);

    ib_status_t rc;
    ib_field_t *field;
    const char *name;
    const char *str;

    name = ib_rule_capture_name(rule_exec, capture);

    str = ib_float_to_string(rule_exec->tx->mp, value);
    if (str == NULL) {
        return IB_EALLOC;
    }
    rc = ib_field_create_bytestr_alias(&field, rule_exec->tx->mp,
                                       name, strlen(name),
                                       (uint8_t *)str, strlen(str));
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_rule_capture_set_item(rule_exec, 0, field);
    return rc;
}

/**
 * Store a number in the capture buffer
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] capture The capture number
 * @param[in] value The actual value
 */
static ib_status_t capture_num(const ib_rule_exec_t *rule_exec,
                               int capture,
                               ib_num_t value)
{
    assert(rule_exec != NULL);

    ib_status_t rc;
    ib_field_t *field;
    const char *name;
    const char *str;

    name = ib_rule_capture_name(rule_exec, capture);

    str = ib_num_to_string(rule_exec->tx->mp, value);
    if (str == NULL) {
        return IB_EALLOC;
    }
    rc = ib_field_create_bytestr_alias(&field, rule_exec->tx->mp,
                                       name, strlen(name),
                                       (uint8_t *)str, strlen(str));
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_rule_capture_set_item(rule_exec, 0, field);
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
 *   - Expands the rh_in field.
 *   - Finds a target type that rh_in and lh_in must be converted to.
 *   - If a float is disallowed, this applies that check and returns IB_EINVAL.
 *   - Both fields are converted and put into rh_out and lh_out.
 *     NOTE: If conversion is NOT necessary, then the original
 *           rh_in or lh_in will be placed in the output variables.
 *           This will strip the const-ness of the value.
 *           This is not relevant to how math operations
 *           use these fields, so user-beware.
 *
 * @param rule_exec The rule execution environment.
 * @param flags Rule flags used for @a rh_in expansion.
 * @param lh_in Left-hand operand input.
 * @param rh_in Right-hand operand input.
 * @param lh_out Left-hand operand out. This may equal @a lh_in.
 * @param rh_out Right-hand operand out. This may equal @a rh_in.
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If a type cannot be converted.
 */
static ib_status_t prepare_math_operands(
    const ib_rule_exec_t *rule_exec,
    const ib_flags_t flags,
    const ib_field_t *lh_in,
    const ib_field_t *rh_in,
    ib_field_t **lh_out,
    ib_field_t **rh_out)
{
    assert(rule_exec);
    assert(rule_exec->tx);
    assert(rule_exec->tx->mp);
    assert(lh_in);
    assert(rh_in);
    assert(lh_out);
    assert(rh_out);

    ib_ftype_t type = 0;
    ib_status_t rc;

    /* An intermediate holding place for produced fields. */
    ib_field_t *tmp_field = NULL;

    /* First, expand the right hand input. */
    rc = expand_field(rule_exec, flags, rh_in, &tmp_field);
    if (rc != IB_OK) {
        return rc;
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
    rc = ib_field_convert(rule_exec->tx->mp, type, *rh_out, &tmp_field);
    if (rc != IB_OK){
        *rh_out = NULL;
        *lh_out = NULL;
        return rc;
    }
    else if (tmp_field) {
        *rh_out = tmp_field;
    }

    /* Convert lh_field. */
    rc = ib_field_convert(rule_exec->tx->mp, type, lh_in, &tmp_field);
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
 * param[in] rule_exec Rule execution.
 * param[in] data Parameter field.
 * param[in] flags Flags to influence @a data expansion.
 * param[in] field The field used.
 * param[in] num_compare If this is an ib_num_t, use this to compare.
 * param[in] float_compare If this is an ib_float_t, use this to compare.
 * param[out] result The result is store here.
 */
static ib_status_t execute_compare(
    const ib_rule_exec_t *rule_exec,
    void *data,
    ib_flags_t flags,
    ib_field_t *field,
    num_compare_fn_t num_compare,
    float_compare_fn_t float_compare,
    ib_num_t *result)
{
    assert(data);
    assert(result);
    assert(rule_exec);
    assert(rule_exec->tx);
    assert(rule_exec->tx->mp);

    const ib_field_t *pdata = (const ib_field_t *)data;
    ib_status_t rc;
    ib_field_t *rh_field = NULL;
    ib_field_t *lh_field = NULL;

    rc = prepare_math_operands(
        rule_exec,
        flags,
        field,
        pdata,
        &lh_field,
        &rh_field);
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
        if (ib_rule_should_capture(rule_exec, *result)) {
            ib_rule_capture_clear(rule_exec);
            rc = capture_num(rule_exec, 0, value);
            if (rc != IB_OK) {
                ib_rule_log_error(rule_exec, "Error storing capture #0: %s",
                                  ib_status_to_string(rc));
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
        if (ib_rule_should_capture(rule_exec, *result)) {
            ib_rule_capture_clear(rule_exec);
            rc = capture_float(rule_exec, 0, value);
            if (rc != IB_OK) {
                ib_rule_log_error(rule_exec, "Error storing capture #0: %s",
                                  ib_status_to_string(rc));
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
 * @param[in] rule_exec Rule execution object
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns
 *   - IB_OK Success.
 *   - IB_EINVAL Invalid conversion, such as float, or invalid input field.
 *   - IB_EOTHER Unexpected internal error.
 */
static ib_status_t op_eq_execute(const ib_rule_exec_t *rule_exec,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    ib_status_t rc;

    rc = execute_compare(
        rule_exec,
        data,
        flags,
        field,
        &num_eq,
        &float_eq,
        result);

    return rc;
}

/**
 * Execute function for the numeric "not equal" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_ne_execute(const ib_rule_exec_t *rule_exec,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    ib_status_t rc;

    rc = execute_compare(
        rule_exec,
        data,
        flags,
        field,
        &num_ne,
        &float_ne,
        result);

    return rc;
}

/**
 * Execute function for the "gt" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_gt_execute(const ib_rule_exec_t *rule_exec,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    ib_status_t rc;

    rc = execute_compare(
        rule_exec,
        data,
        flags,
        field,
        &num_gt,
        &float_gt,
        result);

    return rc;
}

/**
 * Execute function for the numeric "less-than" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_lt_execute(const ib_rule_exec_t *rule_exec,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    ib_status_t rc;

    rc = execute_compare(
        rule_exec,
        data,
        flags,
        field,
        &num_lt,
        &float_lt,
        result);

    return rc;
}

/**
 * Execute function for the numeric "greater than or equal to" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_ge_execute(const ib_rule_exec_t *rule_exec,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    ib_status_t rc;

    rc = execute_compare(
        rule_exec,
        data,
        flags,
        field,
        &num_ge,
        &float_ge,
        result);

    return rc;
}

/**
 * Execute function for the "less than or equal to" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_le_execute(const ib_rule_exec_t *rule_exec,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    ib_status_t rc;

    rc = execute_compare(
        rule_exec,
        data,
        flags,
        field,
        &num_le,
        &float_le,
        result);

    return rc;
}

/**
 * Create function for the numeric comparison operators
 *
 * @param[in] ib The IronBee engine (unused)
 * @param[in] ctx The current IronBee context (unused)
 * @param[in] rule Parent rule to the operator
 * @param[in,out] mp Memory pool to use for allocation
 * @param[in] params Constant parameters
 * @param[in,out] op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t op_numcmp_create(ib_engine_t *ib,
                                    ib_context_t *ctx,
                                    const ib_rule_t *rule,
                                    ib_mpool_t *mp,
                                    const char *params,
                                    ib_operator_inst_t *op_inst)
{
    ib_field_t *f;
    ib_status_t rc;
    bool expandable;
    ib_num_t num_value;
    ib_float_t float_value;

    char *params_unesc;
    size_t params_unesc_len;

    rc = unescape_op_args(ib, mp, &params_unesc, &params_unesc_len, params);
    if (rc != IB_OK) {
        ib_log_debug(ib, "Unable to unescape parameter: %s", params);
        return rc;
    }

    if (params_unesc == NULL) {
        return IB_EINVAL;
    }

    /* Is the string expandable? */
    rc = ib_data_expand_test_str(params_unesc, &expandable);
    if (rc != IB_OK) {
        return rc;
    }
    if (expandable) {
        op_inst->flags |= IB_OPINST_FLAG_EXPAND;

        rc = ib_field_create(&f, mp, IB_FIELD_NAME("param"),
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(params_unesc));
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
            if ( (op_inst->op->fn_execute == op_eq_execute) ||
                 (op_inst->op->fn_execute == op_ne_execute) )
            {
                ib_log_error(ib,
                             "Floating point parameter \"%s\" "
                             "is not supported for operator \"%s\"",
                             params_unesc, op_inst->op->name);
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
            ib_log_error(ib,
                         "Parameter \"%s\" for operator \"%s\" "
                         "is not a valid number",
                         params_unesc, op_inst->op->name);
            return IB_EINVAL;
        }
    }

    if (rc == IB_OK) {
        op_inst->data = f;
        op_inst->fparam = f;
    }

    return rc;
}

/**
 * Execute function for the "nop" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code (IB_OK)
 */
static ib_status_t op_nop_execute(const ib_rule_exec_t *rule_exec,
                                  void *data,
                                  ib_flags_t flags,
                                  ib_field_t *field,
                                  ib_num_t *result)
{
    *result = 1;

    if (ib_rule_should_capture(rule_exec, *result)) {
        ib_rule_capture_clear(rule_exec);
        ib_rule_capture_set_item(rule_exec, 0, field);
    }
    return IB_OK;
}

/**
 * Initialize the core operators
 **/
ib_status_t ib_core_operators_init(ib_engine_t *ib, ib_module_t *mod)
{
    ib_status_t rc;


    /**
     * String comparison operators
     */

    /* Register the string equal operator */
    rc = ib_operator_register(ib,
                              "streq",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              strop_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_streq_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the string equal, case-insensitive, operator */
    rc = ib_operator_register(ib,
                              "istreq",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              strop_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_streq_execute,
                              (void *)1);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the string contains operator */
    rc = ib_operator_register(ib,
                              "contains",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              strop_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_contains_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the string match operator */
    rc = ib_operator_register(ib,
                              "match",
                              IB_OP_FLAG_PHASE,
                              op_match_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_match_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the case insensitive string match operator */
    rc = ib_operator_register(ib,
                              "imatch",
                              IB_OP_FLAG_PHASE,
                              op_match_create,
                              (void *)1,
                              NULL, /* no destroy function */
                              NULL,
                              op_match_execute, /* Note: same as above. */
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the ipmatch operator */
    rc = ib_operator_register(ib,
                              "ipmatch",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_ipmatch_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_ipmatch_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the ipmatch6 operator */
    rc = ib_operator_register(ib,
                              "ipmatch6",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_ipmatch6_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_ipmatch6_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * Numeric comparison operators
     */

    /* Register the numeric equal operator */
    rc = ib_operator_register(ib,
                              "eq",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_eq_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric not-equal operator */
    rc = ib_operator_register(ib,
                              "ne",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_ne_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric greater-than operator */
    rc = ib_operator_register(ib,
                              "gt",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_gt_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric less-than operator */
    rc = ib_operator_register(ib,
                              "lt",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_lt_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric greater-than or equal to operator */
    rc = ib_operator_register(ib,
                              "ge",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_ge_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the numeric less-than or equal to operator */
    rc = ib_operator_register(ib,
                              "le",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_le_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register NOP operator */
    rc = ib_operator_register(ib,
                              "nop",
                              ( IB_OP_FLAG_ALLOW_NULL |
                                IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM |
                                IB_OP_FLAG_CAPTURE ),
                              NULL, NULL, /* No create function */
                              NULL, NULL, /* no destroy function */
                              op_nop_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }


    return IB_OK;
}
