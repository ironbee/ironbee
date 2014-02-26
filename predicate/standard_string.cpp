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
 * @brief Predicate --- Standard String Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_string.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/validate.hpp>

#include <boost/regex.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

struct StringReplaceRx::data_t
{
    boost::regex expression;
    string       replacement;
};

StringReplaceRx::StringReplaceRx() :
    m_data(new data_t())
{
    // nop
}

string StringReplaceRx::name() const
{
    return "stringReplaceRx";
}

bool StringReplaceRx::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 3) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;
    result = Validate::nth_child_is_string(reporter, 1) && result;

    return result;
}

void StringReplaceRx::pre_eval(Environment environment, NodeReporter reporter)
{
    // Validation guarantees that the first two children are string
    // literals and thus can be evaluated with default EvalContext.

    node_list_t::const_iterator child_i = children().begin();
    Value expression_value = literal_value(*child_i);
    ++child_i;
    Value replacement_value = literal_value(*child_i);

    ConstByteString expression = expression_value.value_as_byte_string();
    ConstByteString replacement  = replacement_value.value_as_byte_string();

    if (! expression) {
        reporter.error("Missing expression.");
        return;
    }
    if (! replacement) {
        reporter.error("Missing replacement.");
        return;
    }

    try {
        m_data->expression.assign(
            expression.const_data(), expression.length(),
            boost::regex_constants::normal
        );
    }
    catch (const boost::bad_expression& e) {
        reporter.error("Could not compile regexp: " + expression.to_s() + " (" + e.what() + ")");
        return;
    }

    m_data->replacement = replacement.to_s();
}

Value StringReplaceRx::value_calculate(
    Value           v,
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    if (! m_data) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Evaluation without pre evaluation!"
            )
        );
    }

    if (v.type() != Value::BYTE_STRING) {
        return Value();
    }

    ConstByteString text = v.value_as_byte_string();
    boost::shared_ptr<vector<char> > result(new vector<char>());
    // Ensure that result.data() is non-null, even if we never insert
    // anything.
    result->reserve(1);

    // value_to_data() ensures that a copy is associated with the memory
    // pool and will be deleted when the memory pool goes away.
    assert(context.memory_manager());
    value_to_data(result, context.memory_manager().ib());

    boost::regex_replace(
        back_inserter(*result),
        text.const_data(), text.const_data() + text.length(),
        m_data->expression,
        m_data->replacement
    );

    return Field::create_byte_string(
        context.memory_manager(),
        v.name(), v.name_length(),
        ByteString::create_alias(
            context.memory_manager(),
            result->data(), result->size()
        )
    );
}

void StringReplaceRx::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    const node_p& input_node = children().back();

    map_calculate(input_node, graph_eval_state, context);
}

void load_string(CallFactory& to)
{
    to
        .add<StringReplaceRx>()
        ;
}

} // Standard
} // Predicate
} // IronBee
