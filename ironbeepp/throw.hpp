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
 * @brief IronBee++ &mdash; Internal Throw
 * @internal
 *
 * This is the opposite of catch.hpp.  It converts status codes into
 * exceptions.
 *
 * @warning Because this file works at the C++/C boundary, it includes some C
 *          IronBee files.  This will greatly pollute the global namespace
 *          and macro space.
 *
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/types.h>

#ifndef __IBPP__THROW__
#define __IBPP__THROW__

namespace IronBee {
namespace Internal {

/**
 * Throw exception if @a status != IB_OK.
 * @internal
 *
 * The message is "Error reported from C API."
 *
 * @param[in] status Status code to base exception off of.
 **/
void throw_if_error( ib_status_t status );

} // Internal
} // IronBee

#endif
