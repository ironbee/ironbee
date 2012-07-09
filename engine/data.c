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
 * @brief IronBee &mdash; Data Access
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pcre.h>

#include <ironbee/engine.h>
#include <ironbee/string.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>
#include <ironbee/provider.h>
#include <ironbee/field.h>
#include <ironbee/expand.h>
#include <ironbee/transformation.h>
#include <ironbee/debug.h>
#include <ironbee/util.h>

/* -- Constants -- */
static const char DPI_LIST_FILTER_MARKER = ':';
static const char DPI_LIST_FILTER_PREFIX = '/';
static const char DPI_LIST_FILTER_SUFFIX = '/';

/*
 * Parameters used for variable expansion in rules.
 */
/** Variable prefix */
static const char *IB_VARIABLE_EXPANSION_PREFIX = "%{";
/** Variable postfix */
static const char *IB_VARIABLE_EXPANSION_POSTFIX = "}";

/* Internal helper functions */

/**
 * Get a subfield from @a dpi by @a api.
 *
 * If @a parent_field is a list (IB_FTYPE_LIST) then a case insensitive
 * string comparison is done to find the first list element that matches.
 *
 * If @a parent_field is a dynamic field, then the field @a name
 * is fetched from it and the return code from that operation is returned.
 *
 * @param[in] api The API to perform the get operation.
 * @param[in] dpi The data provider instance passed to a call to a
 *                function available from @a api.
 * @param[in] parent_field The parent field that contains the requested field.
 *                         This must be an IB_FTYPE_LIST.
 * @param[in] name The regex to use to match member field names in
 *                 @a field_name.
 * @param[in] name_len The length of @a pattern.
 * @param[out] result_field The result field.
 *
 * @returns
 *  - IB_OK on success.
 *  - IB_EINVAL The parent field is not a list or a dynamic type. Also
 *              returned if @a name_len is 0.
 *  - Other if a dynamic field fails.
 */
static ib_status_t ib_data_get_subfields(IB_PROVIDER_API_TYPE(data) *api,
                                        ib_provider_inst_t *dpi,
                                        const ib_field_t *parent_field,
                                        const char *name,
                                        size_t name_len,
                                        ib_field_t **result_field)
{
    IB_FTRACE_INIT();

    assert(api);
    assert(dpi);
    assert(parent_field);
    assert(name);
    assert(result_field);

    ib_status_t rc;
    ib_list_t *list; /* List of values to check stored in parent_field. */
    ib_list_node_t *list_node; /* List node in list. */

    if( name_len == 0 ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Pull a value from a dynamic field. */
    if(ib_field_is_dynamic(parent_field)) {
        rc = ib_field_value_ex(parent_field,
                               result_field,
                               name,
                               name_len);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Check that our input field is a list type. */
    else if (parent_field->type == IB_FTYPE_LIST) {
        ib_list_t *result_list;

        /* Make the result list */
        rc = ib_list_create(&result_list, dpi->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Fetch the parent list. */
        rc = ib_field_value(parent_field, &list);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug(dpi->pr->ib, "Iterating over list of size %zd.",
                     IB_LIST_ELEMENTS(list));

        IB_LIST_LOOP(list, list_node) {
            ib_field_t *list_field =
                (ib_field_t *) IB_LIST_NODE_DATA(list_node);

            if (list_field->nlen == name_len &&
                strncasecmp(list_field->name, name, name_len) == 0)
            {
                rc = ib_list_push(result_list, list_field);
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }
            }
        }

        /* Send back the result_list inside of result_field. */
        rc = ib_field_create(result_field,
                             dpi->mp,
                             name,
                             name_len,
                             IB_FTYPE_LIST,
                             result_list);

        IB_FTRACE_RET_STATUS(rc);
    }

    /* We don't know what input type this is. Return IB_EINVAL. */
    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

/**
 * Return a list of fields whose name matches @a pattern.
 *
 * The list @a field_name is retrieved from the @a dpi using @a api. Its
 * members are iterated through and the names of those fields compared
 * against @a pattern. If the name matches, the field is added to an
 * @c ib_list_t* which will be returned via @a result_field.
 *
 * @param[in] api The API to perform the get operation.
 * @param[in] dpi The data provider instance passed to a call to a
 *                function available from @a api.
 * @param[in] parent_field The parent field whose member fields will
 *                         be filtered with @a pattern.
 *                         This must be an IB_FTYPE_LIST.
 * @param[in] pattern The regex to use to match member field names in
 *                    @a field_name.
 * @param[in] pattern_len The length of @a pattern.
 * @param[out] result_field The result field.
 *
 * @returns
 *  - IB_OK if a successful search is performed.
 *  - IB_EINVAL if field is not a list or the pattern cannot compile.
 *  - IB_ENOENT if the field name is not found.
 */
static ib_status_t ib_data_get_filtered_list(IB_PROVIDER_API_TYPE(data) *api,
                                             ib_provider_inst_t *dpi,
                                             const ib_field_t *parent_field,
                                             const char *pattern,
                                             size_t pattern_len,
                                             ib_field_t **result_field)
{
    IB_FTRACE_INIT();

    assert(api);
    assert(dpi);
    assert(pattern);
    assert(parent_field);
    assert(pattern_len>0);
    assert(result_field);

    ib_status_t rc;
    char *pattern_str = NULL; /* NULL terminated string to pass to pcre. */
    pcre *pcre_pattern = NULL; /* PCRE pattern. */
    const char *errptr = NULL; /* PCRE Error reporter. */
    int erroffset; /* PCRE Error offset into subject reporter. */
    ib_list_t *list = NULL; /* Holds the value of field when fetched. */
    ib_list_node_t *list_node = NULL; /* A node in list. */
    ib_list_t *result_list = NULL; /* Holds matched list_node values. */

    /* Check that our input field is a list type. */
    if (parent_field->type != IB_FTYPE_LIST) {
        rc = IB_EINVAL;
        goto exit_label;
    }

    /* Allocate pattern_str to hold null terminated string. */
    pattern_str = (char *)malloc(pattern_len+1);
    if (pattern_str == NULL) {
        rc = IB_EALLOC;
        goto exit_label;
    }

    /* Build a string to hand to the pcre library. */
    memcpy(pattern_str, pattern, pattern_len);
    pattern_str[pattern_len] = '\0';

    rc = ib_field_value(parent_field, &list);
    if (rc != IB_OK) {
        goto exit_label;
    }

    pcre_pattern = pcre_compile(pattern_str, 0, &errptr, &erroffset, NULL);
    if (pcre_pattern == NULL) {
        rc = IB_EINVAL;
        goto exit_label;
    }
    if (errptr) {
        rc = IB_EINVAL;
        goto exit_label;
    }

    rc = ib_list_create(&result_list, dpi->mp);
    if (rc != IB_OK) {
        goto exit_label;
    }

    IB_LIST_LOOP(list, list_node) {
        int pcre_rc;
        ib_field_t *list_field = (ib_field_t *)list_node->data;
        pcre_rc = pcre_exec(pcre_pattern,
                            NULL,
                            list_field->name,
                            list_field->nlen,
                            0,
                            0,
                            NULL,
                            0);

        if (pcre_rc == 0) {
            rc = ib_list_push(result_list, list_node->data);
            if (rc != IB_OK) {
                goto exit_label;
            }
        }
    }

    rc = ib_field_create(result_field,
                         dpi->mp,
                         parent_field->name,
                         parent_field->nlen,
                         IB_FTYPE_LIST,
                         result_list);


exit_label:
    if (pattern_str) {
        free(pattern_str);
    }
    if (pcre_pattern) {
        pcre_free(pcre_pattern);
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Add a field to the @a dpi allowing for subfield notation.
 *
 * That is, a field may be stored in a normal field, such as @c FOO.
 * A field may also be stored in a subfield, that is a child field of
 * the list @c FOO. If @a name is @c FOO:BAR then the field @c BAR
 * will be stored in the list @c FOO in the @a dpi.
 *
 * Note that in cases where @a field has a name other than @c BAR,
 * @a field 's name will be set to @c BAR using the @a dpi memory pool
 * for storage.
 *
 * @param[in] api The API to perform the add operation.
 * @param[in] dpi The data provider instance passed to a call to a
 *                function available from @a api.
 * @param[in,out] field The field to add to the DPI. This is an out-parameter
 *                in the case where, first, @a name specifies a subfield
 *                that @a field should be stored under and, second,
 *                @c field->name is different than the subfield.
 * @param[in] name Name of @a field.
 * @param[in] nlen Length of @name.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EINVAL if The parent field exists but is not a list.
 *   - IB_EALLOC on memory allocation errors.
 */
static ib_status_t ib_data_add_internal(IB_PROVIDER_API_TYPE(data) *api,
                                        ib_provider_inst_t *dpi,
                                        ib_field_t *field,
                                        const char *name,
                                        size_t nlen)
{
    IB_FTRACE_INIT();

    assert(api);
    assert(dpi);
    assert(field);
    assert(name);

    ib_status_t rc;
    char *filter_marker;

    if (nlen == 0) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    filter_marker = memchr(name, DPI_LIST_FILTER_MARKER, nlen);

    /* Add using a subfield. */
    if ( filter_marker ) {
        ib_field_t *parent;
        const char *parent_name = name;
        const char *child_name = filter_marker + 1;
        size_t parent_nlen = filter_marker - name;
        size_t child_nlen = nlen - parent_nlen - 1;

        /* Get or create the parent field. */
        rc = api->get(dpi, parent_name, parent_nlen, &parent);
        /* If the field does not exist, make one. */
        if (rc == IB_ENOENT) {

            /* Try to add the list that does not exist. */
            rc = ib_data_add_list_ex(dpi, parent_name, parent_nlen, &parent);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Ensure that the parent field is a list type. */
        if (parent->type != IB_FTYPE_LIST) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* If the child and the field do not have the same name,
         * set the field name to be the name it is stored under. */
        if (memcmp(child_name,
                   field->name,
                   (child_nlen < field->nlen) ? child_nlen : field->nlen))
        {
            field->nlen = child_nlen;
            field->name = ib_mpool_memdup(dpi->mp, child_name, child_nlen);
            if (field->name == NULL) {
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }
        }

        /* If the list already exists, add the value. */
        ib_field_list_add(parent, field);
    }

    /* Normal add. */
    else {
        rc = api->add(dpi, field, name, nlen);
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* -- Exported Data Access Routines -- */

ib_status_t ib_data_add(ib_provider_inst_t *dpi,
                        ib_field_t *f)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_status_t rc;

    rc = ib_data_add_internal(api, dpi, f, f->name, f->nlen);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_named(ib_provider_inst_t *dpi,
                              ib_field_t *f,
                              const char *key,
                              size_t klen)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_status_t rc;

    rc = ib_data_add_internal(api, dpi, f, key, klen);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_num_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               ib_num_t val,
                               ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, name, nlen, IB_FTYPE_NUM, ib_ftype_num_in(&val));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_internal(api, dpi, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_nulstr_ex(ib_provider_inst_t *dpi,
                                  const char *name,
                                  size_t nlen,
                                  const char *val,
                                  ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, name, nlen, IB_FTYPE_NULSTR, ib_ftype_nulstr_in(val));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_internal(api, dpi, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_bytestr_ex(ib_provider_inst_t *dpi,
                                   const char *name,
                                   size_t nlen,
                                   uint8_t *val,
                                   size_t vlen,
                                   ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create_bytestr_alias(&f, dpi->mp, name, nlen, val, vlen);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_internal(api, dpi, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_list_ex(ib_provider_inst_t *dpi,
                                const char *name,
                                size_t nlen,
                                ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, name, nlen, IB_FTYPE_LIST, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_internal(api, dpi, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_stream_ex(ib_provider_inst_t *dpi,
                                  const char *name,
                                  size_t nlen,
                                  ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, name, nlen, IB_FTYPE_SBUFFER, NULL);
    if (rc != IB_OK) {
        ib_util_log_debug("SBUFFER field creation failed: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_internal(api, dpi, f, f->name, f->nlen);
    ib_util_log_debug("SBUFFER field creation returned: %s", ib_status_to_string(rc));
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_get_ex(ib_provider_inst_t *dpi,
                           const char *name,
                           size_t name_len,
                           ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    ib_status_t rc;
    char *name_str = NULL;
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;

    char *filter_marker = memchr(name, DPI_LIST_FILTER_MARKER, name_len);

    /*
     * If there is a filter_marker then we are going to
     * extract sub-values.
     *
     * A sub-value might be a pattern-match on a list: ARGV:/foo\d?/
     * Or a sub field: ARGV:my_var
     * Or a dynamic field: ARGV:my_var
     */
    if ( filter_marker ) {

        /* If there is a filter mark (':') get the parent field. */
        ib_field_t *parent_field;

        char *filter_start = memchr(name, DPI_LIST_FILTER_PREFIX, name_len);
        char *filter_end;

        /* Fetch the field name, but the length is (filter_mark - name).
         * That is, the string before the ':' we found. */
        rc = api->get(dpi, name, filter_marker - name, &parent_field);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        if ( filter_start && filter_start + 1 < name + name_len ) {
            filter_end = memchr(filter_start+1,
                                DPI_LIST_FILTER_SUFFIX,
                                name_len - (filter_start+1-name));
        }
        else {
            filter_end = NULL;
        }


        /* Does the expansions use a pattern match or not? */
        if (filter_start && filter_end) {

            /* Bad filter: FOO/: */
            if (filter_marker != filter_start-1) {
                rc = IB_EINVAL;
                goto error_handler;
            }

            /* Bad filter: FOO:/ */
            if (filter_start == filter_end) {
                rc = IB_EINVAL;
                goto error_handler;
            }

            /* Bad filter: FOO:// */
            if (filter_start == filter_end-1) {
                rc = IB_EINVAL;
                goto error_handler;
            }

            /* Validated that filter_start and filter_end are sane. */
            rc = ib_data_get_filtered_list(api,
                                           dpi,
                                           parent_field,
                                           filter_start+1,
                                           filter_end - filter_start - 1,
                                           pf);
        }

        /* No pattern match. Just extract the sub-field. */
        else {

            /* Handle extracting a subfield for a list of a dynamic field. */
            rc = ib_data_get_subfields(api,
                                      dpi,
                                      parent_field,
                                      filter_marker+1,
                                      name_len - (filter_marker+1-name),
                                      pf);
        }
    }

    /* Typical no-expansion fetch of a value. */
    else {
        rc = api->get(dpi, name, name_len, pf);
    }

    IB_FTRACE_RET_STATUS(rc);

    /* Error handling routine. */
error_handler:
    name_str = malloc(name_len+1);
    if ( name_str == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    memcpy(name_str, name, name_len);
    name_str[name_len] = '\0';
    free(name_str);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_get_all(ib_provider_inst_t *dpi,
                            ib_list_t *list)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_status_t rc;

    rc = api->get_all(dpi, list);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_tfn_get_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               ib_field_t **pf,
                               const char *tfn)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;

    ib_engine_t *ib = dpi->pr->ib;
    char *fullname;
    size_t fnlen;
    size_t tlen;
    ib_status_t rc;

    /* No tfn just means a normal get. */
    if (tfn == NULL) {
        rc = ib_data_get_ex(dpi, name, nlen, pf);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Build the full name with tfn: "name.t(tfn)" */
    tlen = strlen(tfn);
    fnlen = nlen + tlen + 4; /* Additional ".t()" bytes */
    fullname = (char *)ib_mpool_alloc(dpi->mp, fnlen);
    memcpy(fullname, name, nlen);
    memcpy(fullname + nlen, ".t(", fnlen - nlen);
    memcpy(fullname + nlen + 3, tfn, fnlen - nlen - 3);
    fullname[fnlen - 1] = ')';

    /* See if there is already a transformed version, otherwise
     * one needs to be created.
     */
    rc = api->get(dpi, fullname, fnlen, pf);
    if (rc == IB_ENOENT) {
        const char *tname;
        size_t i;

        /* Get the non-tfn field. */
        rc = api->get(dpi, name, nlen, pf);
        if (rc != IB_OK) {
            ib_log_debug(ib, "Failed to fetch field: %p (%s)", *pf, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Currently this only works for string type fields. */
        if (   ((*pf)->type != IB_FTYPE_NULSTR)
            && ((*pf)->type != IB_FTYPE_BYTESTR))
        {
            ib_log_error(ib,
                         "Cannot transform a non-string based field type=%d",
                         (int)(*pf)->type);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }


        /* Copy the field, noting the tfn. */
        rc = ib_field_copy(pf, dpi->mp, fullname, fnlen, *pf);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        (*pf)->tfn = (char *)ib_mpool_memdup(dpi->mp, tfn, tlen + 1);


        /* Transform. */
        tname = tfn;
        for (i = 0; i <= tlen; i++) {
            ib_tfn_t *t;
            ib_flags_t flags;

            if ((tfn[i] == ',') || (i == tlen)) {
                size_t len = (tfn + i) - tname;

                rc = ib_tfn_lookup_ex(ib, tname, len, &t);
                if (rc == IB_OK) {
                    ib_log_debug2(ib,
                                 "TFN: %" IB_BYTESTR_FMT ".%" IB_BYTESTR_FMT,
                                 IB_BYTESTRSL_FMT_PARAM(name, nlen),
                                 IB_BYTESTRSL_FMT_PARAM(tname, len));

                    rc = ib_tfn_transform(ib, dpi->mp, t, *pf, pf, &flags);
                    if (rc != IB_OK) {
                        /// @todo What to do here?  Fail or ignore?
                        ib_log_error(ib,
                                     "Transformation failed: %" IB_BYTESTR_FMT,
                                     IB_BYTESTRSL_FMT_PARAM(tname, len));
                    }
                }
                else {
                    /// @todo What to do here?  Fail or ignore?
                    ib_log_error(ib,
                                 "Unknown transformation: %" IB_BYTESTR_FMT,
                                 IB_BYTESTRSL_FMT_PARAM(tname, len));
                }
                tname = tfn + i + 1;

            }
        }

        /* Store the transformed field. */
        rc = ib_data_add_named(dpi, *pf, fullname, fnlen);
        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Cannot store field \"%.*s\" type=%d: %s",
                         (int)fnlen, fullname,
                         (int)(*pf)->type,
                         ib_status_to_string(rc));

            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_remove_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    ib_status_t rc;
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;

    rc = api->remove(dpi, name, nlen, pf);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t expand_lookup_fn(const void *data,
                                    const char *name,
                                    size_t nlen,
                                    ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(data != NULL);
    assert(name != NULL);
    assert(pf != NULL);

    ib_status_t rc;
    ib_provider_inst_t *dpi = (ib_provider_inst_t *)data;

    rc = ib_data_get_ex(dpi, name, nlen, pf);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_expand_str(ib_provider_inst_t *dpi,
                               const char *str,
                               bool recurse,
                               char **result)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    ib_status_t rc;
    rc = ib_expand_str_gen(dpi->mp,
                           str,
                           IB_VARIABLE_EXPANSION_PREFIX,
                           IB_VARIABLE_EXPANSION_POSTFIX,
                           recurse,
                           expand_lookup_fn,
                           dpi,
                           result);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_expand_str_ex(ib_provider_inst_t *dpi,
                                  const char *str,
                                  size_t slen,
                                  bool nul,
                                  bool recurse,
                                  char **result,
                                  size_t *result_len)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    ib_status_t rc;
    rc = ib_expand_str_gen_ex(dpi->mp,
                              str,
                              slen,
                              IB_VARIABLE_EXPANSION_PREFIX,
                              IB_VARIABLE_EXPANSION_POSTFIX,
                              nul,
                              recurse,
                              expand_lookup_fn,
                              dpi,
                              result,
                              result_len);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_expand_test_str(const char *str,
                                    bool *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_expand_test_str(
        str,
        IB_VARIABLE_EXPANSION_PREFIX,
        IB_VARIABLE_EXPANSION_POSTFIX,
        result);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_expand_test_str_ex(const char *str,
                                       size_t slen,
                                       bool *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc = ib_expand_test_str_ex(
        str,
        slen,
        IB_VARIABLE_EXPANSION_PREFIX,
        IB_VARIABLE_EXPANSION_POSTFIX,
        result);
    IB_FTRACE_RET_STATUS(rc);
}

static const int MAX_CAPTURE_NUM = 9;
typedef struct {
    const char *full;
    const char *name;
} capture_names_t;
static const capture_names_t names[] =
{
    { IB_TX_CAPTURE":0", "0" },
    { IB_TX_CAPTURE":1", "1" },
    { IB_TX_CAPTURE":2", "2" },
    { IB_TX_CAPTURE":3", "3" },
    { IB_TX_CAPTURE":4", "4" },
    { IB_TX_CAPTURE":5", "5" },
    { IB_TX_CAPTURE":6", "6" },
    { IB_TX_CAPTURE":7", "7" },
    { IB_TX_CAPTURE":8", "8" },
    { IB_TX_CAPTURE":9", "9" },
};
const char *ib_data_capture_name(int num)
{
    IB_FTRACE_INIT();
    assert(num >= 0);

    if (num <= MAX_CAPTURE_NUM) {
        IB_FTRACE_RET_CONSTSTR(names[num].name);
    }
    else {
        IB_FTRACE_RET_CONSTSTR("??");
    }
}
const char *ib_data_capture_fullname(int num)
{
    IB_FTRACE_INIT();
    assert(num >= 0);

    if (num <= MAX_CAPTURE_NUM) {
        IB_FTRACE_RET_CONSTSTR(names[num].full);
    }
    else {
        IB_FTRACE_RET_CONSTSTR(IB_TX_CAPTURE":??");
    }
}

/**
 * Get the capture list, create if required.
 *
 * @param[in] tx Transaction
 * @param[out] olist If not NULL, pointer to the capture item's list
 *
 * @returns IB_OK: All OK
 *          IB_EINVAL: @a num is too large
 *          Error status from: ib_data_get()
 *                             ib_data_add_list()
 *                             ib_field_value()
 */
static ib_status_t get_capture_list(ib_tx_t *tx,
                                    ib_list_t **olist)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_field_t *field = NULL;
    ib_list_t *list = NULL;
    ib_provider_inst_t *dpi;

    assert(tx != NULL);
    dpi = tx->dpi;
    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)tx->dpi->pr->api;

    /* Look up the capture list */
    rc = api->get(tx->dpi, IB_TX_CAPTURE, strlen(IB_TX_CAPTURE), &field);
    if (rc == IB_ENOENT) {
        rc = ib_data_add_list(dpi, IB_TX_CAPTURE, &field);
    }
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (field->type != IB_FTYPE_LIST) {
        ib_data_remove(dpi, IB_TX_CAPTURE, NULL);
    }
    rc = ib_field_mutable_value(field, ib_ftype_list_mutable_out(&list));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (olist != NULL) {
        *olist = list;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_capture_clear(ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);

    ib_status_t rc;
    ib_list_t *list;

    rc = get_capture_list(tx, &list);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_list_clear(list);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_data_capture_set_item(ib_tx_t *tx,
                                     int num,
                                     ib_field_t *in_field)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);
    assert(num >= 0);

    if (num > MAX_CAPTURE_NUM) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_status_t rc;
    ib_list_t *list;
    ib_field_t *field;
    ib_list_node_t *node;
    ib_list_node_t *next;
    const char *name;

    name = ib_data_capture_name(num);

    rc = get_capture_list(tx, &list);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Remove any nodes with the same name */
    IB_LIST_LOOP_SAFE(list, node, next) {
        field = (ib_field_t *)node->data;
        if (strncmp(name, field->name, field->nlen) == 0) {
            ib_list_node_remove(list, node);
        }
    }
    field = NULL;

    if(in_field == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Make sure we have the correct name */
    if (strncmp(name, in_field->name, in_field->nlen) == 0) {
        field = in_field;
    }
    else {
        rc = ib_field_alias(&field, tx->mp, name, strlen(name), in_field);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

    }
    assert(field != NULL);

    /* Add the node to the list */
    rc = ib_list_push(list, field);

    IB_FTRACE_RET_STATUS(rc);
}
