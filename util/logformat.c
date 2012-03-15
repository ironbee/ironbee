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
 * @brief IronBee - Utility Log Format helper functions
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

/**
 * @internal
 *
 * Logformat helper for custom index file formats
 */

#include "ironbee_config_auto.h"

#include <ironbee/logformat.h>

#include <ironbee/debug.h>

#include "ironbee_util_private.h"

enum {
    IB_LOGFORMAT_ST_NONE,
    IB_LOGFORMAT_ST_PR,
    IB_LOGFORMAT_ST_BS
};

ib_status_t ib_logformat_create(ib_mpool_t *mp, ib_logformat_t **lf) {
    IB_FTRACE_INIT();
    if (lf == NULL) {
        return IB_EINVAL;
    }

    *lf = (ib_logformat_t *)ib_mpool_calloc(mp, 1, sizeof(ib_logformat_t) + 1);
    if (*lf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memset(*lf, 0, sizeof(ib_logformat_t) + 1);
    (*lf)->mp = mp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_logformat_set(ib_logformat_t *lf, const char *format) {
    IB_FTRACE_INIT();
    ib_mpool_t *mp = lf->mp;
    char literal[IB_LOGFORMAT_MAXLINELEN + 1];
    int literal_tot = 0;
    uint8_t status = 0;
    int i = 0;
    int j = 0;
    int l = 0;

    memset(lf, 0, sizeof(ib_logformat_t) + 1);
    lf->mp = mp;

    literal[0] = '\0';

    /* Store the original format (right now its just for debugging purposes) */
    lf->orig_format = ib_mpool_strdup(lf->mp, format);
    if (lf->orig_format == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    for (; lf->orig_format[i] != '\0' &&
         lf->field_cnt < IB_LOGFORMAT_MAXFIELDS &&
         j < IB_LOGFORMAT_MAXLINELEN; i++)
    {
        if (i == 0 && lf->orig_format[i] != '%') {
            lf->literal_starts = 1;
        }
        switch (status) {
            case IB_LOGFORMAT_ST_PR:
                /* Which field? */
                switch (lf->orig_format[i]) {
                    case IB_LOG_FIELD_REMOTE_ADDR:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_REMOTE_ADDR;
                        break;
                    case IB_LOG_FIELD_LOCAL_ADDR:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_LOCAL_ADDR;
                        break;
                    case IB_LOG_FIELD_HOSTNAME:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_HOSTNAME;
                        break;
                    case IB_LOG_FIELD_SITE_ID:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_SITE_ID;
                        break;
                    case IB_LOG_FIELD_SENSOR_ID:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_SENSOR_ID;
                        break;
                    case IB_LOG_FIELD_TRANSACTION_ID:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_TRANSACTION_ID;
                        break;
                    case IB_LOG_FIELD_TIMESTAMP:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_TIMESTAMP;
                        break;
                    case IB_LOG_FIELD_LOG_FILE:
                        lf->fields[lf->field_cnt++] = IB_LOG_FIELD_LOG_FILE;
                        break;
                    case '%':
                        if (i == 1) {
                            lf->literal_starts = 1;
                        }
                        /* just allow it */
                        literal[j++] = '%';
                        break;
                    default:
                        /* Not understood - ignore it */
                        break;
                }

                if (literal[0] != '\0') {
                    literal_tot += j;
                    if (literal_tot > IB_LOGFORMAT_MAXLINELEN) {
                        /* Too long */
                        IB_FTRACE_RET_STATUS(IB_EINVAL);
                    }

                    /* Add string end for later usage with *printf() functions*/
                    literal[j] = '\0';
                    lf->literals[l] = ib_mpool_strdup(lf->mp, literal);
                    if (lf->literals[l] == NULL) {
                        /* Oops.. */
                        IB_FTRACE_RET_STATUS(IB_EINVAL);
                    }
                    else {
                        lf->literals_len[l] = j;
                        ++l;
                    }
                    literal[0] = '\0';
                    j = 0;
                }
                status = IB_LOGFORMAT_ST_NONE;
                break;
            case IB_LOGFORMAT_ST_BS:
                /* Avoid '\b', '\n' */
                switch (lf->orig_format[i]) {
                    case 't':
                        literal[j++] = '\t';
                        break;
                    case 'n':
                    case 'b':
                        /* Just add a space */
                        /// @todo more to add?
                        literal[j++] = ' ';
                        break;
                    default:
                        /* Just use the character directly */
                        literal[j++] = lf->orig_format[i];
                }
                status = IB_LOGFORMAT_ST_NONE;
                break;
            case IB_LOGFORMAT_ST_NONE:
            default:
                switch (lf->orig_format[i]) {
                    /** @todo Do we need to check certain escape chars?
                     *  Will we allow for example '\n' in the log index file?
                     */
                    case '\\':
                        status = IB_LOGFORMAT_ST_BS;
                        break;
                    case '%':
                        status = IB_LOGFORMAT_ST_PR;
                        break;
                    default:
                        /* literal string */
                        literal[j++] = lf->orig_format[i];
                }
        }
    }

    if (lf->orig_format[i] != '\0' || status == IB_LOGFORMAT_ST_PR) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    lf->fields[lf->field_cnt + 1] = '\0';
    /* Write the last parsed literal and the length */
    if (literal[0] != '\0') {
        literal_tot += j;
        if (literal_tot > IB_LOGFORMAT_MAXLINELEN) {
            /* Too long */
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        literal[j] = '\0';
        lf->literals_len[l] = j;
        lf->literals[l++] = ib_mpool_strdup(lf->mp, literal);
    }
    lf->literal_cnt = l;

    IB_FTRACE_RET_STATUS(IB_OK);
}
