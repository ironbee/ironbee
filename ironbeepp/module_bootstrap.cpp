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
 * @brief IronBee++ &mdash; Module Bootstrap Implementation
 *
 * @sa module_bootstrap.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/module_bootstrap.hpp>

namespace IronBee {

namespace Internal {

void bootstrap_module(
    ib_engine_t* ib_engine,
    ib_module_t& ib_module,
    const char*  name,
    const char*  filename
)
{
    IB_MODULE_INIT_DYNAMIC(
        &ib_module,
        filename,                           // filename
        NULL,                               // data
        NULL,                               // ib, filled in init.
        name,                               // name
        NULL,                               // config data
        0,                                  // config data length
        NULL,                               // config copier
        NULL,                               // config copier data
        NULL,                               // config field map
        NULL,                               // config directive map
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );
    ib_module.ib = ib_engine;
}

} // Internal

} // IronBee
