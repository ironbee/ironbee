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

#ifndef _IB_PRIVATE_H_
#define _IB_PRIVATE_H_

/**
 * @file
 * @brief IronBee &mdash; Private Declarations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/types.h>
#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/debug.h>
#include <ironbee/lock.h>
#include <ironbee/server.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/array.h>
#include <ironbee/logformat.h>
#include <ironbee/rule_defs.h>

/* Pull in FILE* for ib_auditlog_cfg_t. */
#include <stdio.h>

#include "state_notify_private.h"

#endif /* IB_PRIVATE_H_ */
