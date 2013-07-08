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
 * @brief Predicate --- Standard Development implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_development.hpp>

#include <predicate/call_helpers.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>

#include <ironbee/log.h>

#include <boost/bind.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>

using namespace std;
using boost::bind;
using boost::ref;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

string p_construct_value_string(ConstList<Value> values)
{
    list<string> string_values;
    BOOST_FOREACH(const Value& v, values) {
        string string_value;
        if (v.name_length() > 0) {
            string_value += string(v.name(), v.name_length());
            string_value += ":";
        }
        if (v.type() == Value::LIST) {
            string_value +=
                p_construct_value_string(v.value_as_list<Value>());
        }
        else {
            string_value += v.to_s();
        }
        string_values.push_back(string_value);
    }
    return "[" + boost::algorithm::join(string_values, ", ") + "]";
}

}

string P::name() const
{
    return "p";
}

void P::calculate(EvalContext context)
{
    list<string> value_strings;
    BOOST_FOREACH(const node_p& n, children()) {
        n->eval(context);
        value_strings.push_back(
            p_construct_value_string(n->values())
        );
    }

    cerr << boost::algorithm::join(value_strings, "; ") << endl;
    map_calculate(children().back(), context);
}

Value P::value_calculate(Value v, EvalContext context)
{
    return v;
}

bool P::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 1);
}

string Identity::name() const
{
    return "identity";
}

void Identity::calculate(EvalContext context)
{
    map_calculate(children().front(), context);
}

Value Identity::value_calculate(Value v, EvalContext context)
{
    return v;
}

bool Identity::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

struct Sequence::data_t
{
    int64_t current;
};

Sequence::Sequence() :
    m_data(new data_t())
{
};

string Sequence::name() const
{
    return "sequence";
}

void Sequence::reset()
{
    m_data->current = literal_value(children().front()).value_as_number();
}

bool Sequence::validate(NodeReporter reporter) const
{
    bool result = true;

    result = Validate::n_or_more_children(reporter, 1) && result;
    result = Validate::n_or_fewer_children(reporter, 3) && result;
    result = Validate::nth_child_is_integer(reporter, 0) && result;
    if (children().size() > 1) {
        result = Validate::nth_child_is_integer(reporter, 1) && result;
    }
    if (children().size() > 2) {
        result = Validate::nth_child_is_integer(reporter, 2) && result;
    }
    return result;
}

void Sequence::calculate(EvalContext context)
{
    // Figure out parameters.
    int64_t start;
    int64_t end = -1;
    int64_t step = 1;

    node_list_t::const_iterator i = children().begin();
    start = literal_value(*i).value_as_number();
    ++i;
    if (i != children().end()) {
        end = literal_value(*i).value_as_number();
        ++i;
        if (i != children().end()) {
            step = literal_value(*i).value_as_number();
        }
    }
    else {
        end = start - 1;
    }

    // Output current.
    add_value(
        Field::create_number(context.memory_pool(), "", 0, m_data->current)
    );

    // Advance current.
    m_data->current += step;

    // Figure out if infinite.
    if (
        (step > 0 && start > end ) ||
        (step < 0 && end > start )
    ) {
        return;
    }

    // Figure out if done.  Note >/< and not >=/<=.
    // Also note never finished if step == 0.
    if (
        (step > 0 && m_data->current > end)  ||
        (step < 0 && m_data->current < end)
    ) {
        finish();
    }
}

void load_development(CallFactory& to)
{
    to
        .add<P>()
        .add<Identity>()
        .add<Sequence>()
        ;
}

} // Standard
} // Predicate
} // IronBee
