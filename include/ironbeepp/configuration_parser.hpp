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
 * @brief IronBee++ --- ConfigurationParser
 *
 * This file defines (Const)ConfigurationParser, a wrapper for ib_cfgparser_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @sa Engine::register_configuration_directives()
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONFIGURATION_PARSER__
#define __IBPP__CONFIGURATION_PARSER__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/list.hpp>

#include <ostream>

// IronBee C
typedef struct ib_cfgparser_t ib_cfgparser_t;

namespace IronBee {

class MemoryPool;
class Location;
class Context;
class Engine;

/**
 * Const ConfigurationParser; equivalent to a const pointer to ib_cfgparser_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See ConfigurationParser for discussion of configuration_parsers.
 *
 * @tparam T Value type for configuration_parser.
 *
 * @sa ConfigurationParser
 * @sa ironbeepp
 * @sa ib_cfgparser_t
 * @nosubgrouping
 **/
class ConstConfigurationParser :
    public CommonSemantics<ConstConfigurationParser>
{
public:
    //! C Type.
    typedef const ib_cfgparser_t* ib_type;

    /**
     * Construct singular ConstConfigurationParser.
     *
     * All behavior of a singular ConstConfigurationParser is undefined
     * except for assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstConfigurationParser();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_cfgparser_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ConfigurationParser from ib_cfgparser_t.
    explicit
    ConstConfigurationParser(ib_type ib_configuration_parser);

    ///@}

    //! Associated engine.
    Engine engine() const;

    //! Associated memory manager.
    MemoryManager memory_manager() const;

    //! Current configuration context.
    Context current_context() const;

    //! Current configuration file.
    const char* current_file() const;

    //! Current configuration block name.
    const char* current_block_name() const;

private:
    ib_type m_ib;
};

/**
 * ConfigurationParser; equivalent to a pointer to ib_cfgparser_t.
 *
 * ConfigurationParser can be treated as ConstConfigurationParser.  See @ref
 * ironbeepp for details on IronBee++ object semantics.
 *
 * ConfigurationParsers connect configuration text to engines.  It provides
 * methods for parsing a buffer or a file.
 *
 * @sa ConstConfigurationParser
 * @sa ironbeepp
 * @sa ib_cfgparser_t
 * @nosubgrouping
 **/
class ConfigurationParser :
    public ConstConfigurationParser
{
public:
    //! C Type.
    typedef ib_cfgparser_t* ib_type;

    /**
     * Create configuration parser.
     *
     * @param[in] engine Engine to parse for.
     * @returns Configuration parser.
     **/
    static ConfigurationParser create(Engine engine);

    /**
     * Remove the constness of a ConstConfigurationParser.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] configuration_parser ConstConfigurationParser to remove
     *                                 const from.
     * @returns ConfigurationParser pointing to same underlying
     *          configuration_parser as @a configuration_parser.
     **/
    static ConfigurationParser remove_const(
         ConstConfigurationParser configuration_parser
    );

    /**
     * Construct singular ConfigurationParser.
     *
     * All behavior of a singular ConfigurationParser is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConfigurationParser();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_cfgparser_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ConfigurationParser from ib_cfgparser_t.
    explicit
    ConfigurationParser(ib_type ib_configuration_parser);

    ///@}

    /**
     * Parse file at @a path.
     *
     * @param[in] path Path to parse.
     * @throw IronBee++ exception on error.
     **/
    void parse_file(const std::string& path) const;

    /**
     * Parse @a buffer.
     *
     * @param[in] buffer Buffer to parse.
     * @param[in] length Length of @a buffer.
     * @param[in] more   True iff more input in future.
     * @throw IronBee++ exception on error.
     **/
    void parse_buffer(
        const char* buffer,
        size_t      length,
        bool        more
    ) const;

    //! As above, but for string.
    void parse_buffer(const std::string& s, bool more) const;

    //! Destroy configuration parser, reclaiming memory.
    void destroy() const;

private:
    ib_type m_ib;
};

/**
 * Output operator for ConfigurationParser.
 *
 * Outputs ConfigurationParser[@e value] where value is the current file
 * and current block name separated by :.
 *
 * @param[in] o Ostream to output to.
 * @param[in] configuration_parser ConfigurationParser to output.
 * @return @a o
 **/
std::ostream& operator<<(
    std::ostream& o,
    const ConstConfigurationParser& configuration_parser
);

} // IronBee

#endif
