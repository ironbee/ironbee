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

/**
 * IronBee logger function.
 *
 * Performs IronBee logging for the nginx module.
 *
 * @param[in] ib IronBee engine
 * @param[in] level Debug level
 * @param[in] file File name
 * @param[in] line Line number
 * @param[in] fmt Format string
 * @param[in] ap Var args list to match the format
 * @param[in] dummy Dummy pointer
 */
void ngxib_logger(const ib_engine_t *ib,
                  ib_log_level_t level,
                  const char *file,
                  int line,
                  const char *fmt,
                  va_list ap,
                  void *dummy)
{
    char buf[8192 + 1];
    int limit = 7000;
    unsigned int ngx_level = NGX_LOG_WARN;
    int ec;

    assert(ngx_log != NULL);

    /* Buffer the log line. */
    ec = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (ec >= limit) {
        /* Mark as truncated, with a " ...". */
        memcpy(buf + (limit - 5), " ...", 5);

        /// @todo Do something about it
        ngx_log_error(ngx_level, ngx_log, 0,
                      "Ironbee: Log format truncated: limit (%d/%d)",
                      (int)ec, limit);
    }

    /* Translate the log level. */
    switch (level) {
        case 0:
            ngx_level = NGX_LOG_EMERG;
            break;
        case 1:
            ngx_level = NGX_LOG_ALERT;
            break;
        case 2:
            ngx_level = NGX_LOG_ERR;
            break;
        case 3:
            ngx_level = NGX_LOG_WARN;
            break;
        case 4:
            ngx_level = NGX_LOG_DEBUG; /// @todo For now, so we get file/line
            break;
        case 9:
            ngx_level = NGX_LOG_DEBUG;
            break;
        default:
            ngx_level = NGX_LOG_DEBUG; /// @todo Make configurable
            break;
    }

    /// @todo Make configurable
    if (ngx_level > NGX_LOG_NOTICE) {
        ngx_level = NGX_LOG_NOTICE;
    }

    /* Write it to the error log. */
    ngx_log_error(ngx_level, ngx_log, 0, "ironbee: %s", buf);
}
/**
 * Log level callback.  Currently fixed.
 *
 * @param[in] ib     IronBee engine.
 * @param[in] cbdata Callback data.
 * @returns log level.
 */
ib_log_level_t ngxib_loglevel(const ib_engine_t *ib, void *cbdata)
{
    return IB_LOG_WARNING;
}

#if 0
IB_PROVIDER_IFACE_TYPE(logger) *ngxib_logger_iface(void)
{
    static IB_PROVIDER_IFACE_TYPE(logger) ironbee_logger_iface = {
        IB_PROVIDER_IFACE_HEADER_DEFAULTS,
        ironbee_logger
    };
    return &ironbee_logger_iface;
}
#endif
