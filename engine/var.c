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
#include <ironbee/string_assembly.h>

#include <pcre.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

/* types */

struct ib_var_config_t
{
    /** Memory pool */
    ib_mpool_t *mp;

    /** Hash of keys to index.  Value `ib_var_source_t *` */
    ib_hash_t *index_by_name;

    /** Next index to use. */
    size_t next_index;
};

struct ib_var_store_t
{
    /** Configuration */
    const ib_var_config_t *config;
    /** Memory pool */
    ib_mpool_t *mp;
    /** Hash of source name to value. Value: `ib_field_t *` */
    ib_hash_t *hash;
    /** Array of source index to value.  Value: `ib_field_t *` */
    ib_array_t *array;
};

struct ib_var_source_t
{
    /** Configuration */
    const ib_var_config_t *config;

    /**
     * Name of source.
     *
     * For indexed sources, this will be a copy of the name passed to
     * ib_var_source_register().  For unindexed sources, this will be an alias
     * of the name passed to ib_var_source_acquire().  This difference
     * reflects the performance and lifetime issues of each use.
     **/
    const char *name;

    /** Length of @ref name */
    size_t name_length;
    /** Initial phase value is set. */
    ib_rule_phase_num_t initial_phase;
    /** Final phase with value is changed. */
    ib_rule_phase_num_t final_phase;

    /**
     * Is source indexed?
     *
     * If true, @ref index is meaningful and can be used to lookup value in
     * ib_var_store_t::array.  If false, @ref index is meaningless, and value
     * must be looked up by name in ib_var_store_t::hash.
     */
    bool is_indexed;

    /**
     * Index (only if @ref is_indexed is true).
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
     * Expand to lazily construct filter.
     *
     * If NULL, then use @ref filter.
     **/
    const ib_var_expand_t *expand;

    /**
     * Filter.  May be NULL.
     *
     * If @ref expand and @ref filter are null, then this is a trivial target
     * and the result is the source being wrapped in a list of size 1.
     **/
    const ib_var_filter_t *filter;
};

struct ib_var_expand_t
{
    /** Text before expansion.  May be NULL. */
    const char *prefix;
    /** Length of @a prefix. */
    size_t prefix_length;
    /** Target after prefix.  May be NULL. */
    const ib_var_target_t *target;
    /** Next expansion chunk. */
    ib_var_expand_t *next;
};

/* helpers */

/**
 * Convert a field to a string.
 *
 * Any errors will result in an expansion of "ERROR".  Only bytestring, num
 * and float fields are supported.  All others are expanded into
 * "UNSUPPORTED".
 *
 * @param[out] dst        Result.  For bytestring fields, lifetime will equal
 *                        value of bytestring.  For other fields, lifetime
 *                        will at least be as long as @a mp.
 * @param[out] dst_length Length of @a dst.
 * @param[in]  field      Field to convert.
 * @param[in]  mp         Memory pool to use.
 **/
static void field_to_string(
    const char       **dst,
    size_t            *dst_length,
    const ib_field_t  *field,
    ib_mpool_t        *mp
)
NONNULL_ATTRIBUTE(1, 2, 3, 4);

/**
 * Find expansion substring in @a s.
 *
 * Looks for "%{" in @a s.  If found, looks for "}" after that.  If found,
 * outputs locations to @a a and @a b and returns true.
 *
 * The prefix will be `[s, a)`; expansion will be `[a + 2, b)`, and suffix
 * will be `[b + 1, s + l)`.
 *
 * @param[out] a Will point to '%' in first "%{".
 * @param[out] b Will point to '}' in following "}".
 * @param[in]  s String to search.
 * @param[in]  l Length of @a s.
 * @return true iff an expansion substring was found in @a s.
 **/
bool find_expand_string(
    const char **a,
    const char **b,
    const char  *s,
    size_t       l
)
NONNULL_ATTRIBUTE(1, 2, 3);


/**
 * Get the filter for a target, expanding if needed.
 *
 * @param[in]  target Target to get filter for.
 * @param[out] result Where to store filter.  Lifetime is either that of
 *                    @a target (if non-expand) or @a mp (if expand).
 * @param[in]  mp     Memory pool to use for expansion.  May be NULL if no
 *                    expansion.
 * @param[in]  store  Store to use for expansion.
 *
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if expansion results in regexp filter.
 * - Any return of ib_var_expand_execute() or ib_var_filter_acquire().
 **/
static
ib_status_t target_filter_get(
    const ib_var_target_t  *target,
    const ib_var_filter_t **result,
    ib_mpool_t             *mp,
    const ib_var_store_t   *store
)
NONNULL_ATTRIBUTE(1, 2, 4);

/* var_config */

ib_status_t ib_var_config_acquire(
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

ib_status_t ib_var_store_acquire(
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

void ib_var_store_export(
    ib_var_store_t *store,
    ib_list_t      *result
)
{
    assert(store  != NULL);
    assert(result != NULL);

    /* Ignore return code.  Can only be IB_ENOENT */
    ib_hash_get_all(store->hash, result);
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
        ib_field_t *local_field = NULL;
        ib_status_t rc;

        rc = ib_array_get(store->array, source->index, &local_field);

        /* Array only errors if out of band, i.e., not set. */
        if (rc != IB_OK || local_field == NULL) {
            return IB_ENOENT;
        }
        if (field != NULL) {
            *field = local_field;
        }
        return rc;
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

    /* Use non-const version; okay, as caller storing result in const. */
    return ib_var_source_get(
        (ib_var_source_t *)source,
        (ib_field_t **)field,
        (ib_var_store_t *)store
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
        field->name = source->name;
        field->nlen = source->name_length;
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

ib_status_t ib_var_source_acquire(
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
        /* Non-indexed. */
        if (mp == NULL) {
            return IB_ENOENT;
        }

        local_source = ib_mpool_alloc(mp, sizeof(*local_source));
        if (local_source == NULL) {
            return IB_EALLOC;
        }

        local_source->name          = ib_mpool_memdup(mp, name, name_length);
        if (local_source->name == NULL) {
            return IB_EALLOC;
        }
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

ib_status_t ib_var_source_append(
    ib_var_source_t *source,
    ib_var_store_t  *store,
    ib_field_t      *field
)
{
    assert(source != NULL);
    assert(field  != NULL);
    assert(store  != NULL);

    ib_status_t  rc;
    ib_field_t  *source_field;
    ib_list_t   *list;

    rc = ib_var_source_get(source, &source_field, store);
    if (rc != IB_OK && rc != IB_ENOENT) {
        return rc;
    }

    if (rc == IB_ENOENT) {
        rc = ib_var_source_initialize(
            source,
            &source_field,
            store,
            IB_FTYPE_LIST
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    assert(source_field != NULL);

    if (source_field->type != IB_FTYPE_LIST) {
        return IB_EINCOMPAT;
    }

    rc = ib_field_value(source_field, ib_ftype_list_mutable_out(&list));
    if (rc != IB_OK) {
        return rc == IB_EALLOC ? rc : IB_EOTHER;
    }

    rc = ib_list_push(list, field);
    if (rc != IB_OK) {
        return rc == IB_EALLOC ? rc : IB_EOTHER;
    }

    return IB_OK;
}

/* var_filter */

ib_status_t ib_var_filter_acquire(
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
        filter_string_length                     >= 2   &&
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
#ifdef PCRE_STUDY_JIT_COMPILE
        local_filter->re_extra = pcre_study(
            local_filter->re, PCRE_STUDY_JIT_COMPILE,
            study_error
        );
#else
        local_filter->re_extra = pcre_study(
            local_filter->re, 0,
            study_error
        );
#endif
        if (*study_error != NULL) {
            return IB_EINVAL;
        }
        if (local_filter->re_extra) {
            rc = ib_mpool_cleanup_register(
                mp,
                /* The cast effectively casts pcre_extra* to void* */
#ifdef PCRE_STUDY_JIT_COMPILE
                (ib_mpool_cleanup_fn_t)pcre_free_study,
#else
                (ib_mpool_cleanup_fn_t)pcre_free,
#endif
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
    assert(field  != NULL);

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
        /* Can only fail on dynamic field. */
        assert(rc == IB_OK);
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
                    assert(rc == IB_EALLOC);
                    return rc;
                }
            }
        }

        *result = local_result;
    }

    return IB_OK;
}

ib_status_t ib_var_filter_remove(
    const ib_var_filter_t  *filter,
    ib_list_t             **result,
    ib_mpool_t             *mp,
    ib_field_t             *field
)
{
    assert(filter != NULL);
    assert(field  != NULL);
    assert((result != NULL && mp != NULL) || (result == NULL && mp == NULL));

    ib_status_t    rc;
    ib_list_t      *local_result = NULL;
    ib_list_t      *field_list;
    ib_list_node_t *node;
    ib_list_node_t *next_node;
    bool            removed = false;

    if (field->type != IB_FTYPE_LIST || ib_field_is_dynamic(field)) {
        return IB_EINVAL;
    }

    if (filter->re != NULL) {
        return IB_EINVAL;
    }

    if (result != NULL) {
        rc = ib_list_create(&local_result, mp);
        if (rc != IB_OK) {
            assert(rc == IB_EALLOC);
            return rc;
        }
    }

    rc = ib_field_value(field, ib_ftype_list_mutable_out(&field_list));
    assert(rc == IB_OK);
    /* Can only fail on dynamic field. */
    IB_LIST_LOOP_SAFE(field_list, node, next_node) {
        ib_field_t *f = (ib_field_t *)ib_list_node_data(node);
        if (
            filter->filter_string_length == f->nlen &&
            strncasecmp(
                filter->filter_string,
                f->name, f->nlen
            ) == 0
        ) {
            if (result != NULL) {
                rc = ib_list_push(local_result, (void *)f);
                if (rc != IB_OK) {
                    assert(rc == IB_EALLOC);
                    return rc;
                }
            }
            ib_list_node_remove(field_list, node);
            removed = true;
        }
    }

    if (! removed) {
        return IB_ENOENT;
    }

    if (result != NULL) {
        *result = local_result;
    }

    return IB_OK;
}

/* var_target */

ib_status_t ib_var_target_acquire(
    ib_var_target_t       **target,
    ib_mpool_t             *mp,
    ib_var_source_t        *source,
    const ib_var_expand_t  *expand,
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
    local_target->expand = expand;
    local_target->filter = filter;

    *target = local_target;

    return IB_OK;
}

void ib_var_target_source_name(
    const ib_var_target_t  *target,
    const char            **name,
    size_t                 *len
)
{
    assert(target != NULL);
    assert(target->source != NULL);

    ib_var_source_name(target->source, name, len);
}

ib_status_t ib_var_target_acquire_from_string(
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
    ib_var_expand_t *expand = NULL;
    ib_var_filter_t *filter = NULL;
    size_t           split_at;
    const char      *split;

    split = memchr(target_string, ':', target_string_length);
    if (split == NULL) {
        split_at = target_string_length;
    }
    else {
        split_at = split - target_string;
    }

    if (split_at == 0) {
        return IB_EINVAL;
    }

    rc = ib_var_source_acquire(
        &source,
        mp,
        config,
        target_string, split_at
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* The -1 allows for trivial filters such as "FOO:" */
    if (split_at < target_string_length - 1) {
        const char *filter_string = target_string + split_at + 1;
        size_t filter_string_length = target_string_length - split_at - 1;
        /* Do not allow expansions in regexp. */
        if (
            filter_string[0] == '/' ||
            ! ib_var_expand_test(filter_string, filter_string_length)
        ) {
            rc = ib_var_filter_acquire(
                &filter,
                mp,
                filter_string, filter_string_length,
                error_message, error_offset
            );
        }
        else {
            rc = ib_var_expand_acquire(
                &expand,
                mp,
                filter_string, filter_string_length,
                config,
                error_message, error_offset
            );
        }
        if (rc != IB_OK) {
            return rc;
        }
    }

    return ib_var_target_acquire(target, mp, source, expand, filter);
}

ib_status_t target_filter_get(
    const ib_var_target_t  *target,
    const ib_var_filter_t **result,
    ib_mpool_t             *mp,
    const ib_var_store_t   *store
)
{
    assert(target != NULL);
    assert(result != NULL);
    assert(mp != NULL || target->expand == NULL);
    assert(
        (target->expand == NULL && target->filter == NULL) ||
        (target->expand == NULL && target->filter != NULL) ||
        (target->expand != NULL && target->filter == NULL)
    );
    assert(store  != NULL);

    ib_status_t rc;

    if (target->expand == NULL) {
        *result = target->filter;
    }
    else {
        const char *filter_string;
        size_t filter_string_length;
        ib_var_filter_t *local_filter;

        rc = ib_var_expand_execute(
            target->expand,
            &filter_string,
            &filter_string_length,
            mp,
            store
        );
        if (rc != IB_OK) {
            return rc;
        }

        if (filter_string_length > 0 && filter_string[0] == '/') {
            return IB_EINVAL;
        }

        rc = ib_var_filter_acquire(
            &local_filter,
            mp,
            filter_string, filter_string_length,
            NULL, NULL /* Know not a regexp filter. */
        );
        if (rc != IB_OK) {
            return rc;
        }
        *result = local_filter;
    }

    return IB_OK;
}

ib_status_t ib_var_target_type(
    ib_var_target_t  *target,
    ib_var_store_t   *store,
    ib_ftype_t       *type
)
{
    assert(target != NULL);
    assert(target->source != NULL);
    assert(store  != NULL);
    assert(
        (target->expand == NULL && target->filter == NULL) ||
        (target->expand == NULL && target->filter != NULL) ||
        (target->expand != NULL && target->filter == NULL)
    );

    ib_status_t  rc;
    ib_field_t  *field;

    /* If there is a filter, we expect the type to be a list, and
     * will report it as such.
     */
    if (target->filter != NULL) {
        *type = IB_FTYPE_LIST;
        return IB_OK;
    }

    rc = ib_var_source_get(target->source, &field, store);
    if (rc != IB_OK) {
        return rc;
    }

    /* We got a result! Return the type. */
    *type = field->type;
    return IB_OK;
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
    assert(
        (target->expand == NULL && target->filter == NULL) ||
        (target->expand == NULL && target->filter != NULL) ||
        (target->expand != NULL && target->filter == NULL)
    );

    ib_status_t            rc;
    ib_field_t            *field;
    const ib_list_t       *local_result;
    const ib_var_filter_t *filter = NULL;

    rc = ib_var_source_get(
        target->source,
        &field,
        store
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = target_filter_get(target, &filter, mp, store);
    if (rc != IB_OK) {
        return rc;
    }

    if (filter != NULL) {
        /* Filter list field. */
        rc = ib_var_filter_apply(
            filter,
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

ib_status_t ib_var_target_remove(
    ib_var_target_t  *target,
    ib_list_t       **result,
    ib_mpool_t       *mp,
    ib_var_store_t   *store
)
{
    assert(target != NULL);
    assert(store  != NULL);
    assert((result != NULL && mp != NULL) || (result == NULL && mp == NULL));
    assert(
        (target->expand == NULL && target->filter == NULL) ||
        (target->expand == NULL && target->filter != NULL) ||
        (target->expand != NULL && target->filter == NULL)
    );

    ib_status_t rc;
    ib_mpool_t *local_mp = NULL;
    const ib_var_filter_t *filter;
    ib_field_t *field;
    ib_list_t *local_result = NULL;

    /* No regexp filters. */
    if (target->filter != NULL && target->filter->re != NULL) {
        return IB_EINVAL;
    }

    /* Create result list if needed. */
    if (result != NULL) {
        rc = ib_list_create(&local_result, mp);
        if (rc != IB_OK) {
            assert(rc == IB_EALLOC);
            return rc;
        }
    }

    /* Fetch and check value. */
    rc = ib_var_source_get(target->source, &field, store);
    if (rc != IB_OK) {
        return rc;
    }

    /* Figure out if we need a local memory pool. */
    if (mp != NULL) {
        local_mp = mp;
    }
    else if (target->expand != NULL) {
        rc = ib_mpool_create(&local_mp, "ib_var_target_remove", NULL);
        if (rc != IB_OK) {
            assert(rc == IB_EALLOC);
            return rc;
        }
    }

    /* !!! From here on, cannot return directly; must goto finish. !!! */

    rc = target_filter_get(target, &filter, local_mp, store);
    if (rc != IB_OK) {
        goto finish;
    }

    if (filter == NULL) {
        /* Trivial */
        if (local_result != NULL) {
            ib_list_push(local_result, (void *)field);
        }

        rc = ib_var_source_set(target->source, store, NULL);
        goto finish;
    }
    else if (local_mp != NULL) {
        /* Simple */
        rc = ib_var_filter_remove(filter, &local_result, local_mp, field);
        goto finish;
    }
    else {
        /* No memory pool. */
        rc = ib_var_filter_remove(filter, NULL, NULL, field);
        goto finish;
    }

finish:
    if (mp == NULL && local_mp != NULL) {
        ib_mpool_destroy(local_mp);
    }
    if (result != NULL && rc == IB_OK) {
        *result = local_result;
    }

    return rc;
}

ib_status_t ib_var_target_expand(
    ib_var_target_t       *target,
    ib_var_target_t      **expanded,
    ib_mpool_t            *mp,
    const ib_var_store_t  *store
)
{
    assert(target   != NULL);
    assert(expanded != NULL);
    assert(mp       != NULL);
    assert(store    != NULL);

    const ib_var_filter_t *expanded_filter;
    ib_var_target_t *expanded_target;
    ib_status_t rc;

    if (target->expand == NULL) {
        *expanded = target;
        return IB_OK;
    }

    rc = target_filter_get(target, &expanded_filter, mp, store);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_target_acquire(
        &expanded_target,
        mp,
        target->source,
        NULL,
        expanded_filter
    );
    if (rc != IB_OK) {
        return rc;
    }

    *expanded = expanded_target;
    return IB_OK;
}

ib_status_t ib_var_target_expand_const(
    const ib_var_target_t  *target,
    const ib_var_target_t **expanded,
    ib_mpool_t             *mp,
    const ib_var_store_t   *store
)
{
    assert(target   != NULL);
    assert(expanded != NULL);
    assert(mp       != NULL);
    assert(store    != NULL);

    /* Use non-const version; okay, as caller storing result in const */
    return ib_var_target_expand(
        (ib_var_target_t *)target,
        (ib_var_target_t **)expanded,
        mp,
        store
    );
}

ib_status_t ib_var_target_set(
    ib_var_target_t *target,
    ib_mpool_t      *mp,
    ib_var_store_t  *store,
    ib_field_t      *field
)
{
    assert(target != NULL);
    assert(mp     != NULL);
    assert(store  != NULL);
    assert(field  != NULL);
    assert(
        (target->expand == NULL && target->filter == NULL) ||
        (target->expand == NULL && target->filter != NULL) ||
        (target->expand != NULL && target->filter == NULL)
    );

    ib_status_t rc;
    const ib_var_filter_t *filter;
    ib_field_t *source_field;
    ib_list_t *list;

    /* No regexp filters. */
    if (target->filter != NULL && target->filter->re != NULL) {
        return IB_EINVAL;
    }

    rc = target_filter_get(target, &filter, mp, store);
    if (rc != IB_OK) {
        return rc;
    }

    if (filter == NULL) {
        return ib_var_source_set(target->source, store, field);
    }

    /* Target must be simple. */
    rc = ib_var_source_get(target->source, &source_field, store);
    if (rc == IB_ENOENT) {
        rc = ib_var_source_initialize(
            target->source,
            &source_field,
            store,
            IB_FTYPE_LIST
        );
        if (rc != IB_OK) {
            return rc;
        }
    }
    if (
        source_field->type != IB_FTYPE_LIST ||
        ib_field_is_dynamic(source_field)
    ) {
        return IB_EINVAL;
    }

    field->name = filter->filter_string;
    field->nlen = filter->filter_string_length;

    rc = ib_field_value(source_field, ib_ftype_list_mutable_out(&list));
    if (rc != IB_OK) {
        return rc == IB_EALLOC ? rc : IB_EOTHER;
    }
    rc = ib_list_push(list, field);
    if (rc != IB_OK) {
        return rc == IB_EALLOC ? rc : IB_EOTHER;
    }

    return IB_OK;
}

ib_status_t ib_var_target_remove_and_set(
    ib_var_target_t *target,
    ib_mpool_t      *mp,
    ib_var_store_t  *store,
    ib_field_t      *field
)
{
    assert(target != NULL);
    assert(mp     != NULL);
    assert(store  != NULL);
    assert(field  != NULL);

    ib_var_target_t *expanded;
    ib_status_t rc;

    rc = ib_var_target_expand(target, &expanded, mp, store);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_target_remove(target, NULL, NULL, store);
    if (rc != IB_OK && rc != IB_ENOENT) {
        return rc;
    }

    return ib_var_target_set(target, mp, store, field);
}

/* var_expand */

void field_to_string(
    const char       **dst,
    size_t            *dst_length,
    const ib_field_t  *field,
    ib_mpool_t        *mp
)
{
    assert(dst        != NULL);
    assert(dst_length != NULL);
    assert(field      != NULL);
    assert(mp         != NULL);

    /* Length sufficient for number or float. */
    static const size_t c_numeric_width = 45;

    ib_status_t rc;
    char *local_dst;

    switch (field->type) {
    case IB_FTYPE_BYTESTR: {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            goto error;
        }
        *dst = (const char *)ib_bytestr_const_ptr(bs);
        *dst_length = ib_bytestr_length(bs);
        if (*dst == NULL) {
            *dst = "NULL";
            *dst_length = sizeof("NULL");
        }
        return;
    }
    case IB_FTYPE_NUM: {
        int printed;
        ib_num_t n;

        rc = ib_field_value(field, ib_ftype_num_out(&n));
        if (rc != IB_OK) {
            goto error;
        }

        local_dst = ib_mpool_alloc(mp, c_numeric_width);
        if (local_dst == NULL) {
            goto error;
        }
        printed = snprintf(local_dst, c_numeric_width, "%" PRIu64, n);
        if (printed < 0) {
            goto error;
        }
        *dst = local_dst;
        *dst_length = printed;
        return;
    }
    case IB_FTYPE_FLOAT: {
        int printed;
        ib_float_t n;

        rc = ib_field_value(field, ib_ftype_float_out(&n));
        if (rc != IB_OK) {
            goto error;
        }

        local_dst = ib_mpool_alloc(mp, c_numeric_width);
        if (local_dst == NULL) {
            goto error;
        }
        printed = snprintf(local_dst, c_numeric_width, "%Lf", n);
        if (printed < 0) {
            goto error;
        }
        *dst = local_dst;
        *dst_length = printed;
        return;
    }
    default:
        *dst        = "UNSUPPORTED";
        *dst_length = sizeof("UNSUPPORTED");
        return;
    }

    return;

error:
    *dst = "ERROR";
    *dst_length = sizeof("ERROR");
}

bool find_expand_string(
    const char **a,
    const char **b,
    const char  *s,
    size_t       l
)
{
    assert(a != NULL);
    assert(b != NULL);
    assert(s != NULL);

    const char *la = s;
    const char *lb;
    while (la < s + l) {
        la = memchr(la, '%', l - (la - s));
        if (la == NULL) {
            return false;
        }
        if (la < s + l && *(la + 1) == '{') {
            break;
        }
    }
    /* %{ at end of string leaves no room for }; hence the '- 1' */
    if (la >= s + l - 1) {
        return false;
    }

    /* Current now points to % of first %{ in string. */
    lb = memchr(la + 2, '}', l - (la - s));
    if (lb == NULL) {
        return false;
    }

    *a = la;
    *b = lb;

    return true;
}

ib_status_t ib_var_expand_acquire(
    ib_var_expand_t       **expand,
    ib_mpool_t             *mp,
    const char             *str,
    size_t                  str_length,
    const ib_var_config_t  *config,
    const char            **error_message,
    int                    *error_offset
)
{
    assert(expand != NULL);
    assert(mp     != NULL);
    assert(str    != NULL);
    assert(config != NULL);

    ib_status_t rc;
    ib_var_expand_t *first = NULL;
    ib_var_expand_t **parent_next = &first;
    const char *suffix;
    const char *local_str;

    local_str = ib_mpool_memdup(mp, str, str_length);
    if (local_str == NULL) {
        return IB_EALLOC;
    }

    /* Special case empty string. */
    if (str_length == 0) {
        first = ib_mpool_calloc(mp, 1, sizeof(*first));
        if (first == NULL) {
            return IB_EALLOC;
        }

        first->prefix = local_str;
        first->prefix_length = 0;

        *expand = first;
        return IB_OK;
    }

    suffix = local_str;
    while (suffix < local_str + str_length) {
        size_t suffix_length = str_length - (suffix - local_str);
        const char *a;
        const char *b;
        ib_var_expand_t *current;
        bool found;

        /* note calloc */
        current = ib_mpool_calloc(mp, 1, sizeof(*current));
        if (current == NULL) {
            return IB_EALLOC;
        }
        *parent_next = current;
        parent_next = &(current->next);

        found = find_expand_string(&a, &b, suffix, suffix_length);
        if (! found) {
            current->prefix = suffix;
            current->prefix_length = suffix_length;
            break;
        }
        else {
            const char *target_string = a + 2;
            size_t target_string_length = b - target_string;

            if (a != suffix) {
                current->prefix = suffix;
                current->prefix_length = a - suffix;
            }

            ib_var_target_t *target;
            rc = ib_var_target_acquire_from_string(
                &target,
                mp,
                config,
                target_string,
                target_string_length,
                error_message,
                error_offset
            );
            if (rc != IB_OK) {
                return rc;
            }
            current->target = target;

            suffix = b + 1;
        }
    }

    *expand = first;

    return IB_OK;
}

ib_status_t ib_var_expand_execute(
    const ib_var_expand_t  *expand,
    const char            **dst,
    size_t                 *dst_length,
    ib_mpool_t             *mp,
    const ib_var_store_t   *store
)
{
    assert(expand     != NULL);
    assert(dst        != NULL);
    assert(dst_length != NULL);
    assert(mp         != NULL);
    assert(store      != NULL);

    ib_status_t rc;
    ib_sa_t *sa;

    /* Check for trivial case. */
    if (expand->next == NULL && expand->target == NULL) {
        *dst = expand->prefix;
        *dst_length = expand->prefix_length;
        return IB_OK;
    }

    rc = ib_sa_begin(&sa, mp);
    if (rc != IB_OK) {
        assert(rc == IB_EALLOC);
        return IB_EALLOC;
    }

    /* Construct temporary memory pool. */
    ib_mpool_t *temp_mp;
    rc = ib_mpool_create(&temp_mp, "ib_var_expand_execute", mp);
    if (rc != IB_OK) {
        assert(rc == IB_EALLOC);
        return IB_EALLOC;
    }

    for (
        const ib_var_expand_t *current = expand;
        current != NULL;
        current = current->next
    ) {
        if (current->prefix != NULL) {
            rc = ib_sa_append(sa, current->prefix, current->prefix_length);
            if (rc != IB_OK) {goto finish_ealloc;}
        }
        if (current->target != NULL) {
            const ib_list_t *result;

            rc = ib_var_target_get_const(
                current->target,
                &result,
                temp_mp,
                store
            );
            if (rc != IB_OK) {
                goto finish;
            }

            bool first = true;
            const ib_list_node_t *node;
            IB_LIST_LOOP_CONST(result, node) {
                const char *value;
                size_t value_length;
                const ib_field_t *field = ib_list_node_data_const(node);
                field_to_string(&value, &value_length, field, temp_mp);
                if (first) {
                    first = false;
                }
                else {
                    rc = ib_sa_append(sa, ", ", 2);
                    if (rc != IB_OK) {goto finish_ealloc;}
                }
                rc = ib_sa_append(sa, value, value_length);
                if (rc != IB_OK) {goto finish_ealloc;}
            }
        }
    }

    rc = ib_sa_finish(&sa, dst, dst_length, mp);
    if (rc != IB_OK) {goto finish_ealloc;}

    rc = IB_OK;
    goto finish;

finish_ealloc:
    assert(rc == IB_EALLOC);
    /* fall through */
finish:
    ib_mpool_destroy(temp_mp);
    return rc;
}

bool ib_var_expand_test(
    const char *str,
    size_t      str_length
)
{
    assert(str != NULL);

    const char *a;
    const char *b;

    return find_expand_string(&a, &b, str, str_length);
}
