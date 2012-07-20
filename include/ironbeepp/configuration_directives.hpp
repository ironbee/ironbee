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
 * @brief IronBee++ &mdash; ConfigurationDirectives
 *
 * This file defines ConfigurationDirectivesRegistrar, a helper class for
 * attaching directives to an Engine.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @sa Engine::register_configuration_directives()
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONFIGURATION_DIRECTIVES__
#define __IBPP__CONFIGURATION_DIRECTIVES__

#include <ironbeepp/abi_compatibility.hpp>

#include <ironbeepp/engine.hpp>
#include <ironbeepp/list.hpp>

#include <boost/function.hpp>

namespace IronBee {

class ConfigurationParser;

/**
 * Helper class for Engine::register_configuration_directives().
 *
 * This class is returned by Engine::register_configuration_directives() and
 * provides methods to easily register multiple configuration directives.
 *
 * Example:
 * @code
 * engine.register_configuration_directives()
 *       .param1("FirstConfig", some_functional())
 *       .param2("SecondConfig", &some_function)
 *       ;
 * @endcode
 **/
class ConfigurationDirectivesRegistrar
{
public:
    //! Constructor.  Use Engine::register_configuration_directives() instead.
    explicit
    ConfigurationDirectivesRegistrar(Engine engine);

    /**
     * Start block handler.
     *
     * Takes configuration parser, directive name, and block parameter.
     **/
    typedef boost::function<void(
        ConfigurationParser,
        const char*,
        const char*
    )> start_block_t;

    /**
     * End block handler.
     *
     * Takes configuration parser and directive name.
     **/
    typedef boost::function<void(
        ConfigurationParser,
        const char*
    )> end_block_t;

    /**
     * Register block directive.
     *
     * @param[in] name           Name of directive.
     * @param[in] start_function Function to call at start of block.
     * @param[in] end_function   Function to call at end of block.
     * @returns *this
     **/
    ConfigurationDirectivesRegistrar& block(
        const char*   name,
        start_block_t start_function,
        end_block_t   end_function
    );

    /**
     * On-off handler.
     *
     * Takes configuration parser, directive name, and whether on or off.
     **/
    typedef boost::function<void(
        ConfigurationParser,
        const char*,
        bool
    )> on_off_t;

     /**
      * Register on-off directive.
      *
      * @param[in] name     Name of directive.
      * @param[in] function Function to call when directive given.
      * @returns *this
      **/
    ConfigurationDirectivesRegistrar& on_off(
        const char* name,
        on_off_t    function
    );

    /**
     * Single parameter handler.
     *
     * Takes configuration parser, directive name, and parameter.
     **/
    typedef boost::function<void(
        ConfigurationParser,
        const char*,
        const char*
    )> param1_t;

    /**
     * Register single parameter directive.
     *
     * @param[in] name     Name of directive.
     * @param[in] function Function to call when directive given.
     * @returns *this
     **/
    ConfigurationDirectivesRegistrar& param1(
        const char* name,
        param1_t    function
    );

    /**
     * Two parameter handler.
     *
     * Takes configuration parser, directive name, and two parameters.
     **/
    typedef boost::function<void(
        ConfigurationParser,
        const char*,
        const char*,
        const char*
    )> param2_t;

    /**
     * Register two parameter directive.
     *
     * @param[in] name     Name of directive.
     * @param[in] function Function to call when directive given.
     * @returns *this
     **/
    ConfigurationDirectivesRegistrar& param2(
        const char* name,
        param2_t    function
    );

     /**
      * Register many parameter directive.
      *
      * Takes configuration parameter, directive name, and list of parameters.
      **/
    typedef boost::function<void(
        ConfigurationParser,
        const char*,
        List<const char*>
    )> list_t;

     /**
      * Register many parameter directive.
      *
      * @param[in] name     Name of directive.
      * @param[in] function Function to call when directive given.
      * @returns *this
      **/
    ConfigurationDirectivesRegistrar& list(
        const char* name,
        list_t      function
    );

    /**
     * Opflag handler.
     *
     * Takes configuration parser, directive name, value of flags, and a mask
     * indicating which flags where set.  I.e., if bit N in the mask is set,
     * then that flag was changed to bit N of the value.
     **/
    typedef boost::function<void(
        ConfigurationParser,
        const char*,
        uint32_t,
        uint32_t
    )> op_flags_t;

    /**
     * Register op flags parameter directive.
     *
     * Consider boost::assign for easy construction of @a value_map.
     *
     * @param[in] name      Name of directive.
     * @param[in] function  Function to call when directive given.
     * @param[in] value_map Map of flag name to flag bits.  If a flag name
     *                      is specified in the configuration, the bits in
     *                      its value are set to 1 in the mask and either
     *                      set to 1 or 0 in the value, depending on the
     *                      operation.
     * @returns *this
     **/
    ConfigurationDirectivesRegistrar& op_flags(
        const char*                    name,
        op_flags_t                     function,
        std::map<std::string, int64_t> value_map
    );

private:
    Engine m_engine;
};

} // IronBee

#endif
