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
 * @brief IronBee --- Mock module
 */

#ifndef _MOCK_MODULE_H_
#define _MOCK_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ironbee/module.h>

struct mock_module_conf_t {
    const char *param1_p1;
    const char *param2_p1;
    const char *param2_p2;
    const ib_list_t *list_params;
    bool blkend_called;
    int onoff_onoff;
    const char *sblk1_p1;
    ib_flags_t opflags_val;
    ib_flags_t opflags_mask;
};
typedef struct mock_module_conf_t mock_module_conf_t;

/**
 * @param[in] ib IronBee to register this module and initialize.
 *
 * @returns Result of ib_module_init.
 */
ib_status_t mock_module_register(ib_engine_t *ib);

/**
 * Return the module name.
 * @returns the module name.
 */
const char *mock_module_name();

#ifdef __cplusplus
}
#endif

#endif // _MOCK_MODULE_H_
