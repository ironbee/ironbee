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
 * @brief IronBee --- Constant Module External API
 *
 * This header file defines a C and C++ for getting and setting constants.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */
#ifndef __MODULE__CONSTANT__
#define __MODULE__CONSTANT__

#include <ironbee/context.h>
#include <ironbee/field.h>

#ifdef __cplusplus
#include <string>

extern "C" {
#endif

/**
 * Get a constant.
 *
 * @param[out] value      Where to store value; `*value` will be set to NULL
 *                        if no constant found.
 * @param[in]  ctx        Context.
 * @param[in]  key        Key.
 * @param[in]  key_length Length of @a key.
 * @return
 * - IB_OK on success, whether constant found or not.
 * - IB_EOTHER on unexpected failure.
 **/
ib_status_t DLL_PUBLIC ib_module_constant_get(
    const ib_field_t** value,
    const ib_context_t *ctx,
    const char *key,
    size_t key_length
)
NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Set a constant.
 *
 * @param[in] ctx   Context.
 * @param[in] value Constant to set; name of @a value will be used as key.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if constant already exists.
 * - IB_EOTHER on unexpected failure.
 **/
ib_status_t DLL_PUBLIC ib_module_constant_set(
    ib_context_t* ctx,
    const ib_field_t *value
)
NONNULL_ATTRIBUTE(1, 2);

#ifdef __cplusplus
}

namespace IronBee {

class Context;
class ConstField;
class ConstByteString;

namespace Constant {

/**
 * Get a constant.
 *
 * @param[in]  ctx        Context.
 * @param[in]  key        Key.
 * @param[in]  key_length Length of @a key.
 * @throw
 * - @ref eother on unexpected failure.
 **/
ConstField get(ConstContext ctx, const char* key, size_t key_length);

//! Overload of above for c-string.
ConstField get(ConstContext ctx, const char* key);

//! Overload of above for ConstByteString.
ConstField get(ConstContext ctx, ConstByteString key);

//! Overload of above for std::string.
ConstField get(ConstContext ctx, const std::string& key);

/**
 * Set a constant.
 *
 * @param[in] ctx   Context.
 * @param[in] value Constant to set; name of @a value will be used as key.
 * @throw
 * - @ref einval if constant already exists.
 * - @ref eother on unexpected failure.
 **/
void set(Context ctx, ConstField value);

} // Constant

} // IronBee

#endif

#endif
