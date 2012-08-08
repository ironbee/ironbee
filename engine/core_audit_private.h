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
 * @brief IronBee -- Core Audit Related Routines
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef _IB_CORE_AUDIT_PRIVATE_H_
#define _IB_CORE_AUDIT_PRIVATE_H_

#include <ironbee/core.h>

#include <stdio.h>

/* -- Audit Provider -- */

/* Forward define this structure. */
typedef struct core_audit_cfg_t core_audit_cfg_t;

/**
 * Core audit configuration structure
 */
struct core_audit_cfg_t {
    FILE           *index_fp;       /**< Index file pointer */
    FILE           *fp;             /**< Audit log file pointer */
    const char     *fn;             /**< Audit log file name */
    const char     *full_path;      /**< Audit log full path */
    const char     *temp_path;      /**< Full path to temporary filename */
    int             parts_written;  /**< Parts written so far */
    const char     *boundary;       /**< Audit log boundary */
    ib_tx_t        *tx;             /**< Transaction being logged */
};

/**
 * Set cfg->fn to the file name and cfg->fp to the FILE* of the audit log.
 *
 * @param[in] lpi Log provider instance.
 * @param[in] log Audit Log that will be written. Contains the context and
 *            other information.
 * @param[in] cfg The configuration.
 * @param[in] corecfg The core configuration.
 */
ib_status_t core_audit_open_auditfile(ib_provider_inst_t *lpi,
                                      ib_auditlog_t *log,
                                      core_audit_cfg_t *cfg,
                                      ib_core_cfg_t *corecfg);

ib_status_t core_audit_open_auditindexfile(ib_provider_inst_t *lpi,
                                           ib_auditlog_t *log,
                                           core_audit_cfg_t *cfg,
                                           ib_core_cfg_t *corecfg);

/**
 * If required, open the log files.
 *
 * There are two files opened. One is a single file to store the audit log.
 * The other is the shared audit log index file. This index file is
 * protected by a lock during open and close calls but not writes.
 *
 * This and core_audit_close are thread-safe.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] log The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
ib_status_t core_audit_open(ib_provider_inst_t *lpi,
                            ib_auditlog_t *log);

/**
 * Write audit log header. This is not thread-safe and should be protected
 * with a lock.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] log The log record.
 * @return IB_OK or IB_EUNKNOWN.
 */
ib_status_t core_audit_write_header(ib_provider_inst_t *lpi,
                                    ib_auditlog_t *log);

/**
 * Write part of a audit log. This call should be protected by a lock.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] part The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
ib_status_t core_audit_write_part(ib_provider_inst_t *lpi,
                                  ib_auditlog_part_t *part);

/**
 * Write an audit log footer. This call should be protected by a lock.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] log The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
ib_status_t core_audit_write_footer(ib_provider_inst_t *lpi,
                                    ib_auditlog_t *log);

/**
 * Render the log index line. Line must have a size of
 * IB_LOGFORMAT_MAX_INDEX_LENGTH + 1
 *
 * @param lpi provider instance
 * @param log audit log instance
 * @param line buffer to store the line before writing to disk/pipe.
 * @param line_size Size of @a line.
 *
 * @returns Status code
 */
ib_status_t core_audit_get_index_line(ib_provider_inst_t *lpi,
                                      ib_auditlog_t *log,
                                      char *line,
                                      int *line_size);

ib_status_t core_audit_close(ib_provider_inst_t *lpi, ib_auditlog_t *log);

#endif // _IB_CORE_AUDIT_PRIVATE_H_
