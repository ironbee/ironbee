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

/**
 * AuditLog format version number.
 *
 * This number must be incremented each time the format changes.
 *
 * Format is a decimal number based on date the format changed: @e YYYYMMDDn
 * - @e YYYY: 4-digit year
 * - @e   MM: 2-digit month
 * - @e   DD: 2-digit day
 * - @e    n: Revision number if changes more than once in a day (default=0)
 *
 * @note: Update ironbeepp/abi_compatibility to match
 */
#define IB_AUDITLOG_VERSION 201212210

/**
 * Set cfg->fn to the file name and cfg->fp to the FILE* of the audit log.
 *
 * @param[in] ib IronBee engine.
 * @param[in] log Audit Log that will be written. Contains the context and
 *            other information.
 * @param[in] cfg The configuration.
 * @param[in] corecfg The core configuration.
 */
ib_status_t core_audit_open_auditfile(ib_engine_t *ib,
                                      ib_auditlog_t *log,
                                      ib_core_audit_cfg_t *cfg,
                                      ib_core_cfg_t *corecfg);

ib_status_t core_audit_open_auditindexfile(ib_engine_t *ib,
                                           ib_auditlog_t *log,
                                           ib_core_audit_cfg_t *cfg,
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
 * @param[in] ib IronBee engine.
 * @param[in] log The log record.
 *
 * @return
 * - IB_OK On success.
 * - Other on failure. See log file for details.
 */
ib_status_t core_audit_open(ib_engine_t *ib,
                            ib_auditlog_t *log);

/**
 * Write audit log header. This is not thread-safe and should be protected
 * with a lock.
 *
 * @param[in] ib IronBee engine.
 * @param[in] log The log record.
 * @return IB_OK or IB_EUNKNOWN.
 */
ib_status_t core_audit_write_header(ib_engine_t *ib,
                                    ib_auditlog_t *log);

/**
 * Write part of a audit log. This call should be protected by a lock.
 *
 * @param[in] ib IronBee engine.
 * @param[in] part The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
ib_status_t core_audit_write_part(ib_engine_t *ib,
                                  ib_auditlog_part_t *part);

/**
 * Write an audit log footer. This call should be protected by a lock.
 *
 * @param[in] ib IronBee engine.
 * @param[in] log The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
ib_status_t core_audit_write_footer(ib_engine_t *ib,
                                    ib_auditlog_t *log);

/**
 * Close the audit log file and write to the index file.
 *
 * @param[in] ib IronBee engine.
 * @param[in] log The audit log we've just written to a file.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t core_audit_close(ib_engine_t *ib, ib_auditlog_t *log);

#endif // _IB_CORE_AUDIT_PRIVATE_H_
