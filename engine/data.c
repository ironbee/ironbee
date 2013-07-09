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
 * @brief IronBee --- Data Access
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfel@calfeld.net>
 */

#include "ironbee_config_auto.h"

#include <ironbee/data.h>

#include <ironbee/bytestr.h>
#include <ironbee/engine.h>
#include <ironbee/expand.h>
#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <pcre.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

struct ib_data_config_t
{
    ib_mpool_t *mp;
    ib_hash_t  *index_by_key; /**< Hash of keys to index. */
    size_t      next_index;   /**< Next index to use. */
};

struct ib_data_t
{
    const ib_data_config_t *config; /**< Configuration; holds indices by keys. */
    ib_mpool_t             *mp;     /**< Memory pool. */
    ib_hash_t              *hash;   /**< Hash of data fields. */
    ib_array_t             *array;  /**< Array of indexed data fields. */
};

/* Internal helper functions */

/**
 * Get a subfield from @a data.
 *
 * If @a parent_field is a list (IB_FTYPE_LIST) then a case insensitive
 * string comparison is done to find the first list element that matches.
 *
 * If @a parent_field is a dynamic field, then the field @a name
 * is fetched from it and the return code from that operation is returned.
 *
 * @param[in] data         Data.
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
static
ib_status_t ib_data_get_subfields(
    const ib_data_t   *data,
    const ib_field_t  *parent_field,
    const char        *name,
    size_t             name_len,
    ib_field_t       **result_field
)
{
    assert(data != NULL);
    assert(parent_field != NULL);
    assert(name != NULL);
    assert(result_field != NULL);

    ib_status_t rc;
    ib_list_t *list; /* List of values to check stored in parent_field. */
    ib_list_node_t *list_node; /* List node in list. */

    if (name_len == 0) {
        return IB_EINVAL;
    }

    /* Check that our input field is a list type. */
    if (parent_field->type == IB_FTYPE_LIST) {
        ib_list_t *result_list;
        /* Pull a value from a dynamic field. */
        /* TODO: Make all of this const correct. */
        if (ib_field_is_dynamic(parent_field)) {
            rc = ib_field_value_ex(parent_field,
                                   &result_list,
                                   name,
                                   name_len);
            if (rc != IB_OK) {
              return rc;
            }
        }
        else {
            /* Make the result list */
            rc = ib_list_create(&result_list, data->mp);
            if (rc != IB_OK) {
                return rc;
            }

            /* Fetch the parent list. */
            rc = ib_field_value(parent_field, &list);
            if (rc != IB_OK) {
                return rc;
            }

            IB_LIST_LOOP(list, list_node) {
                ib_field_t *list_field =
                    (ib_field_t *) IB_LIST_NODE_DATA(list_node);

                if (list_field->nlen == name_len &&
                    strncasecmp(list_field->name, name, name_len) == 0)
                {
                    rc = ib_list_push(result_list, list_field);
                    if (rc != IB_OK) {
                        return rc;
                    }
                }
            }
          }
          /* Send back the result_list inside of result_field. */
          rc = ib_field_create(result_field,
                               data->mp,
                               parent_field->name,
                               parent_field->nlen,
                               IB_FTYPE_LIST,
                               result_list);

          return rc;
    }

    /* We don't know what input type this is. Return IB_EINVAL. */
    return IB_EINVAL;
}

/**
 * Return a list of fields whose name matches @a pattern.
 *
 * The list @a field_name is retrieved from @a data. Its members are iterated
 * through and the names of those fields compared against @a pattern. If the
 * name matches, the field is added to an @c ib_list_t* which will be returned
 * via @a result_field.
 *
 * @param[in] data         Data.
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
static
ib_status_t ib_data_get_filtered_list(
    const ib_data_t           *data,
    const ib_field_t          *parent_field,
    const char                *pattern,
    size_t                     pattern_len,
    ib_field_t               **result_field
)
{
    assert(data != NULL);
    assert(pattern != NULL);
    assert(parent_field != NULL);
    assert(pattern_len > 0);
    assert(result_field != NULL);

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

    rc = ib_list_create(&result_list, data->mp);
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
                         data->mp,
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
    return rc;
}

/**
 * Add a field to the @a data allowing for subfield notation.
 *
 * That is, a field may be stored in a normal field, such as @c FOO.
 * A field may also be stored in a subfield, that is a child field of
 * the list @c FOO. If @a name is @c FOO:BAR then the field @c BAR
 * will be stored in the list @c FOO in @a data.
 *
 * Note that in cases where @a field has a name other than @c BAR,
 * @a field 's name will be set to @c BAR using the @a data memory pool
 * for storage.
 *
 * @param[in] data Data.
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
static
ib_status_t ib_data_add_internal(
    ib_data_t  *data,
    ib_field_t *field,
    const char *name,
    size_t      nlen
)
{
    assert(data != NULL);
    assert(field != NULL);
    assert(name != NULL);

    ib_status_t rc;
    char *filter_marker;

    if (nlen == 0) {
        return IB_EINVAL;
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

        rc = ib_data_get_ex(data, parent_name, parent_nlen, &parent);
        /* If the field does not exist, make one. */
        if (rc == IB_ENOENT) {

            /* Try to add the list that does not exist. */
            rc = ib_data_add_list_ex(data, parent_name, parent_nlen, &parent);
            if (rc != IB_OK) {
                return rc;
            }
        }
        else if (rc != IB_OK) {
            return rc;
        }

        /* Ensure that the parent field is a list type. */
        if (parent->type != IB_FTYPE_LIST) {
            return IB_EINVAL;
        }

        /* If the child and the field do not have the same name,
         * set the field name to be the name it is stored under. */
        if (memcmp(child_name,
                   field->name,
                   (child_nlen < field->nlen) ? child_nlen : field->nlen))
        {
            field->nlen = child_nlen;
            field->name = ib_mpool_memdup(data->mp, child_name, child_nlen);
            if (field->name == NULL) {
                return IB_EALLOC;
            }
        }

        /* If the list already exists, add the value. */
        ib_field_list_add(parent, field);
    }

    /* Normal add. */
    else {
        size_t index;

        rc = ib_hash_set_ex(data->hash, name, nlen, field);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_data_lookup_index_ex(data->config, name, nlen, &index);
        if (rc == IB_OK) {
            assert(data->array != NULL);
            rc = ib_array_setn(data->array, index, field);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }

    return IB_OK;
}

static
ib_status_t expand_lookup_fn(
    const void  *raw_data,
    const char  *name,
    size_t       nlen,
    ib_field_t **pf
)
{
    assert(raw_data != NULL);
    assert(name != NULL);
    assert(pf != NULL);

    ib_status_t rc;
    const ib_data_t *data = (const ib_data_t *)raw_data;

    rc = ib_data_get_ex(data, name, nlen, pf);
    return rc;
}

/* -- Exported Data Access Routines -- */

ib_status_t ib_data_config_create(
    ib_mpool_t        *mp,
    ib_data_config_t **config
)
{
    assert(mp != NULL);
    assert(config != NULL);

    ib_status_t rc;
    ib_data_config_t *local_config;

    local_config = ib_mpool_calloc(mp, 1, sizeof(**config));
    if (local_config == NULL) {
        return IB_EALLOC;
    }
    local_config->mp = mp;
    local_config->next_index = 0;

    rc = ib_hash_create_nocase(&local_config->index_by_key, mp);
    if (rc != IB_OK) {
        return rc;
    }

    *config = local_config;

    return IB_OK;
}

ib_status_t ib_data_register_indexed_ex(
    ib_data_config_t *config,
    const char       *key,
    size_t            key_length,
    size_t           *index
)
{
    assert(config != NULL);
    assert(key != NULL);
    assert(key_length > 0);

    ib_status_t rc;
    rc = ib_hash_get_ex(config->index_by_key, NULL, key, key_length);
    if (rc != IB_ENOENT) {
        return IB_EINVAL;
    }


    size_t *local_index = ib_mpool_alloc(config->mp, sizeof(*local_index));
    if (local_index == NULL) {
        return IB_EALLOC;
    }

    *local_index = config->next_index;

    rc = ib_hash_set_ex(config->index_by_key, key, key_length, local_index);
    if (rc != IB_OK) {
        return rc;
    }

    /* Nothing can fail now. Update state. */
    ++config->next_index;
    if (index != NULL) {
        *index = *local_index;
    }

    return IB_OK;
}

ib_status_t ib_data_lookup_index_ex(
    const ib_data_config_t *config,
    const char             *key,
    size_t                  key_length,
    size_t                 *index
)
{
    assert(config != NULL);

    size_t *local_index;
    ib_status_t rc;

    /* Null and 0-length keys are allowed and are never indexed. */
    if (key == NULL || key_length == 0) {
        return IB_ENOENT;
    }

    rc = ib_hash_get_ex(config->index_by_key, &local_index, key, key_length);
    if (rc == IB_ENOENT) {
        return IB_ENOENT;
    }

    if (index != NULL) {
        *index = *local_index;
    }

    return IB_OK;
}

ib_status_t ib_data_lookup_index(
    const ib_data_config_t *config,
    const char             *key,
    size_t                 *index
)
{
    return ib_data_lookup_index_ex(config, key, strlen(key), index);
}

ib_status_t ib_data_register_indexed(
    ib_data_config_t *config,
    const char       *key
)
{
    return ib_data_register_indexed_ex(config, key, strlen(key), NULL);
}

ib_status_t ib_data_create(
    const ib_data_config_t  *config,
    ib_mpool_t              *mp,
    ib_data_t              **data
)
{
    assert(mp != NULL);
    assert(data != NULL);

    ib_status_t rc;

    *data = ib_mpool_calloc(mp, 1, sizeof(**data));
    if (*data == NULL) {
        return IB_EALLOC;
    }

    (*data)->config = config;
    (*data)->mp = mp;
    rc = ib_hash_create_nocase(&(*data)->hash, mp);
    if (rc != IB_OK) {
        *data = NULL;
        return rc;
    }

    if ((*data)->config->next_index > 0) {
        rc = ib_array_create(&(*data)->array, mp, (*data)->config->next_index, 5);
        if (rc != IB_OK) {
            *data = NULL;
            return rc;
        }
    }

    return IB_OK;
}

ib_mpool_t *ib_data_pool(
    const ib_data_t *data
)
{
    return data->mp;
}

ib_status_t ib_data_add(
    ib_data_t  *data,
    ib_field_t *f
)
{
    return ib_data_add_internal(data, f, f->name, f->nlen);
}

ib_status_t ib_data_add_named(
    ib_data_t  *data,
    ib_field_t *f,
    const char *key,
    size_t      klen
)
{
    return ib_data_add_internal(data, f, key, klen);
}

ib_status_t ib_data_add_num_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_num_t     val,
    ib_field_t **pf
)
{
    assert(data != NULL);

    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(
        &f,
        data->mp,
        name, nlen,
        IB_FTYPE_NUM,
        ib_ftype_num_in(&val)
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_data_add_internal(data, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    return rc;
}

ib_status_t ib_data_add_nulstr_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    const char  *val,
    ib_field_t **pf
)
{
    assert(data != NULL);

    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(
        &f,
        data->mp,
        name, nlen,
        IB_FTYPE_NULSTR,
        ib_ftype_nulstr_in(val)
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_data_add_internal(data, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    return rc;
}

ib_status_t ib_data_add_bytestr_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    uint8_t     *val,
    size_t       vlen,
    ib_field_t **pf
)
{
    assert(data != NULL);

    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create_bytestr_alias(&f, data->mp, name, nlen, val, vlen);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_data_add_internal(data, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    return rc;
}

ib_status_t ib_data_add_list_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_field_t **pf
)
{
    assert(data != NULL);

    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, data->mp, name, nlen, IB_FTYPE_LIST, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_data_add_internal(data, f, f->name, f->nlen);
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    return rc;
}

ib_status_t ib_data_add_stream_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_field_t **pf
)
{
    assert(data != NULL);

    ib_field_t *f;
    ib_status_t rc;

    if (pf != NULL) {
        *pf = NULL;
    }

    rc = ib_field_create(&f, data->mp, name, nlen, IB_FTYPE_SBUFFER, NULL);
    if (rc != IB_OK) {
        ib_util_log_debug(
            "SBUFFER field creation failed: %s", ib_status_to_string(rc)
        );
        return rc;
    }

    rc = ib_data_add_internal(data, f, f->name, f->nlen);
    ib_util_log_debug(
        "SBUFFER field creation returned: %s", ib_status_to_string(rc)
    );
    if ((rc == IB_OK) && (pf != NULL)) {
        *pf = f;
    }

    return rc;
}

ib_status_t ib_data_get_ex(
    const ib_data_t  *data,
    const char       *name,
    size_t            name_len,
    ib_field_t      **pf
)
{
    assert(data != NULL);

    ib_status_t rc;
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
        rc = ib_hash_get_ex(data->hash, &parent_field, name, filter_marker - name);
        if (rc != IB_OK) {
            return rc;
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
            rc = ib_data_get_filtered_list(
                data,
                parent_field,
                filter_start+1,
                filter_end - filter_start - 1,
                pf
            );
        }

        /* No pattern match. Just extract the sub-field. */
        else {

            /* Handle extracting a subfield for a list of a dynamic field. */
            rc = ib_data_get_subfields(
                data,
                parent_field,
                filter_marker+1,
                name_len - (filter_marker+1-name),
                pf
            );
        }
    }

    /* Typical no-expansion fetch of a value. */
    else {
        rc = ib_hash_get_ex(data->hash, pf, name, name_len);
    }

    return rc;

    /* Error handling routine. */
error_handler:
    name_str = malloc(name_len+1);
    if ( name_str == NULL ) {
        return IB_EALLOC;
    }

    memcpy(name_str, name, name_len);
    name_str[name_len] = '\0';
    free(name_str);

    return rc;
}

ib_status_t ib_data_get_indexed(
    const ib_data_t  *data,
    size_t            index,
    ib_field_t      **pf
)
{
    assert(data != NULL);

    ib_status_t rc;
    ib_field_t *f;

    if (data->array == NULL) {
        /* No indexed fields. */
        assert(data->config->next_index == 0);
        return IB_ENOENT;
    }

    rc = ib_array_get(data->array, index, &f);
    if (rc != IB_OK) {
        return IB_ENOENT;
    }

    if (pf != NULL) {
        *pf = f;
    }

    return IB_OK;
}

ib_status_t ib_data_get_all(
    const ib_data_t *data,
    ib_list_t       *list
)
{
    assert(data != NULL);
    assert(data->hash != NULL);

    return ib_hash_get_all(data->hash, list);
}

ib_status_t ib_data_add_num(
    ib_data_t   *data,
    const char  *name,
    ib_num_t     val,
    ib_field_t **pf
)
{
    return ib_data_add_num_ex(data, name, strlen(name), val, pf);
}

ib_status_t ib_data_add_nulstr(
    ib_data_t   *data,
    const char  *name,
    const char  *val,
    ib_field_t **pf
)
{
    return ib_data_add_nulstr_ex(data, name, strlen(name), val, pf);
}

ib_status_t ib_data_add_bytestr(
    ib_data_t   *data,
    const char  *name,
    uint8_t     *val,
    size_t       vlen,
    ib_field_t **pf
)
{
    return ib_data_add_bytestr_ex(data, name, strlen(name), val, vlen, pf);
}

ib_status_t ib_data_add_list(
    ib_data_t   *data,
    const char  *name,
    ib_field_t **pf
)
{
    return ib_data_add_list_ex(data, name, strlen(name), pf);
}

ib_status_t ib_data_add_stream(
    ib_data_t   *data,
    const char  *name,
    ib_field_t **pf
)
{
    return ib_data_add_stream_ex(data, name, strlen(name), pf);
}

ib_status_t ib_data_get(
    const ib_data_t  *data,
    const char       *name,
    ib_field_t      **pf
)
{
    return ib_data_get_ex(data, name, strlen(name), pf);
}

ib_status_t ib_data_remove(
    ib_data_t   *data,
    const char  *name,
    ib_field_t **pf
)
{
    return ib_data_remove_ex(data, name, strlen(name), pf);
}

ib_status_t ib_data_remove_ex(ib_data_t *data,
                              const char *name,
                              size_t nlen,
                              ib_field_t **pf)
{
    assert(data != NULL);

    return ib_hash_remove_ex(data->hash, pf, name, nlen);
}

ib_status_t ib_data_set(
    ib_data_t  *data,
    ib_field_t *f,
    const char *name,
    size_t      nlen
)
{
    assert(data != NULL);
    return ib_hash_set_ex(data->hash, name, nlen, f);
}

ib_status_t ib_data_set_relative(
    ib_data_t  *data,
    const char *name,
    size_t      nlen,
    intmax_t    adjval
)
{
    ib_field_t *f;
    ib_status_t rc;
    ib_num_t num;

    rc = ib_data_get_ex(data, name, nlen, &f);
    if (rc != IB_OK) {
        return IB_ENOENT;
    }

    switch (f->type) {
        case IB_FTYPE_NUM:
            /// @todo Make sure this is atomic
            /// @todo Check for overflow
            rc = ib_field_value(f, ib_ftype_num_out(&num));
            if (rc != IB_OK) {
                return rc;
            }
            num += adjval;
            rc = ib_field_setv(f, ib_ftype_num_in(&num));
            break;
        default:
            return IB_EINVAL;
    }

    return rc;
}

ib_status_t ib_data_expand_str(
    const ib_data_t  *data,
    const char       *str,
    bool              recurse,
    char            **result
)
{
    assert(data != NULL);

    return ib_expand_str_gen(
        data->mp,
        str,
        IB_VARIABLE_EXPANSION_PREFIX,
        IB_VARIABLE_EXPANSION_POSTFIX,
        recurse,
        expand_lookup_fn,
        data,
        result
    );
}

ib_status_t ib_data_expand_str_ex(
    const ib_data_t  *data,
    const char       *str,
    size_t            slen,
    bool              nul,
    bool              recurse,
    char            **result,
    size_t           *result_len
)
{
    assert(data != NULL);

    return ib_expand_str_gen_ex(
        data->mp,
        str,
        slen,
        IB_VARIABLE_EXPANSION_PREFIX,
        IB_VARIABLE_EXPANSION_POSTFIX,
        nul,
        recurse,
        expand_lookup_fn,
        data,
        result,
        result_len
    );
}

void ib_data_expand_test_str(
    const char *str,
    bool       *result
)
{
    ib_status_t rc;
    
    rc = ib_expand_test_str(
        str,
        IB_VARIABLE_EXPANSION_PREFIX,
        IB_VARIABLE_EXPANSION_POSTFIX,
        result
    );

    assert(rc == IB_OK);
}

void ib_data_expand_test_str_ex(
    const char *str,
    size_t      slen,
    bool       *result
)
{
    ib_status_t rc;
    
    rc = ib_expand_test_str_ex(
        str,
        slen,
        IB_VARIABLE_EXPANSION_PREFIX,
        IB_VARIABLE_EXPANSION_POSTFIX,
        result
    );

    assert(rc == IB_OK);
}
