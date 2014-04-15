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
 * @brief IronBee --- CLIPP Configuration Parser
 *
 * Defines parser of CLIPP configuration.
 *
 * The CLIPP configuration grammar is a sequence of zero or more chains.  Each
 * chain is a base component and a sequence of zero or more modifier
 * components.  Each component is a name and argument separated by a colon.
 * Modifier components have an at sign before their name.
 *
 * Formally:
 * @code
 * configuration := *chain
 * chain         := base *modifier
 * base          := component
 * modifier      := AT component
 * component     := name COLON argument
 *                | name
 * @endcode
 *
 * Arguments can be quoted strings which can be used to embed spaces.  At
 * present no other escaping is possible but this will probably change.
 *
 * Chains must be separated by white space.  Whitespace is allowed between
 * base and modifier and between modifiers but is not required.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__CONFIGURATION_PARSER__
#define __IRONBEE__CLIPP__CONFIGURATION_PARSER__

#include <boost/scoped_ptr.hpp>
#include <string>
#include <vector>

namespace IronBee {
namespace CLIPP {
namespace ConfigurationParser {

//! A parsed component.
struct component_t
{
    //! Name of component.
    std::string name;
    //! Argument of component.
    std::string arg;
};

//! Sequence of components.
typedef std::vector<component_t> component_vec_t;

//! A parsed chain.
struct chain_t
{
    //! Base component.
    component_t       base;
    //! Modifiers
    component_vec_t modifiers;
};

//! A sequence of chains.
typedef std::vector<chain_t> chain_vec_t;

/**
 * Parse @a input as configuration.
 *
 * @param[in] input Text to parse.
 * @return Sequence of chains.
 * @throw runtime_error on any error.
 **/
chain_vec_t parse_string(const std::string& input);

/**
 * Parse @a path as configuration file.
 *
 * This is similar to @a parse_string except that it will ignore any line
 * where the first non-space character is @c #.
 *
 * @param[in] path Path to configuration file.
 * @return Sequence of chains.
 * @throw runtime_error or any error.
 **/
chain_vec_t parse_file(const std::string& path);

} // ConfigurationParser
} // CLIPP
} // IronBee

#endif
