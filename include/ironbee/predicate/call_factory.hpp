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
 * @brief Predicate --- CallFactory
 *
 * Factory for Call nodes.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__CALL_FACTORY__
#define __PREDICATE__CALL_FACTORY__

#include <ironbee/predicate/dag.hpp>

#include <boost/function.hpp>

#include <map>
#include <string>

namespace IronBee {
namespace Predicate {

/**
 * Generate Call nodes given a name.
 *
 * Use add() to add subclasses of Call and operator()() to create an
 * instance by name.
 **/
class CallFactory
{
public:
    //! Generator function.
    typedef boost::function<call_p(const std::string&)> generator_t;

    /**
     * Add a Call subclass to factory.
     *
     * @tparam CallSubclass Subclass to add.
     * @return *this
     **/
    template <typename CallSubclass>
    CallFactory& add();

    /**
     * Add a Call subclass to factory.
     *
     * @param[in] name      Name to use.
     * @param[in] generator Generator function.  Return must have same name
     *   as @a name.
     * @return *this
     **/
    CallFactory& add(const std::string& name, generator_t generator);

    /**
     * Construct instance of subclass named @a name.
     *
     * @param[in] name Name of subclass to instantiate.
     * @return call_p pointing to new default constructed subclass of
     *   name @a name.
     * @throw IronBee::enoent if no class registered with name @a name.
     * @throw IronBee::einval if result has different name than @a name.
     **/
    call_p operator()(const std::string& name) const;

private:
    //! Type of map of name to generator.
    typedef std::map<std::string, generator_t> classes_t;
    //! Map of name to generator.
    classes_t m_classes;

    /**
     * Generator helper.
     *
     * @tparam CallSubclass Subclass to generate.
     **/
    template <typename CallSubclass>
    class Generator
    {
    public:
        /**
         * Default construct @a CallSubclass.
         *
         * @return call_p pointing to new default constructed
         *   @a CallSubclass.
         **/
        call_p operator()(const std::string&) const
        {
            return call_p(new CallSubclass());
        }
    };
};

template <typename CallSubclass>
CallFactory& CallFactory::add()
{
    return add(CallSubclass().name(), Generator<CallSubclass>());
}

} // Predicate
} // IronBee

#endif
