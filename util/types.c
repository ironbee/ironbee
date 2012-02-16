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
 * @brief IronBee - Types related functions.
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/types.h>

const char *ib_status_to_string(ib_status_t status)
{
  switch (status) {
    case IB_OK: return "OK";
    case IB_DECLINED: return "DECLINED";
    case IB_EUNKNOWN: return "EUNKNOWN";
    case IB_ENOTIMPL: return "ENOTIMPL";
    case IB_EINCOMPAT: return "EINCOMPAT";
    case IB_EALLOC: return "EALLOC";
    case IB_EINVAL: return "EINVAL";
    case IB_ENOENT: return "ENOENT";
    case IB_ETRUNC: return "ETRUNC";
    case IB_ETIMEDOUT: return "ETIMEDOUT";
    case IB_EAGAIN: return "EAGAIN";
    case IB_EOTHER: return "EOTHER";
    default: return "Unknown Status Code";
  }
}
