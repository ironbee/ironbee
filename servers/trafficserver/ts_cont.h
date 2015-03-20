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

#ifndef _TS_CONT_H_
#define _TS_CONT_H_

/**
 * @file
 * @brief IronBee --- Apache Traffic Server Plugin
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/mm.h>
#include <ironbee/types.h>

#include "ts_types.h"

typedef enum {
    JOB_CONN_STARTED,
    JOB_TX_STARTED,
    JOB_REQ_HEADER,
    JOB_REQ_DATA,
    JOB_RES_HEADER,
    JOB_RES_DATA,
    JOB_TX_FINISHED,
    JOB_CONN_FINISHED
} job_type_t;

typedef struct ts_jobqueue_t ts_jobqueue_t;

void ts_jobqueue_destroy(ts_jobqueue_t *jq);

ib_status_t ts_jobqueue_create(
    tsib_txn_ctx   *txndata,
    TSMutex         mutex
);

void ts_jobqueue_in(
    tsib_txn_ctx *txndata,
    job_type_t    job_type,
    TSCont        contp,
    void         *edata
);

void ts_jobqueue_schedule(tsib_txn_ctx *txndata);
#endif /*  _TS_CONT_H_ */