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
 * @brief Predicate --- CallFactory implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/call_factory.hpp>

#include <ironbeepp/exception.hpp>

namespace IronBee {
namespace Predicate {

CallFactory& CallFactory::add(const std::string& name, generator_t generator)
{
    m_classes[name] = generator;
    return *this;
}

call_p CallFactory::operator()(const std::string& name) const
{
    classes_t::const_iterator i = m_classes.find(name);
    if (i == m_classes.end()) {
        BOOST_THROW_EXCEPTION(
            enoent() << errinfo_what(
                ("No such call class: " + name).c_str()
            )
        );
    }
    call_p call = i->second(name);
    if (call->name() != name) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Name mismatch: Expected " + name + " but received " +
                call->name()
            )
        );
    }
    return call;
}

} // Predicate
} // IronBee
