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
 * @brief IronBee --- nginx 1.3 module
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include <assert.h>
#include <ctype.h>

#include <ironbee/config.h>
#include <ironbee/context.h>
#include <ironbee/engine_manager.h>
#include <ironbee/state_notify.h>
#include <ironbee/util.h>
#include <ironbee/module.h>

#include "ngx_ironbee.h"


/**
 * Log a message to the server plugin.
 *
 * @param[in] ib_logger The IronBee logger.
 * @param[in] rec The record to use in logging.
 * @param[in] log_msg The user's log message.
 * @param[in] log_msg_sz The user's log message size.
 * @param[out] writer_record Unused. We always return IB_DECLINED.
 * @param[in] cbdata The server plugin module data used for logging.
 *
 * @returns
 * - IB_DECLINED when everything goes well.
 * - IB_OK is not returned.
 * - Other on error.
 */
static ib_status_t logger_format(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *cbdata
)
{
    assert(logger != NULL);
    assert(rec != NULL);
    assert(log_msg != NULL);
    assert(cbdata != NULL);

    module_data_t            *mod_data = (module_data_t *)cbdata;
    unsigned int              ngx_level;
    ib_status_t               rc;
    ib_logger_standard_msg_t *std_msg;

    if (!mod_data->ib_log_active) {
        return IB_OK;
    }

    /* Translate the log level. */
    switch (rec->level) {
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

    rc = ib_logger_standard_formatter_notime(
          logger,
          rec,
          log_msg,
          log_msg_sz,
          &std_msg,
          NULL);
    if (rc != IB_OK) {
        return rc;
    }

    if (rec->conn != NULL) {
        ngx_connection_t *conn = (ngx_connection_t *)(rec->conn->server_ctx);
        ngx_log_error(ngx_level, conn->log, 0, "ironbee: %s %.*s",
                      std_msg->prefix, (int)std_msg->msg_sz,
                      (const char *)std_msg->msg);
    }
    else {
        ngx_log_error(ngx_level, mod_data->log, 0, "ironbee: %s %.*s",
                      std_msg->prefix, (int)std_msg->msg_sz,
                      (const char *)std_msg->msg);
    }

    ib_logger_standard_msg_free(logger, std_msg, cbdata);

    /* since we do all the work here, signal the logger to not
     * use the record function. */
    return IB_DECLINED;
}

/**
 * Initialize a new server plugin module instance.
 *
 * @param[in] ib Engine this module is operating on.
 * @param[in] module This module structure.
 * @param[in] cbdata The server plugin module data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t init_module(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata
)
{
    ib_status_t rc;
    assert(ib != NULL);
    assert(module != NULL);
    assert(cbdata != NULL);

    module_data_t *mod_data = (module_data_t *)cbdata;
    ib_logger_format_t *logger;
    rc = ib_logger_format_create(
        ib_engine_logger_get(ib),
        &logger,
        logger_format,
        mod_data,
        NULL,
        NULL);
    if (rc != IB_OK) {
        return IB2NG(rc);
    }

    ib_logger_writer_add(
        ib_engine_logger_get(ib),
        NULL,                      /* Open. */
        NULL,                      /* Callback data. */
        NULL,                      /* Close. */
        mod_data,                  /* Callback data. */
        NULL,                      /* Reopen. */
        NULL,                      /* Callback data. */
        logger,                    /* Format - This does all the work. */
        NULL,                      /* Record. */
        NULL                       /* Callback data. */
    );

    return IB_OK;
}
/**
 * Create a new module to be registered with @a ib.
 *
 * This is pre-configuration time so directives may be registered.
 *
 * @param[out] module Module created using ib_module_create() and
 *             properly initialized. This should not be
 *             passed to ib_module_init(), the manager will do that.
 * @param[in] ib The unconfigured engine this module will be
 *            initialized in.
 * @param[in] cbdata The server plugin data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINED Is never returned.
 * - Other on fatal errors.
 */
ib_status_t ngxib_module(
    ib_module_t **module,
    ib_engine_t *ib,
    void *cbdata
)
{
    assert(module != NULL);
    assert(ib != NULL);
    assert(cbdata != NULL);

    ib_status_t    rc;
    module_data_t *mod_data = (module_data_t *)cbdata;

    rc = ib_module_create(module, ib);
    if (rc != IB_OK) {
        return rc;
    }

    IB_MODULE_INIT_DYNAMIC(
      *module,
      __FILE__,
      NULL,                  /* Module data */
      ib,                    /* Engine. */
      "nginxModule",         /* Module name. */
      NULL,                  /* Config struct. */
      0,                     /* Config size. */
      NULL,                  /* Config copy function. */
      NULL,                  /* Config copy function callback data. */
      NULL,                  /* Configuration field map. */
      NULL,                  /* Configuration directive map. */
      init_module,           /* Init function. */
      mod_data,              /* Init function callback data. */
      NULL,                  /* Finish function. */
      NULL                   /* Finish function callback data. */
    );

    return IB_OK;
}
