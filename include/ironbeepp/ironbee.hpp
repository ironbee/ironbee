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
 * @brief IronBee++ &mdash; IronBee Library Routines
 *
 * This file defines routines that apply to global IronBee state.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__IRONBEE__
#define __IBPP__IRONBEE__

#include <ironbeepp/abi_compatibility.hpp>

namespace IronBee {

/**
 * Initialize IronBee Library.
 *
 * If using IronBee as part of a larger project, e.g., a server, this must be
 * called before anything else.  If writing a module, do not call this, as
 * IronBee will be initialized before the module is loaded.
 *
 * If you call this, you should call shutdown() when done with IronBee.
 *
 * @sa shutdown()
 **/
void initialize();

/**
 * Shutdown IronBee Library.
 *
 * @sa initialize()
 **/
void shutdown();

} // IronBee

#endif
