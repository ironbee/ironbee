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
 * @brief IronBee --- nginx 1.3 module - ironbee logging
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include "ngx_ironbee.h"
#include <assert.h>


/**
 * IronBee logging workaround.
 *
 * nginx requires an ngx_log_t argument, but the ironbee API doesn't
 * support passing it.  So we set it before any call that might
 * generate ironbee log messages.
 * Note, there are no threads to worry about, but re-entrancy could
 * be an issue.
 *
 * @param[in] log The ngx log pointer
 * @return The previous value of the log pointer
 */
static ngx_log_t *ngx_log = NULL;
ngx_log_t *ngxib_log(ngx_log_t *log)
{
    ngx_log_t *tmp = ngx_log;
    ngx_log = log;
    return tmp;
}

void ngxib_logger(
    ib_log_level_t  level,
    void           *cbdata,
    const char     *buf)
{
    unsigned int ngx_level = NGX_LOG_WARN;

    /* Translate the log level. */
    switch (level) {
    case IB_LOG_EMERGENCY:
        ngx_level = NGX_LOG_EMERG;
        break;
    case IB_LOG_ALERT:
        ngx_level = NGX_LOG_ALERT;
        break;
    case IB_LOG_CRITICAL:
        ngx_level = NGX_LOG_CRIT;
        break;
    case IB_LOG_ERROR:
        ngx_level = NGX_LOG_ERR;
        break;
    case IB_LOG_WARNING:
        ngx_level = NGX_LOG_WARN;
        break;
    case IB_LOG_NOTICE:
        ngx_level = NGX_LOG_NOTICE;
        break;
    case IB_LOG_INFO:
        ngx_level = NGX_LOG_INFO;
        break;
    case IB_LOG_DEBUG:
    case IB_LOG_DEBUG2:
    case IB_LOG_DEBUG3:
    case IB_LOG_TRACE:
    default:
        ngx_level = NGX_LOG_DEBUG;
        break;
    }

    /// @todo Make configurable
    if (ngx_level > NGX_LOG_NOTICE) {
        ngx_level = NGX_LOG_NOTICE;
    }

    /* Write it to the error log. */
    ngx_log_error(ngx_level, ngx_log, 0, "ironbee: %s", buf);
}
