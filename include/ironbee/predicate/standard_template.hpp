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
 * @brief Predicate --- Standard Template.
 *
 * See reference.txt for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_TEMPLATE__
#define __PREDICATE__STANDARD_TEMPLATE__

#include <ironbee/predicate/call_factory.hpp>

#include <list>
#include <string>

namespace IronBee {
namespace Predicate {
namespace Standard {

//! List of arguments.
typedef std::list<std::string> template_arg_list_t;

/**
 * Create a Template generator.
 *
 * @param[in] args Template arguments.
 * @param[in] body Template body.
 * @param[in] origin_prefix A prefix attached to all origin information of
 *                          body nodes.
 * @return Generator suitable for registration with call factory.
 **/
CallFactory::generator_t define_template(
    const template_arg_list_t& args,
    const node_cp&             body,
    const std::string&         origin_prefix = std::string()
);

/**
 * Load all standard Template calls into a CallFactory.
 *
 * Adds Ref to @a to.  Templates need to be added as they are defined.
 *
 * @sa define_template()
 *
 * @param [in] to CallFactory to load into.
 **/
void load_template(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
