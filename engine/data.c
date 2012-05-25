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

#include "ironbee_private.h"


/* -- Constants -- */
#define DPI_LIST_FILTER_MARKER ':'
#define DPI_LIST_FILTER_PREFIX '/'
#define DPI_LIST_FILTER_SUFFIX '/'

/* -- Exported Data Access Routines -- */

ib_status_t ib_data_add(ib_provider_inst_t *dpi,
                        ib_field_t *f)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    rc = api->add(dpi, f, f->name, f->nlen);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_named(ib_provider_inst_t *dpi,
                              ib_field_t *f,
                              const char *key,
                              size_t klen)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    rc = api->add(dpi, f, key, klen);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_num_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               ib_num_t val,
                               ib_field_t **pf)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, name, nlen, IB_FTYPE_NUM, ib_ftype_num_in(&val));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_add_nulstr_ex(ib_provider_inst_t *dpi,
                                  const char *name,
                                  size_t nlen,
                                  char *val,
                                  ib_field_t **pf)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, name, nlen, IB_FTYPE_NULSTR, ib_ftype_nulstr_in(val));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f, f->name, f->nlen);
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
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create_bytestr_alias(&f, dpi->mp, name, nlen, val, vlen);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f, f->name, f->nlen);
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
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, IB_S2SL(name), IB_FTYPE_LIST, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f, f->name, f->nlen);
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
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_field_t *f;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, dpi->mp, IB_S2SL(name), IB_FTYPE_SBUFFER, NULL);
    if (rc != IB_OK) {
        ib_util_log_debug("SBUFFER field creation failed: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = api->add(dpi, f, f->name, f->nlen);
    ib_util_log_debug("SBUFFER field creation returned: %s", ib_status_to_string(rc));
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    IB_FTRACE_RET_STATUS(rc);
}

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
 *  - IB_ENOENT If the field is not found in the parent field.
 *  - IB_EINVAL The parent field is not a list or a dynamic type.
 *  - Other if a dynamic field fails.
 */
static ib_status_t ib_data_get_subfield(IB_PROVIDER_API_TYPE(data) *api,
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
    assert(name_len>0);
    assert(result_field);

    ib_status_t rc;
    ib_list_t *list; /* List of values to check stored in parent_field. */
    ib_list_node_t *list_node; /* List node in list. */


    /* Pull a value from a dynamic field. */
    if(ib_field_is_dynamic(parent_field)) {
        rc = ib_field_value_ex(parent_field,
                               result_field,
                               name,
                               name_len);
    }

    /* Check that our input field is a list type. */
    else if (parent_field->type == IB_FTYPE_LIST) {
        rc = ib_field_value(parent_field, &list);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        IB_LIST_LOOP(list, list_node) {
            ib_field_t *list_field = (ib_field_t *) list_node->data;

            if (list_field->nlen == name_len &&
                strncasecmp(list_field->name, name, name_len) == 0)
            {
                *result_field = list_field;
                IB_FTRACE_RET_STATUS(rc);
            }
        }

        *result_field = NULL;
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    /* We don't know what input type this is. Return IB_EINVAL. */
    else {
        rc = IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Return a list of fields whose name matches @pattern.
 *
 * The list @a field_name is retrieved from the @a dpi using @a api. Its
 * members are iterated through and the names of those fields compared
 * against @a pattern. If the name matches, the field is added to an
 * ib_list_t* which will be returned via 2a result_field.
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
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Allocate pattern_str to hold null terminated string. */
    pattern_str = (char *)malloc(pattern_len+1);
    if (pattern_str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Build a string to hand to the pcre library. */
    memcpy(pattern_str, pattern, pattern_len);
    pattern_str[pattern_len] = '\0';

    rc = ib_field_value(parent_field, &list);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    pcre_pattern = pcre_compile(pattern_str, 0, &errptr, &erroffset, NULL);
    if (pcre_pattern == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (errptr) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_list_create(&result_list, dpi->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_LIST_LOOP(list, list_node) {
        int pcre_rc;
        ib_field_t *list_field = (ib_field_t *) list_node->data;
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
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    rc = ib_field_create(result_field,
                         dpi->mp,
                         parent_field->name,
                         parent_field->nlen,
                         IB_FTYPE_LIST,
                         result_list);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_get_ex(ib_provider_inst_t *dpi,
                           const char *name,
                           size_t name_len,
                           ib_field_t **pf)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    ib_status_t rc;
    const char *error_msg;
    char *name_str = NULL;

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
                error_msg = "Filter start '/' does not immediately "
                            "follow ':' in: %s";
                goto error_handler;
            }

            /* Bad filter: FOO:/ */
            if (filter_start == filter_end) {
                rc = IB_EINVAL;
                error_msg = "Filter is not closed: %s";
                goto error_handler;
            }

            /* Bad filter: FOO:// */
            if (filter_start == filter_end-1) {
                rc = IB_EINVAL;
                error_msg = "Filter is empty: %s";
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
            rc = ib_data_get_subfield(api,
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
    ib_util_log_error(error_msg, name_str);
    free(name_str);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_get_all(ib_provider_inst_t *dpi,
                            ib_list_t *list)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

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
    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;

    ib_engine_t *ib = dpi->pr->ib;
    char *fullname;
    size_t fnlen;
    size_t tlen;
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

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
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    IB_PROVIDER_API_TYPE(data) *api =
        (IB_PROVIDER_API_TYPE(data) *)dpi->pr->api;

    rc = api->remove(dpi, name, nlen, pf);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_expand_str(ib_provider_inst_t *dpi,
                               const char *str,
                               char **result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    rc = ib_expand_str(dpi->mp,
                       str,
                       IB_VARIABLE_EXPANSION_PREFIX,
                       IB_VARIABLE_EXPANSION_POSTFIX,
                       (ib_hash_t *)(dpi->data),
                       result);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_data_expand_str_ex(ib_provider_inst_t *dpi,
                                  const char *str,
                                  size_t slen,
                                  ib_bool_t nul,
                                  char **result,
                                  size_t *result_len)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(dpi != NULL);
    assert(dpi->pr != NULL);
    assert(dpi->pr->api != NULL);

    rc = ib_expand_str_ex(dpi->mp,
                          str,
                          slen,
                          IB_VARIABLE_EXPANSION_PREFIX,
                          IB_VARIABLE_EXPANSION_POSTFIX,
                          nul,
                          (ib_hash_t *)dpi->data,
                          result,
                          result_len);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_data_expand_test_str(const char *str,
                                               ib_bool_t *result)
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

ib_status_t DLL_PUBLIC ib_data_expand_test_str_ex(const char *str,
                                                  size_t slen,
                                                  ib_bool_t *result)
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
