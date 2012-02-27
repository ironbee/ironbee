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
 * @brief IronBee++ Internals -- Builder implementation.
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/internal/builder.hpp>

#define IBPP_EXPOSE_C
#include <ironbeepp/engine.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/context.hpp>

#include "module_data.hpp"
#include "engine_data.hpp"
#include "context_data.hpp"

#include <boost/make_shared.hpp>

using boost::make_shared;

namespace IronBee {
namespace Internal {

Engine Builder::engine( ib_engine_t* ib_engine )
{
    Engine::data_t data = make_shared<EngineData>();
    data->ib_engine = ib_engine;
    return Engine( data );
}

Module Builder::module( ib_module_t* ib_module )
{
    Module::data_t data = make_shared<ModuleData>();
    data->ib_module = ib_module;
    return Module( data );
}

Context Builder::context( ib_context_t* ib_context )
{
    Context::data_t data = make_shared<ContextData>();
    data->ib_context = ib_context;
    return Context( data );
}

} // Internal
} // IronBee
