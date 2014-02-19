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

#ifndef _IB_LOGFORMAT_H_
#define _IB_LOGFORMAT_H_

/**
 * @file
 * @brief IronBee --- Log Format Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilLogformat AuditLog Format
 * @ingroup IronBeeUtil
 *
 * Code related to the format of the audit log.
 *
 * @{
 */
#define IB_LOGFORMAT_DEFAULT "%T %h %a %S %s %t %f"

typedef struct ib_logformat_t ib_logformat_t;

/* fields */
#define IB_LOG_FIELD_REMOTE_ADDR        'a'
#define IB_LOG_FIELD_LOCAL_ADDR         'A'
#define IB_LOG_FIELD_HOSTNAME           'h'
#define IB_LOG_FIELD_SITE_ID            's'
#define IB_LOG_FIELD_SENSOR_ID          'S'
#define IB_LOG_FIELD_TRANSACTION_ID     't'
#define IB_LOG_FIELD_TIMESTAMP          'T'
#define IB_LOG_FIELD_LOG_FILE           'f'

typedef enum {
    item_type_literal,              /* String literal */
    item_type_format                /* Format character */
} ib_logformat_itype_t;

typedef struct ib_logformat_field_t {
    char         fchar;
} ib_logformat_field_t;

#define IB_LOGFORMAT_MAX_SHORT_LITERAL (sizeof(char *) - 1)
typedef struct ib_logformat_literal_t {
    union {
        char   short_str[IB_LOGFORMAT_MAX_SHORT_LITERAL + 1];
        char  *str;
    } buf;
    size_t     len;
} ib_logformat_literal_t;

typedef struct ib_logformat_item_t {
    ib_logformat_itype_t  itype;    /* Item type */
    union {
        ib_logformat_field_t   field;
        ib_logformat_literal_t literal;
    } item;
} ib_logformat_item_t;

struct ib_logformat_t {
    ib_mm_t      mm;
    char        *format;
    ib_list_t   *items;          /* List of pointers to ib_logformat_item_t */
};

/**
 * Callback function to get an individual data item
 *
 * @param[in] lf Logformat data
 * @param[in] field Data on the field to get
 * @param[in] cbdata Callback-specific data
 * @param[out] str String representation of the item to add to line
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_logformat_fn_t)(
    const ib_logformat_t        *lf,
    const ib_logformat_field_t  *field,
    const void                  *cbdata,
    const char                 **str);

/**
 * Creates a logformat helper
 *
 * @param mm memory manager to use
 * @param lf reference pointer where the new instance will be stored
 *
 * @returns Status code
 */
ib_status_t ib_logformat_create(ib_mm_t mm, ib_logformat_t **lf);

/**
 * Used to parse and store the specified format
 *
 * @param lf pointer to the logformat helper
 * @param format string with the format to process
 *
 * @returns Status code
 */
ib_status_t ib_logformat_parse(ib_logformat_t *lf, const char *format);

/**
 * Used to format a parsed logformat line.
 *
 * @param[in] lf Pointer to the logformat helper
 * @param[in,out] line Line buffer
 * @param[in] line_size Size of @a line
 * @param[out] line_len Length of data written to @a line_size
 * @param[in] fn Callback function to fill get an individual field
 * @param[in] fndata Callback specific data
 *
 * @returns Status code
 */
ib_status_t ib_logformat_format(const ib_logformat_t *lf,
                                char *line,
                                size_t line_size,
                                size_t *line_len,
                                ib_logformat_fn_t fn,
                                void *fndata);

/** @} IronBeeUtilLogformat */


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_LOGFORMAT_H_ */
