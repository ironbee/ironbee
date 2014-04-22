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

#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <yajl/yajl_tree.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <assert.h>
#include <inttypes.h>

static const size_t float_buf_size = 64;

/**
 * JSON YAJL encode: Encode a list
 *
 * @param[in,out] handle YAJL generator handle
 * @param[in] name Name of @a list (or NULL)
 * @param[in] nlen Length of @a name
 * @param[in] list IronBee list to encode
 *
 * @returns IronBee status code
 */
static ib_status_t encode_list(
    yajl_gen         handle,
    const char      *name,
    size_t           nlen,
    const ib_list_t *list)
{
    ib_status_t           rc = IB_OK;
    const ib_list_node_t *node;
    yajl_gen_status       status;
    int                   errors = 0;

    /* Encode the name */
    if ( (name != NULL) && (nlen != 0) ) {
        status = yajl_gen_string(handle, (const unsigned char *)name, nlen);
        if (status != yajl_gen_status_ok) {
            return IB_EUNKNOWN;
        }
    }

    /* Encode the map start */
    status = yajl_gen_map_open(handle);
    if (status != yajl_gen_status_ok) {
        return IB_EUNKNOWN;
    }

    IB_LIST_LOOP_CONST(list, node) {
        const ib_field_t *field = (const ib_field_t *)node->data;
        ib_status_t       tmprc;

        status = yajl_gen_string(handle,
                                 (const unsigned char *)field->name,
                                 field->nlen);
        if (status != yajl_gen_status_ok) {
            rc = IB_EUNKNOWN;
            ++errors;
            continue;
        }

        switch(field->type) {
        case IB_FTYPE_LIST:
        {
            const ib_list_t *list2;
            tmprc = ib_field_value(field, ib_ftype_list_out(&list2));
            if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
                rc = tmprc;
                ++errors;
            }
            else {
                tmprc = encode_list(handle, NULL, 0, list2);
                if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
                    rc = tmprc;
                    ++errors;
                }
            }
            break;
        }

        case IB_FTYPE_NUM:
        {
            ib_num_t num;
            tmprc = ib_field_value(field, ib_ftype_num_out(&num));
            if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
                rc = tmprc;
                ++errors;
            }
            else {
                status = yajl_gen_integer(handle, num);
                if (status != yajl_gen_status_ok) {
                    if (rc != IB_OK) {
                        rc = IB_EUNKNOWN;
                    }
                    ++errors;
                }
            }
            break;
        }

        case IB_FTYPE_FLOAT:
        {
            ib_float_t fnum;
            tmprc = ib_field_value(field, ib_ftype_float_out(&fnum));
            if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
                rc = tmprc;
                ++errors;
            }
            else {
                char buf[float_buf_size+1];
                snprintf(buf, float_buf_size, "%#.8Lg", fnum);
                status = yajl_gen_number(handle, buf, strlen(buf));
                if (status != yajl_gen_status_ok) {
                    if (rc != IB_OK) {
                        rc = IB_EUNKNOWN;
                    }
                    ++errors;
                }
            }
            break;
        }

        case IB_FTYPE_NULSTR:
        {
            const char *str;
            tmprc = ib_field_value(field, ib_ftype_nulstr_out(&str));
            if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
                rc = tmprc;
                ++errors;
            }
            else if (str != NULL) {
                status = yajl_gen_string(handle,
                                         (unsigned char *)str,
                                         strlen(str));
                if (status != yajl_gen_status_ok) {
                    if (rc != IB_OK) {
                        rc = IB_EUNKNOWN;
                    }
                    ++errors;
                }
            }
            break;
        }

        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *bs;
            tmprc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
            if ( (tmprc != IB_OK) && (rc == IB_OK) ) {
                rc = tmprc;
                ++errors;
            }
            else if (bs != NULL) {
                status = yajl_gen_string(handle,
                                         ib_bytestr_const_ptr(bs),
                                         ib_bytestr_length(bs));
                if (status != yajl_gen_status_ok) {
                    if (rc != IB_OK) {
                        rc = IB_EUNKNOWN;
                    }
                    ++errors;
                }
            }
            break;
        }

        default: /* Just ignore it */
            break;

        } /* switch(f->type) */
    }

    /* Encode the map end */
    status = yajl_gen_map_close(handle);
    if (status != yajl_gen_status_ok) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

/* Encode an IB list into JSON */
ib_status_t ib_json_encode(
    ib_mm_t           mm,
    const ib_list_t  *list,
    bool              pretty,
    char            **obuf,
    size_t           *olen)
{
    assert(list != NULL);
    assert(obuf != NULL);

    yajl_gen handle;
    json_yajl_alloc_context_t alloc_ctx = { mm, IB_OK };
    ib_status_t rc;
    yajl_gen_status status;
    yajl_alloc_funcs alloc_fns = {
        json_yajl_alloc,
        json_yajl_realloc,
        json_yajl_free,
        &alloc_ctx
    };

    handle = yajl_gen_alloc(&alloc_fns);
    /* Probably should validate the handle here, but it's not clear from the
     * YAJL documentation how to do that */

    /* Set pretty option */
    if (pretty) {
        int opt = yajl_gen_config(handle, yajl_gen_beautify);
        if (opt == 0) {
            return IB_EINVAL;
        }
    }

    rc = encode_list(handle, NULL, 0, list);
    if (rc != IB_OK) {
        return rc;
    }
    status = yajl_gen_get_buf(handle, (const unsigned char **)obuf, olen);
    if (status != yajl_gen_status_ok) {
        return IB_EUNKNOWN;
    }

    return rc;
}
