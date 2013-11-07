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
 * @brief IronBee --- CLIPP Select Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "select_modifier.hpp"

#include <boost/make_shared.hpp>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace  {

class in_range :
    public std::unary_function<SelectModifier::range_t, bool>
{
public:
    explicit
    in_range(size_t index) :
        m_index(index)
    {
        // nop
    }

    bool operator()(const SelectModifier::range_t& range) const
    {
        return m_index >= range.first && m_index <= range.second;
    }

private:
    size_t m_index;
};

} // Anonymous

struct SelectModifier::State
{
    range_list_t select;
    size_t       current;
};

SelectModifier::SelectModifier(const range_list_t& select) :
    m_state(boost::make_shared<State>())
{
    m_state->select  = select;
    m_state->current = 0;
}

bool SelectModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    ++m_state->current;

    range_list_t::const_iterator i = find_if(
        m_state->select.begin(), m_state->select.end(),
        in_range(m_state->current)
    );

    return i != m_state->select.end();
}

} // CLIPP
} // IronBee
