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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>

#include "ironbee_private.h"
#include "ironbee_core_private.h"

ib_status_t ib_core_operators_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT(ib_core_operators_init);

    IB_FTRACE_RET_STATUS(IB_OK);
}
