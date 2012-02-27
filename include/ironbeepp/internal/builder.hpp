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
 * @brief IronBee++ Internals -- Builder
 * @internal
 *
 * This file defines Internal::Builder, a proxy for constructing objects
 * from C objects.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP_BUILDER__
#define __IBPP_BUILDER__

struct ib_module_t;
struct ib_engine_t;
struct ib_context_t;

namespace IronBee {

class Module;
class Engine;
class Context;

namespace Internal {

/**
 * Construct various IronBee++ objects from IronBee pointers.
 * @internal
 *
 * The purpose of this class is to isolate the C details of constructing
 * objects, allowing the public header files that define these objects to
 * avoid C details.
 **/
class Builder
{
public:
    //! Construct Engine.
    static Engine engine(   ib_engine_t*  ib_engine  );
    //! Construct Module.
    static Module  module(  ib_module_t*  ib_module  );
    //! Construct Context.
    static Context context( ib_context_t* ib_context );
};

} // Internal
} // IronBee

#endif
