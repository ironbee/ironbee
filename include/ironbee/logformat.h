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

#ifndef _IB_LOGFORMAT_H_
#define _IB_LOGFORMAT_H_

/**
 * @file
 * @brief IronBee - Log Format Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilLogformat Log Format
 * @ingroup IronBeeUtil
 * @{
 */
#define IB_LOGFORMAT_MAXFIELDS 128
#define IB_LOGFORMAT_MAXLINELEN 8192
#define IB_LOGFORMAT_DEFAULT ((char*)"%T %h %a %S %s %t %f")

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

struct ib_logformat_t {
    ib_mpool_t *mp;
    char *orig_format;
    uint8_t literal_starts;

    /* We could use here an ib_list, but this will is faster */
    char fields[IB_LOGFORMAT_MAXFIELDS];     /**< Used to hold the field list */
    char *literals[IB_LOGFORMAT_MAXFIELDS + 2]; /**< Used to hold the list of
                                                   literal strings at the start,
                                                 end, and between fields */
    int literals_len[IB_LOGFORMAT_MAXFIELDS + 2]; /**< Used to hold the sizes of
                                                       literal strings */
    uint8_t field_cnt;   /**< Fields count */
    uint8_t literal_cnt; /**< Literals count */
};

/**
 * Creates a logformat helper
 *
 * @param mp memory pool to use
 * @param lf reference pointer where the new instance will be stored
 *
 * @returns Status code
 */
ib_status_t ib_logformat_create(ib_mpool_t *mp, ib_logformat_t **lf);

/**
 * Used to parse and store the specified format
 *
 * @param mp memory pool to use
 * @param lf pointer to the logformat helper
 * @param format string with the format to process
 *
 * @returns Status code
 */
ib_status_t ib_logformat_set(ib_logformat_t *lf, char *format);
/** @} IronBeeUtilLogformat */


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_LOGFORMAT_H_ */
