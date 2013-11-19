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
 * @brief Predicate --- Standard Filter implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_filter.hpp>

#include <predicate/call_helpers.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

bool value_equal(Value a, Value b)
{
    if ((! a && ! b) || (a == b)) {
        return true;
    }

    if (a.type() != b.type()) {
        return false;
    }

    switch (a.type()) {
        case Value::NUMBER:
            return a.value_as_number() == b.value_as_number();
        case Value::FLOAT:
            return a.value_as_float() == b.value_as_float();
        case Value::BYTE_STRING:
        {
            ConstByteString a_s = a.value_as_byte_string();
            ConstByteString b_s = b.value_as_byte_string();
            if (a_s.length() != b_s.length()) {
                return false;
            }
            return equal(
                a_s.const_data(), a_s.const_data() + a_s.length(),
                b_s.const_data()
            );
        }
        case Value::LIST:
            return false;
        default:
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Unsupported value type for " +
                    boost::lexical_cast<string>(a)
                )
            );
    }
}

bool value_less(Value a, Value b)
{
    if ((! a && ! b) || (a == b)) {
        return false;
    }

    if (a.type() != b.type()) {
        return false;
    }

    switch (a.type()) {
        case Value::NUMBER:
            return a.value_as_number() < b.value_as_number();
        case Value::FLOAT:
            return a.value_as_float() < b.value_as_float();
        default:
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Unsupported value type for " +
                    boost::lexical_cast<string>(a)
                )
            );
    }
}

}

string Eq::name() const
{
    return "eq";
}

void Eq::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Eq::value_calculate(
    Value           v,
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    Value f = literal_value(children().front());
    return value_equal(f, v) ? v : Value();
}

bool Eq::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_literal(reporter, 0) && result;

    return result;
}

string Ne::name() const
{
    return "ne";
}

void Ne::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Ne::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    Value f = literal_value(children().front());
    return (! value_equal(f, v)) ? v : Value();
}

bool Ne::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_literal(reporter, 0) && result;

    return result;
}

string Lt::name() const
{
    return "lt";
}

void Lt::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Lt::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    Value f = literal_value(children().front());
    return value_less(f, v) ? v : Value();
}

bool Lt::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_literal(reporter, 0) && result;
    if (! children().empty()) {
        Value::type_e type = literal_value(children().front()).type();
        if (type != Value::FLOAT && type != Value::NUMBER) {
            reporter.error("Lt only supports numeric results.");
            return false;
        }
    }

    return result;
}

string Le::name() const
{
    return "le";
}

void Le::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Le::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    Value f = literal_value(children().front());
    return (value_less(f, v) || value_equal(f, v)) ? v : Value();
}

bool Le::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_literal(reporter, 0) && result;
    if (! children().empty()) {
        Value::type_e type = literal_value(children().front()).type();
        if (type != Value::FLOAT && type != Value::NUMBER) {
            reporter.error("Le only supports numeric results.");
            return false;
        }
    }

    return result;
}

string Gt::name() const
{
    return "gt";
}

void Gt::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Gt::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    Value f = literal_value(children().front());
    return (! value_less(f, v) && ! value_equal(f, v)) ? v : Value();
}

bool Gt::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_literal(reporter, 0) && result;
    if (! children().empty()) {
        Value::type_e type = literal_value(children().front()).type();
        if (type != Value::FLOAT && type != Value::NUMBER) {
            reporter.error("Gt only supports numeric results.");
            return false;
        }
    }

    return result;
}

string Ge::name() const
{
    return "ge";
}

void Ge::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Ge::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    Value f = literal_value(children().front());
    return (! value_less(f, v)) ? v : Value();
}

bool Ge::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_literal(reporter, 0) && result;
    if (! children().empty()) {
        Value::type_e type = literal_value(children().front()).type();
        if (type != Value::FLOAT && type != Value::NUMBER) {
            reporter.error("Ge only supports numeric results.");
            return false;
        }
    }

    return result;
}

struct Typed::data_t
{
    Value::type_e type;
};

Typed::Typed() :
    m_data(new data_t())
{
    // nop
}

string Typed::name() const
{
    return "typed";
}

void Typed::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Typed::value_calculate(
    Value           v,
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    return (v.type() == m_data->type) ? v : Value();
}

namespace {

Value::type_e typed_parse_type(const string& type_s)
{
    if (type_s == "list") {
        return Value::LIST;
    }
    if (type_s == "number") {
        return Value::NUMBER;
    }
    if (type_s == "float") {
        return Value::FLOAT;
    }
    if (type_s == "string") {
        return Value::BYTE_STRING;
    }
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "Invalid typed argument."
        )
    );
}

}

void Typed::pre_eval(Environment environment, NodeReporter reporter)
{
    string type_s =
        literal_value(children().front()).value_as_byte_string().to_s();
    m_data->type = typed_parse_type(type_s);
}

bool Typed::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;
    if (! children().empty()) {
        string type_s =
            literal_value(children().front()).value_as_byte_string().to_s();
        try {
            typed_parse_type(type_s);
        }
        catch (einval) {
            reporter.error("Invalid typed argument.");
        }
    }

    return result;
}

string Named::name() const
{
    return "named";
}

void Named::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value Named::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    ConstByteString name =
        literal_value(children().front()).value_as_byte_string();
    if (
        v.name_length() == name.length() &&
        equal(name.const_data(), name.const_data() + name.length(), v.name())
    ) {
        return v;
    }
    return Value();
}

bool Named::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

string NamedI::name() const
{
    return "namedi";
}

void NamedI::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

namespace {

bool namedi_caseless_compare(char a, char b)
{
    return (a == b || tolower(a) == tolower(b));
}

}

Value NamedI::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    ConstByteString name =
        literal_value(children().front()).value_as_byte_string();
    if (
        v.name_length() == name.length() &&
        equal(
            name.const_data(), name.const_data() + name.length(),
            v.name(),
            namedi_caseless_compare
        )
    ) {
        return v;
    }
    return Value();
}

bool NamedI::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

Sub::Sub() :
    AliasCall("namedi")
{
    // nop
}

string Sub::name() const
{
    return "sub";
}

struct NamedRx::data_t
{
    boost::regex re;
};

NamedRx::NamedRx() :
    m_data(new data_t())
{
    // nop
}

string NamedRx::name() const
{
    return "namedRx";
}

void NamedRx::eval_calculate(GraphEvalState& graph_eval_state, EvalContext context) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

Value NamedRx::value_calculate(Value v, GraphEvalState& graph_eval_state, EvalContext context) const
{
    if (
        regex_search(
            v.name(), v.name() + v.name_length(),
            m_data->re
        )
    ) {
        return v;
    }
    return Value();
}

void NamedRx::pre_eval(Environment environment, NodeReporter reporter)
{
    ConstByteString re =
        literal_value(children().front()).value_as_byte_string();

    try {
        m_data->re = boost::regex(
            re.const_data(), re.const_data() + re.length()
        );
    }
    catch (const boost::bad_expression& e) {
        reporter.error(
            "Error compiling regexp: " + re.to_s() + " (" + e.what() + ")"
        );
    }
}

bool NamedRx::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

void load_filter(CallFactory& to)
{
    to
        .add<Eq>()
        .add<Ne>()
        .add<Lt>()
        .add<Le>()
        .add<Gt>()
        .add<Ge>()
        .add<Typed>()
        .add<Named>()
        .add<NamedI>()
        .add<Sub>()
        .add<NamedRx>()
        ;
}

} // Standard
} // Predicate
} // IronBee
