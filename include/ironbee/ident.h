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
 * @brief IronBee --- user identity framework.
 *
 *
 */
#ifndef _IB_IDENT_H_
#define _IB_IDENT_H_

#include <ironbee/engine_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Identity provider struct */
typedef struct ib_ident_provider_t {
    ib_state_event_type_t event;           /** Event to act on */
    const char *(*check_id)(ib_tx_t *tx);  /** Check identity */
    ib_status_t (*challenge)(ib_tx_t *tx); /** Challenge client to identify */
} ib_ident_provider_t;

/**
 * Register an identity provider
 *
 * @param name Identifier name (used in IdentType directive).
 * @param provider The provider
 * @return success or error
 */
ib_status_t ib_ident_provider_register(const char *name,
                                       ib_ident_provider_t *provider);

#ifdef __cplusplus
}
#endif

#endif /* _IB_IDENT_H_ */
