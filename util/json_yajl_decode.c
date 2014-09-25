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
 * @brief IronBee --- JSON YAJL wrapper functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "json_yajl_private.h"

#include <ironbee/field.h>
#include <ironbee/json.h>
#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/string.h>
#include <ironbee/types.h>
#include <ironbee/util.h>

#include <yajl/yajl_parse.h>
#ifdef __clang__
#pragma clang diagnostic push
#endif
#include <yajl/yajl_tree.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <assert.h>
#include <inttypes.h>

/** A single decoding stack frame */
typedef struct {
    ib_list_t   *list;           /**< The list to decode into */
    bool         keyed;          /**< Is the list keyed (i.e. a "map")? */
} decode_stack_frame_t;

/** Decode context */
typedef struct {
    ib_list_t                 *list;       /**< The primary list */
    decode_stack_frame_t      *cur;        /**< Current stack frame */
    ib_list_t                 *stack;      /**< List <decode_stack_frame_t*> */
    json_yajl_alloc_context_t *alloc_ctx;  /**< Allocation context */
    ib_status_t                status;     /**< IB status for return errors */
    const char                *field_name;     /**< Name of next field */
    size_t                     field_name_len; /**< Length of field name */
} decode_ctx_t;

/* JSON decoders */
static int decode_null(void *ctx)
{
    return 1;
}

static int decode_boolean(void *ctx,
                          int value)
{
    return 1;
}

static int decode_integer(void *ctx,
                          long long value)
{
    return 1;
}

static int decode_double(void *ctx,
                         double value)
{
    return 1;
}

static ib_status_t gen_field_name(decode_ctx_t *decode,
                                  char *buf,
                                  size_t buflen,
                                  const char **name,
                                  size_t *namelen)
{
    bool no_name = ( (decode->field_name == NULL) ||
                     (decode->field_name_len == 0) );

    if (decode->cur->keyed) {
        if (no_name) {
            return IB_EINVAL;
        }
        *name = decode->field_name;
        *namelen = decode->field_name_len;
    }
    else {
        if (! no_name) {
            return IB_EINVAL;
        }
        snprintf(buf, buflen, "%zd", ib_list_elements(decode->cur->list));
        *name = buf;
        *namelen = strlen(buf);
    }
    return IB_OK;
}

static size_t namebuf_len = 16;
static int decode_number(void *ctx,
                         const char *s,
                         size_t len)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    ib_field_t *field;
    ib_status_t rc;
    char namebuf[namebuf_len+1];
    const char *name;
    size_t nlen;

    rc = gen_field_name(decode, namebuf, namebuf_len, &name, &nlen);
    if (rc != IB_OK) {
        goto cleanup;
    }
    rc = ib_field_from_string_ex(decode->alloc_ctx->mm,
                                 name, nlen, s, len,
                                 &field);
    if (rc != IB_OK) {
        goto cleanup;
    }
    rc = ib_list_push(decode->cur->list, field);
    if (rc != IB_OK) {
        goto cleanup;
    }

cleanup:
    decode->field_name = NULL;
    decode->field_name_len = 0;
    if (rc != IB_OK) {
        if (decode->status == IB_OK) {
            decode->status = rc;
        }
        return 0;
    }
    return 1;
}

static int decode_string(void *ctx,
                         const unsigned char *s,
                         size_t len)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    ib_field_t *field;
    ib_bytestr_t *bs;
    ib_status_t rc;
    char namebuf[namebuf_len+1];
    const char *name;
    size_t nlen;

    rc = gen_field_name(decode, namebuf, namebuf_len, &name, &nlen);
    if (rc != IB_OK) {
        goto cleanup;
    }
    rc = ib_bytestr_dup_mem(&bs, decode->alloc_ctx->mm, s, len);
    if (rc != IB_OK) {
        goto cleanup;
    }
    rc = ib_field_create(&field, decode->alloc_ctx->mm,
                         name, nlen,
                         IB_FTYPE_BYTESTR,
                         ib_ftype_bytestr_in(bs));
    if (rc != IB_OK) {
        goto cleanup;
    }
    rc = ib_list_push(decode->cur->list, field);
    if (rc != IB_OK) {
        goto cleanup;
    }

cleanup:
    decode->field_name = NULL;
    decode->field_name_len = 0;
    if (rc != IB_OK) {
        if (decode->status == IB_OK) {
            decode->status = rc;
        }
        return 0;
    }
    return 1;
}

static int decode_map_key(void *ctx,
                          const unsigned char *s,
                          size_t len)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    decode->field_name = (const char *)s;
    decode->field_name_len = len;

    return 1;
}

static int decode_start_list(decode_ctx_t *decode,
                             bool keyed)
{
    decode_stack_frame_t *frame;
    decode_stack_frame_t *cur;
    ib_list_t *list;
    ib_status_t rc;

    frame = ib_mm_alloc(decode->alloc_ctx->mm, sizeof(*frame));
    if (frame == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    if (decode->cur == NULL) {
        list = decode->list;
        cur = frame;
    }
    else {
        ib_field_t *field = NULL;
        char namebuf[namebuf_len+1];
        const char *name;
        size_t nlen;

        cur = decode->cur;
        rc = gen_field_name(decode, namebuf, namebuf_len, &name, &nlen);
        if (rc != IB_OK) {
            goto cleanup;
        }
        rc = ib_list_create(&list, decode->alloc_ctx->mm);
        if (rc != IB_OK) {
            goto cleanup;
        }
        rc = ib_field_create(&field, decode->alloc_ctx->mm,
                             decode->field_name, decode->field_name_len,
                             IB_FTYPE_LIST, ib_ftype_list_in(list));
        if (rc != IB_OK) {
            goto cleanup;
        }
        rc = ib_list_push(decode->cur->list, field);
        if (rc != IB_OK) {
            goto cleanup;
        }
    }

    /* Advance the stack frame *after* all of the above */
    rc = ib_list_push(decode->stack, cur);
    if (rc != IB_OK) {
        goto cleanup;
    }
    frame->list = list;
    frame->keyed = keyed;
    decode->cur = frame;

cleanup:
    decode->field_name = NULL;
    decode->field_name_len = 0;
    if (rc != IB_OK) {
        if (decode->status == IB_OK) {
            decode->status = rc;
        }
        return 0;
    }
    return 1;
}

static int decode_end_list(void *ctx,
                           bool keyed)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    decode_stack_frame_t *frame;
    ib_status_t rc;

    if (decode->cur->keyed != keyed) {
        rc = IB_EINVAL;
        goto cleanup;
    }
    rc = ib_list_pop(decode->stack, &frame);
    if (rc != IB_OK) {
        goto cleanup;
    }
    decode->cur = frame;

cleanup:
    if (rc != IB_OK) {
        if (decode->status == IB_OK) {
            decode->status = rc;
        }
        return 0;
    }
    return 1;
}

static int decode_start_map(void *ctx)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    return decode_start_list(decode, true);
}

static int decode_end_map(void *ctx)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    return decode_end_list(decode, true);
}

static int decode_start_array(void *ctx)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    return decode_start_list(decode, false);
}

static int decode_end_array(void *ctx)
{
    decode_ctx_t *decode = (decode_ctx_t *)ctx;
    return decode_end_list(decode, false);
}

static yajl_callbacks decode_fns =
{
    decode_null,
    decode_boolean,
    decode_integer,
    decode_double,
    decode_number,
    decode_string,
    decode_start_map,
    decode_map_key,
    decode_end_map,
    decode_start_array,
    decode_end_array
};

/* Decode a JSON buffer, extended version */
ib_status_t ib_json_decode_ex(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    ib_list_t      *list_out,
    const char    **error
)
{
    assert(list_out != NULL);

    yajl_handle handle;
    json_yajl_alloc_context_t alloc_ctx = { mm, IB_OK };
    decode_ctx_t decode_ctx;
    yajl_status status;
    ib_status_t rc;
    yajl_alloc_funcs alloc_fns = {
        json_yajl_alloc,
        json_yajl_realloc,
        json_yajl_free,
        &alloc_ctx
    };

    if (error != NULL) {
        *error = NULL;
    }

    /* No data?  Do nothing */
    if ( (data_in == NULL) || (dlen_in == 0) ) {
        return IB_OK;
    }

    rc = ib_list_create(&(decode_ctx.stack), mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* We start with no stack frame; it's created with the first map start */
    decode_ctx.list = list_out;
    decode_ctx.cur = NULL;
    decode_ctx.alloc_ctx = &alloc_ctx;
    decode_ctx.status = IB_OK;
    decode_ctx.field_name = NULL;
    decode_ctx.field_name_len = 0;

    handle = yajl_alloc(&decode_fns, &alloc_fns, &decode_ctx);
    /* Probably should validate the handle here, but it's not clear from the
     * YAJL documentation how to do that */

    /* Parse the buffer */
    status = yajl_parse(handle, data_in, dlen_in);
    if (status != yajl_status_ok) {
        goto parse_error;
    }

    /* Tell yajl that we've hit EOB */
    status = yajl_complete_parse(handle);

    if (status != yajl_status_ok) {
        goto parse_error;
    }

    /* Done */
    return decode_ctx.status;

parse_error:
    if (error != NULL) {
        unsigned char *err = yajl_get_error(handle, 1, data_in, dlen_in);
        if (err != NULL) {
            *error = ib_mm_strdup(mm, (char *)err);
            yajl_free_error(handle, err);
        }
    }

    if (decode_ctx.status != IB_OK) {
        return decode_ctx.status;
    }
    else {
        return IB_EINVAL;
    }
}

/* Decode a JSON buffer */
ib_status_t ib_json_decode(
    ib_mm_t      mm,
    const char  *in,
    ib_list_t   *list_out,
    const char **error
)
{
    assert(in != NULL);
    assert(list_out != NULL);
    assert(error != NULL);

    return ib_json_decode_ex(mm,
                             (const uint8_t *)in,
                             strlen(in),
                             list_out, error);
}
