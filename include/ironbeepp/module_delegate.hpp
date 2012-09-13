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
 * @brief IronBee++ --- Module Delegate
 *
 * @sa ModuleDelegate
 * @sa module_bootstrap.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#ifndef __IBPP__MODULE_DELEGATE__
#define __IBPP__MODULE_DELEGATE__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/module.hpp>

namespace IronBee {

/**
 * Class suitable as parent class for a module delegate.
 *
 * This class can be used as a parent class for a module delegate to provide
 * default nop behavior for all callbacks and to provide easy storage of the
 * associated Module.
 *
 * Note that this class is *not* polymorphic.  No part of IronBee++ will ever
 * refer to a delegate by this class.  Nor is any delegate required to
 * subclass this.  It is an optional convenience.  See module_delegate.hpp
 * for details.
 **/
class ModuleDelegate
{
public:
    /**
     * Constructor.  Records @a module and otherwise does nothing.
     *
     * The module can be recovered via module().
     *
     * @param[in] module Module associated with the delegate.
     **/
    explicit
    ModuleDelegate(Module module);

    //! Initialize handler.  Nop.
    void initialize() const;

    //! Context Open Handler.  Nop.
    void context_open(Context context) const;

    //! Context close Handler.  Nop.
    void context_close(Context context) const;

    //! Context destroy Handler.  Nop.
    void context_destroy(Context context) const;

    //! Module accessor.
    Module& module() { return m_module; };

    //! Module const accessor.
    const Module& module() const { return m_module; };

private:
    Module m_module;
};

} // IronBee

#endif
