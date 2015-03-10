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
 * @brief IronBee --- Stream inflate module. Handles Content-Encoding: deflate.
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>

#include "stream_inflate_private.h"
#include <zlib.h>

namespace {

//! Module delegate.
class StreamInflateModule : public IronBee::ModuleDelegate
{
public:
    //! Constructor.
    explicit StreamInflateModule(IronBee::Module module);
};

} // Anonymous namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("stream_inflate", StreamInflateModule)

namespace {

StreamInflateModule::StreamInflateModule(IronBee::Module module) :
    IronBee::ModuleDelegate(module)
{
    ib_stream_processor_registry_t *registry;
    IronBee::List<const char *> types =
        IronBee::List<const char *>::create(module.engine().main_memory_mm());
    types.push_back("deflate");

    registry = ib_engine_stream_processor_registry(module.engine().ib());
    ib_stream_processor_registry_register(registry,
                                          "stream_inflate",
                                          types.ib(),
                                          create_inflate_processor,
                                          NULL,
                                          execute_inflate_processor,
                                          NULL,
                                          destroy_inflate_processor,
                                          NULL
                                         );
}

} // Anonymous namespace
