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
 * @brief IronBee --- SQL Module
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbeepp/module.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/transformation.hpp>

#include <sqltfn.h>

#include "sql_remove_comments.hpp"

using namespace IronBee;

namespace {

class SqlModuleDelegate : public ModuleDelegate {
public:
    /**
     * Constructor.
     *
     * @param[in] module The module.
     */
    explicit SqlModuleDelegate(Module m);

};

} // Anonymous Namespace

SqlModuleDelegate::SqlModuleDelegate(Module m) : ModuleDelegate(m)
{
    remove_comments::register_trasformations(m);
}

IBPP_BOOTSTRAP_MODULE_DELEGATE("sql", SqlModuleDelegate);
