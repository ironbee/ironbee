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
 * @brief Predicate --- Standard ValueList Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_valuelist.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

string SetName::name() const
{
    return "setName";
}

Value SetName::value_calculate(Value v, EvalContext context)
{
    Value name = literal_value(children().front());
    ConstByteString name_bs = name.value_as_byte_string();

    return v.dup(v.memory_pool(), name_bs.const_data(), name_bs.length());
}

void SetName::calculate(EvalContext context)
{
    map_calculate(children().back(), context);
}

string Cat::name() const
{
    return "cat";
}

void Cat::calculate(EvalContext context)
{
    // @todo Change to be opportunitistic.  I.e., add values of first
    // child immediately.  Once that child is finished, do same for next,
    // and so on.

    // Do nothing if any unfinished children.
    BOOST_FOREACH(const node_p& child, children()) {
        child->eval(context);
        if (! child->is_finished()) {
            return;
        }
    }

    // All children finished, concatenate values.
    BOOST_FOREACH(const node_p& child, children()) {
        BOOST_FOREACH(Value v, child->values()) {
            add_value(v);
        }
    }
    finish();
}

string First::name() const
{
    return "first";
}

void First::calculate(EvalContext context)
{
    const node_p& child = children().front();
    ValueList values = child->eval(context);
    if (! values.empty()) {
        add_value(values.front());
        finish();
    }
    else if (child->is_finished()) {
        finish();
    }
}

struct Rest::data_t
{
    ValueList::const_iterator location;
};

Rest::Rest() :
    m_data(new data_t())
{
    // nop
}

string Rest::name() const
{
    return "rest";
}

void Rest::reset()
{
    m_data->location = ValueList::const_iterator();
}

void Rest::calculate(EvalContext context)
{
    const node_p& child = children().front();
    ValueList values = child->eval(context);

    // Special case if no values yet.
    if (values.empty()) {
        if (child->is_finished()) {
            finish();
        }
        return;
    }

    if (m_data->location == ValueList::const_iterator()) {
        m_data->location = values.begin();
    }

    // At this point, m_data->location refers to element before next one
    // to push.
    ValueList::const_iterator next_location = m_data->location;
    ++next_location;
    const ValueList::const_iterator end = values.end();
    while (next_location != end) {
        add_value(*next_location);
        m_data->location = next_location;
        ++next_location;
    }

    if (child->is_finished()) {
        finish();
    }
}

void load_valuelist(CallFactory& to)
{
    to
        .add<SetName>()
        .add<Cat>()
        .add<First>()
        .add<Rest>()
    ;
}

} // Standard
} // Predicate
} // IronBee
