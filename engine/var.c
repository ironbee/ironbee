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
 * @brief IronBee --- Var Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/var.h>

#include <ironbee/array.h>
#include <ironbee/hash.h>

#include <pcre.h>

#include <assert.h>

/* types */

struct ib_var_config_t
{
    ib_mpool_t *mp;
    ib_hash_t  *index_by_name; /**< Hash of keys to index. Value: ib_var_source_t* */
    size_t      next_index;   /**< Next index to use. */
};

struct ib_var_store_t
{
    const ib_var_config_t *config; /**< Configuration. */
    ib_mpool_t            *mp;     /**< Memory pool. */
    ib_hash_t             *hash;   /**< Hash of data fields. Value: ib_field_t* */
    ib_array_t            *array;  /**< Array of indexed data fields.  Value: ib_field_t* */
};

struct ib_var_source_t
{
    const ib_var_config_t *config; /**< Configuration. */

    /**
     * Name of source.
     *
     * For indexed sources, this will be a copy of the name passed to
     * ib_var_source_register().  For unindexed sources, this will be an alias
     * of the name passed to ib_var_source_lookup().  This difference reflects
     * the performance and lifetime issues of each use.
     **/
    const char *name;

    size_t              name_length; /**< Length of name. */
    ib_rule_phase_num_t initial_phase; /**< Initial phase with value. */
    ib_rule_phase_num_t final_phase; /**< Final phase with value. */
    bool                is_indexed; /**< Is this an indexed source? */

    /**
     * Indexed if @c is_indexed is true.
     *
     * For unindexed sources, this member is intentionally left uninitialized
     * to allow valgrind to catch inappropriate uses of it.
     **/
    size_t index;
};

struct ib_var_filter_t
{
    /**
     * Filter string.
     *
     * This is the raw string provided to the filter.  It is passed directly
     * to dynamic fields.  If @ref re is NULL, it is also used as a
     * case-insensitive match to search non-dynamic fields.
     **/
    const char *filter_string;

    /**
     * Length of @ref filter_string.
     **/
    size_t filter_string_length;

    /**
     * Compiled regular expression.
     *
     * If @ref filter_string begins and ends with a forward slash, this will
     * be the compiled regexp of the substring between the slashes.
     * Otherwise, it will be NULL.
     **/
    pcre *re;

    /**
     * Study of @a re.
     **/
    pcre_extra *re_extra;
};

struct ib_var_target_t
{
    /**
     * Source.  May not be NULL.
     **/
    ib_var_source_t *source;

    /**
     * Filter.  May be NULL.
     *
     * A NULL filter is considered trivial and results in the field from
     * the source being wrapped in a list of size 1.
     **/
    const ib_var_filter_t *filter;
};

/* var_config */

ib_status_t ib_var_config_create(
    ib_var_config_t **config,
    ib_mpool_t       *mp
)
{
    assert(config != NULL);
    assert(mp     != NULL);

    ib_status_t      rc;
    ib_var_config_t *local_config;

    local_config = ib_mpool_alloc(mp, sizeof(*local_config));
    if (local_config == NULL) {
        return IB_EALLOC;
    }

    local_config->mp         = mp;
    local_config->next_index = 0;

    rc = ib_hash_create_nocase(&local_config->index_by_name, mp);
    if (rc != IB_OK) {
        return rc;
    }

    *config = local_config;

    return IB_OK;
}

ib_mpool_t *ib_var_config_pool(
    const ib_var_config_t *config
)
{
    assert(config != NULL);
    return config->mp;
}

/* var_store */

ib_status_t ib_var_store_create(
    ib_var_store_t        **store,
    ib_mpool_t             *mp,
    const ib_var_config_t  *config
)
{
    assert(store  != NULL);
    assert(mp     != NULL);
    assert(config != NULL);

    ib_status_t     rc;
    ib_var_store_t *local_store;

    local_store = ib_mpool_alloc(mp, sizeof(*local_store));
    if (local_store == NULL) {
        return IB_EALLOC;
    }

    local_store->config = config;
    local_store->mp     = mp;

    rc = ib_hash_create_nocase(&local_store->hash, mp);
    if (rc != IB_OK) {
        return rc;
    }

    if (local_store->config->next_index > 0) {
        rc = ib_array_create(
            &local_store->array,
            mp,
            local_store->config->next_index,
            5
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    *store = local_store;

    return IB_OK;
}

const ib_var_config_t *ib_var_store_config(
    const ib_var_store_t *store
)
{
    assert(store != NULL);

    return store->config;
}

ib_mpool_t *ib_var_store_pool(
    const ib_var_store_t *store
)
{
    assert(store != NULL);

    return store->mp;
}

/* var_source */

ib_status_t ib_var_source_register(
    ib_var_source_t     **source,
    ib_var_config_t      *config,
    const char           *name,
    size_t                name_length,
    ib_rule_phase_num_t   initial_phase,
    ib_rule_phase_num_t   final_phase
)
{
    assert(config != NULL);
    assert(name   != NULL);

    ib_var_source_t *local_source;
    ib_status_t      rc;

    local_source = ib_mpool_alloc(
        ib_var_config_pool(config),
        sizeof(*local_source)
    );
    if (local_source == NULL) {
        return IB_EALLOC;
    }

    if (final_phase != IB_PHASE_NONE && final_phase < initial_phase) {
        return IB_EINVAL;
    }

    rc = ib_hash_get_ex(config->index_by_name, NULL, name, name_length);
    if (rc != IB_ENOENT) {
        return IB_EEXIST;
    }

    local_source->name = ib_mpool_memdup(
        ib_var_config_pool(config),
        name, name_length
    );
    if (local_source->name == NULL) {
        return IB_EALLOC;
    }

    local_source->config        = config;
    local_source->name_length   = name_length;
    local_source->initial_phase = initial_phase;
    local_source->final_phase   = final_phase;
    local_source->is_indexed    = true;
    local_source->index         = config->next_index;

    rc = ib_hash_set_ex(
        config->index_by_name,
        local_source->name, local_source->name_length,
        local_source
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Nothing can fail now. Update state. */
    ++config->next_index;
    if (source != NULL) {
        *source = local_source;
    }

    return IB_OK;
}

const ib_var_config_t *ib_var_source_config(
    const ib_var_source_t *source
)
{
    assert(source != NULL);

    return source->config;
}

void ib_var_source_name(
    const ib_var_source_t  *source,
    const char            **name,
    size_t                 *name_length
)
{
    assert(source      != NULL);
    assert(name        != NULL);
    assert(name_length != NULL);

    *name        = source->name;
    *name_length = source->name_length;
}


ib_rule_phase_num_t ib_var_source_initial_phase(
    const ib_var_source_t *source
)
{
    assert(source != NULL);

    return source->initial_phase;
}

ib_rule_phase_num_t ib_var_source_final_phase(
    const ib_var_source_t *source
)
{
    assert(source != NULL);

    return source->final_phase;
}

bool ib_var_source_is_indexed(
    const ib_var_source_t *source
)
{
    assert(source != NULL);

    return source->is_indexed;
}

ib_status_t ib_var_source_get(
    ib_var_source_t  *source,
    ib_field_t      **field,
    ib_var_store_t   *store
)
{
    assert(source != NULL);
    assert(store  != NULL);

    if (ib_var_store_config(store) != ib_var_source_config(source)) {
        return IB_EINVAL;
    }

    if (source->is_indexed) {
        if (field == NULL) {
            return IB_OK;
        }
        return ib_array_get(store->array, source->index, field);
    }
    else {
        return ib_hash_get_ex(
            store->hash,
            field,
            source->name, source->name_length
        );
    }
}

ib_status_t ib_var_source_get_const(
    const ib_var_source_t  *source,
    const ib_field_t      **field,
    const ib_var_store_t   *store
)
{
    assert(source != NULL);
    assert(store  != NULL);

    /* Use non-const version; okay, as caller storing result in const */
    return ib_var_source_get(
        (ib_var_source_t *)source,
        (ib_field_t **)field,
        (ib_var_store_t*)store
    );
}

ib_status_t ib_var_source_set(
    ib_var_source_t *source,
    ib_var_store_t  *store,
    ib_field_t      *field
)
{
    assert(source != NULL);
    assert(store  != NULL);

    ib_status_t rc;

    if (ib_var_store_config(store) != ib_var_source_config(source)) {
        return IB_EINVAL;
    }

    if (field != NULL) {
        if (source->name_length != field->nlen) {
            return IB_EINVAL;
        }
        if (strncmp(source->name, field->name, source->name_length) != 0) {
            return IB_EINVAL;
        }
    }

    if (source->is_indexed) {
        rc = ib_array_setn(store->array, source->index, field);
        if (rc != IB_OK) {
            return rc;
        }
    }
    return ib_hash_set_ex(
        store->hash,
        source->name, source->name_length,
        field
    );
}

ib_status_t ib_var_source_lookup(
    ib_var_source_t       **source,
    ib_mpool_t             *mp,
    const ib_var_config_t  *config,
    const char             *name,
    size_t                  name_length
)
{
    assert(config != NULL);
    assert(name   != NULL);

    ib_status_t      rc;
    ib_var_source_t *local_source;

    rc = ib_hash_get_ex(
        config->index_by_name,
        &local_source,
        name, name_length
    );
    if (rc != IB_OK && rc != IB_ENOENT) {
        return rc;
    }

    if (rc == IB_ENOENT) {
        // Non-indexed.
        if (mp == NULL) {
            return IB_ENOENT;
        }

        local_source = ib_mpool_alloc(mp, sizeof(*local_source));
        if (local_source == NULL) {
            return IB_EALLOC;
        }

        local_source->name          = name;
        local_source->name_length   = name_length;
        local_source->config        = config;
        local_source->initial_phase = IB_PHASE_NONE;
        local_source->final_phase   = IB_PHASE_NONE;
        local_source->is_indexed    = false;
        /* Intentionally leaving index uninitialized so that valgrind can
         * catch invalid uses of it. */
    }

    if (source != NULL) {
        *source = local_source;
    }

    return IB_OK;
}

ib_status_t ib_var_source_initialize(
    ib_var_source_t  *source,
    ib_field_t      **field,
    ib_var_store_t   *store,
    ib_ftype_t        ftype
)
{
    assert(source != NULL);
    assert(store  != NULL);

    ib_status_t  rc;
    void        *val;
    ib_field_t  *local_field;
    ib_num_t     n = 0;
    ib_float_t   f = 0;
    ib_time_t    t = 0;

    switch (ftype) {
    case IB_FTYPE_NUM:
        val = ib_ftype_num_mutable_in(&n);
        break;
    case IB_FTYPE_FLOAT:
        val = ib_ftype_float_mutable_in(&f);
        break;
    case IB_FTYPE_TIME:
        val = ib_ftype_time_mutable_in(&t);
        break;
    case IB_FTYPE_NULSTR: {
        char *s = strdup("");
        if (s == NULL) {
            return IB_EALLOC;
        }
        val = ib_ftype_nulstr_mutable_in(s);
        break;
    }
    case IB_FTYPE_BYTESTR: {
        ib_bytestr_t *bs;
        rc = ib_bytestr_dup_nulstr(&bs, ib_var_store_pool(store), "");
        if (rc != IB_OK) {
            return rc;
        }
        val = ib_ftype_bytestr_mutable_in(bs);
        break;
    }
    case IB_FTYPE_LIST: {
        ib_list_t *l;
        rc = ib_list_create(&l, ib_var_store_pool(store));
        if (rc != IB_OK) {
            return rc;
        }
        val = ib_ftype_list_mutable_in(l);
        break;
    }
    default:
        return IB_EINVAL;
    }

    rc = ib_field_create_no_copy(
        &local_field,
        ib_var_store_pool(store),
        source->name, source->name_length,
        ftype,
        val
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_source_set(source, store, local_field);
    if (rc != IB_OK) {
        return rc;
    }

    if (field != NULL) {
        *field = local_field;
    }

    return IB_OK;
}

/* var_filter */

ib_status_t ib_var_filter_prepare(
    ib_var_filter_t **filter,
    ib_mpool_t       *mp,
    const char       *filter_string,
    size_t            filter_string_length,
    const char      **error_message,
    int              *error_offset
)
{
    assert(filter        != NULL);
    assert(mp            != NULL);
    assert(filter_string != NULL);

    ib_status_t rc;

    ib_var_filter_t *local_filter;
    local_filter = ib_mpool_alloc(mp, sizeof(*local_filter));
    if (local_filter == NULL) {
        return IB_EALLOC;
    }

    local_filter->filter_string =
        ib_mpool_memdup(mp, filter_string, filter_string_length);
    local_filter->filter_string_length = filter_string_length;

    local_filter->re = NULL;

    if (
        filter_string[0]                         == '/' &&
        filter_string[filter_string_length - 1]  == '/'
    ) {
        /* Regexp */
        const char *errmsg;
        int erroffset;

        /* pcre requires NUL terminated strings. */
        char *pattern =
            strndup(filter_string + 1, filter_string_length - 2);
        local_filter->re = pcre_compile(
            pattern,
            PCRE_NO_AUTO_CAPTURE,
            (error_message != NULL ? error_message : &errmsg),
            (error_offset != NULL ? error_offset : &erroffset),
            NULL
        );
        free(pattern);

        if (! local_filter->re) {
            return IB_EINVAL;
        }

        rc = ib_mpool_cleanup_register(mp, pcre_free, local_filter->re);
        if (rc != IB_OK) {
            assert(rc == IB_EALLOC);
            return rc;
        }

        const char *study_error_mem = NULL;
        const char **study_error = &study_error_mem;
        if (error_message != NULL) {
            study_error = error_message;
        }
        local_filter->re_extra = pcre_study(
            local_filter->re, PCRE_STUDY_JIT_COMPILE,
            study_error
        );
        if (*study_error != NULL) {
            return IB_EINVAL;
        }
        if (local_filter->re_extra) {
            rc = ib_mpool_cleanup_register(
                mp,
                /* The cast effectively casts pcre_extra* to void* */
                (ib_mpool_cleanup_fn_t)pcre_free_study,
                local_filter->re_extra
            );
            if (rc != IB_OK) {
                assert(rc == IB_EALLOC);
                return rc;
            }
        }
    }

    *filter = local_filter;

    return IB_OK;
}

ib_status_t ib_var_filter_apply(
    const ib_var_filter_t  *filter,
    const ib_list_t       **result,
    ib_mpool_t             *mp,
    const ib_field_t       *field
)
{
    assert(filter != NULL);
    assert(result != NULL);
    assert(field != NULL);

    ib_status_t rc;

    if (field->type != IB_FTYPE_LIST) {
        return IB_EINVAL;
    }

    if (ib_field_is_dynamic(field)) {
        /* dynamic */
        const ib_list_t *answer;
        rc = ib_field_value_ex(
            field,
            ib_ftype_list_out(&answer),
            filter->filter_string, filter->filter_string_length
        );
        if (rc != IB_OK) {
            return IB_EOTHER;
        }
        *result = answer;
    }
    else {
        ib_list_t *local_result;
        rc = ib_list_create(&local_result, mp);
        if (rc != IB_OK) {
            assert(rc == IB_EALLOC);
            return rc;
        }

        /* case-insensitive string search */
        const ib_list_t      *answer;
        const ib_list_node_t *node;

        rc = ib_field_value(field, ib_ftype_list_out(&answer));
        IB_LIST_LOOP_CONST(answer, node) {
            const ib_field_t *f =
                (const ib_field_t *)ib_list_node_data_const(node);
            bool push = false;

            if (filter->re != NULL) {
                push = (
                    pcre_exec(
                        filter->re, filter->re_extra,
                        f->name, f->nlen,
                        0,
                        0,
                        NULL,
                        0
                    ) >= 0
                );
            }
            else {
                push = (
                    filter->filter_string_length == f->nlen &&
                    strncasecmp(
                        filter->filter_string,
                        f->name, f->nlen
                    ) == 0
                );
            }

            if (push) {
                /* Discard const because lists are const-generic. */
                rc = ib_list_push(local_result, (void *)f);
                if (rc != IB_OK) {
                    return rc;
                }
            }
        }

        *result = local_result;
    }

    return IB_OK;
}

/* var_target */

ib_status_t ib_var_target_create(
    ib_var_target_t       **target,
    ib_mpool_t             *mp,
    ib_var_source_t        *source,
    const ib_var_filter_t  *filter
)
{
    assert(target != NULL);
    assert(mp     != NULL);
    assert(source != NULL);

    ib_var_target_t *local_target;

    local_target = ib_mpool_alloc(mp, sizeof(*local_target));
    if (local_target == NULL) {
        return IB_EALLOC;
    }

    local_target->source = source;
    local_target->filter = filter;

    *target = local_target;

    return IB_OK;
}

ib_status_t ib_var_target_prepare(
    ib_var_target_t       **target,
    ib_mpool_t             *mp,
    const ib_var_config_t  *config,
    const char             *target_string,
    size_t                  target_string_length,
    const char            **error_message,
    int                    *error_offset
)
{
    assert(target        != NULL);
    assert(mp            != NULL);
    assert(config        != NULL);
    assert(target_string != NULL);

    ib_status_t      rc;
    ib_var_source_t *source;
    ib_var_filter_t *filter;
    size_t           split_at;
    const char      *split;

    split  = memchr(target_string, ':', target_string_length);
    if (split == NULL) {
        split_at = target_string_length;
    }
    else {
        split_at = split - target_string;
    }

    if (split_at == 0) {
        return IB_EINVAL;
    }

    rc = ib_var_source_lookup(
        &source,
        mp,
        config,
        target_string, split_at
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* The -1 allows for trivial filters as "FOO:" */
    if (split_at < target_string_length - 1) {
        rc = ib_var_filter_prepare(
            &filter,
            mp,
            target_string + split_at + 1, target_string_length - split_at - 1,
            error_message, error_offset
        );
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        filter = NULL;
    }

    return ib_var_target_create(target, mp, source, filter);
}

ib_status_t ib_var_target_get(
    ib_var_target_t  *target,
    const ib_list_t **result,
    ib_mpool_t       *mp,
    ib_var_store_t   *store
)
{
    assert(target != NULL);
    assert(result != NULL);
    assert(mp     != NULL);
    assert(store  != NULL);

    ib_status_t      rc;
    ib_field_t      *field;
    const ib_list_t *local_result;

    rc = ib_var_source_get(
        target->source,
        &field,
        store
    );
    if (rc != IB_OK) {
        return rc;
    }

    if (target->filter != NULL) {
        /* Filter list field. */
        rc = ib_var_filter_apply(
            target->filter,
            &local_result,
            mp,
            field
        );
        if (rc != IB_OK) {
            return rc;
        }
    }
    else if (field->type == IB_FTYPE_LIST) {
        /* Directly return list field. */
        rc = ib_field_value(field, ib_ftype_list_out(&local_result));
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        /* Wrap non-list field in list. */
        ib_list_t *local_result_mutable;
        rc = ib_list_create(&local_result_mutable, mp);
        if (rc != IB_OK) {
            assert(rc == IB_EALLOC);
            return rc;
        }

        rc = ib_list_push(local_result_mutable, field);
        if (rc != IB_OK) {
            assert(rc == IB_EALLOC);
            return rc;
        }
        local_result = local_result_mutable;
    }

    *result = local_result;
    return IB_OK;
}

ib_status_t ib_var_target_get_const(
    const ib_var_target_t  *target,
    const ib_list_t       **result,
    ib_mpool_t             *mp,
    const ib_var_store_t   *store
)
{
    assert(target != NULL);
    assert(result != NULL);
    assert(mp     != NULL);
    assert(store  != NULL);

    /* Use non-const version; okay, as caller storing result in const. */
    return ib_var_target_get(
        (ib_var_target_t *)target,
        result,
        mp,
        (ib_var_store_t *)store
    );
}
