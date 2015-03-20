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
 * @brief IronBee --- Apache Traffic Server Plugin
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 *
 * Types that are shared between header files.
 */

#ifndef TS_TYPES_H
#define TS_TYPES_H

#include <ts/ts.h>
#include <ironbee/types.h>
#include <ironbee/engine_types.h>


typedef struct tsib_txn_ctx tsib_txn_ctx;

int ironbee_plugin(TSCont contp, TSEvent event, void *edata);

void tx_list_destroy(ib_conn_t *conn);

void tx_finish(ib_tx_t *tx);

void ts_tsib_txn_ctx_destroy(tsib_txn_ctx *txndata);

#endif


