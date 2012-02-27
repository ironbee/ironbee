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
 * @brief IronBee++ Internals -- Data Implementation
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "data.hpp"
#include <ironbeepp/internal/catch.hpp>

namespace IronBee {
namespace Internal {

extern "C" {

ib_status_t data_cleanup( void* data )
{
    IB_FTRACE_INIT();

    boost::any* data_any = reinterpret_cast<boost::any*>( data );
    delete data_any;

    IB_FTRACE_RET_STATUS(IB_OK);
}

} // extern "C"

} // IronBee
} // Internal
