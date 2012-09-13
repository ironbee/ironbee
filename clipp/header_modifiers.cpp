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
 * @brief IronBee --- CLIPP Header Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "header_modifiers.hpp"

#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using boost::make_shared;

namespace IronBee {
namespace CLIPP {

namespace  {

class SetModifierDelegate :
    public Input::ModifierDelegate
{
public:
    SetModifierDelegate(
        SetModifier::which_e which,
        const string&        key,
        const string&        value
    ) :
        m_which(which),
        m_key(key),
        m_value(value)
    {
        // nop
    }

    void request_header(Input::HeaderEvent& event)
    {
        if (m_which == SetModifier::REQUEST || m_which == SetModifier::BOTH) {
            modify_header(event);
        }
    }

    void response_header(Input::HeaderEvent& event)
    {
        if (m_which == SetModifier::RESPONSE || m_which == SetModifier::BOTH) {
            modify_header(event);
        }
    }

private:
    void modify_header(Input::HeaderEvent& event)
    {
        BOOST_FOREACH(Input::header_t& header, event.headers) {
            if (
                header.first.length == m_key.length() &&
                boost::iequals(
                    m_key,
                    boost::make_iterator_range(
                        header.first.data,
                        header.first.data + header.first.length
                    )
                )
            ) {
                header.second = Input::Buffer(m_value);
            }
        }
    }

    SetModifier::which_e m_which;
    const string& m_key;
    const string& m_value;
};

} // Anonymous

struct SetModifier::State
{
    which_e which;
    string  key;
    string  value;
};

SetModifier::SetModifier(
    which_e       which,
    const string& key,
    const string& value
) :
    m_state(make_shared<State>())
{
    m_state->which = which;
    m_state->key   = key;
    m_state->value = value;
}

bool SetModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    SetModifierDelegate delegate(
        m_state->which,
        m_state->key,
        m_state->value
    );

    input->connection.dispatch(delegate);

    return true;
}

} // CLIPP
} // IronBee
