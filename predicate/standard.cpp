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
 * @brief Predicate --- Standard implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard.hpp>
#include <predicate/standard_boolean.hpp>
#include <predicate/merge_graph.hpp>

#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>

#include <ironbee/data.h>
#include <ironbee/engine.h>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

bool caseless_compare(char a, char b)
{
    return (a == b || tolower(a) == tolower(b));
}

//! Assert and extract simple value from node.
Value simple_value(const node_cp& node)
{
    if (! node->finished()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Asked for simple value of unfinished node."
            )
        );
    }
    if (node->values().size() > 1) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Asked for simple values of non-simple node."
            )
        );
    }

    if (node->values().empty()) {
        return Value();
    }
    else {
        return node->values().front();
    }
}

//! Assert and extra literal value from node.
Value literal_value(const node_p& node)
{
    if (! node->is_literal()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Asked for literal value of non-literal node."
            )
        );
    }
    if (! node->finished()) {
        node->eval(EvalContext());
    }
    return simple_value(node);
}

}

string Field::name() const
{
    return "field";
}

void Field::pre_eval(Environment environment, NodeReporter reporter)
{
    const ib_data_config_t* config;
    ib_status_t rc;
    size_t i;

    config = ib_engine_data_config_get_const(environment.ib());
    assert(config != NULL);

    // Key must be static.
    Value key_field = children().front()->eval(EvalContext());
    IronBee::ConstByteString key = key_field.value_as_byte_string();

    rc = ib_data_lookup_index_ex(
        config,
        key.const_data(), key.size(),
        &i
    );

    if (rc == IB_ENOENT) {
        /* Not an indexed field. */
        m_is_indexed = false;
    }
    else if (rc == IB_OK) {
        m_is_indexed = true;
        m_index = i;
    }
    else {
        IronBee::throw_if_error(rc);
    }
}

Value Field::calculate(EvalContext context)
{
    Value key_field = children().front()->eval(context);
    IronBee::ConstByteString key = key_field.value_as_byte_string();
    ib_field_t* data_field;
    ib_status_t rc;

    if (m_is_indexed) {
        IronBee::throw_if_error(
            ib_data_get_indexed(context.ib()->data, m_index, &data_field)
        );
    }
    else {
        rc = ib_data_get_ex(
            context.ib()->data,
            key.const_data(), key.size(),
            &data_field
        );
        if (rc == IB_ENOENT) {
            return Value();
        }
        else {
            IronBee::throw_if_error(rc);
        }
    }

    return Value(data_field);
}


string Operator::name() const
{
    return "operator";
}

struct Operator::data_t
{
    ConstOperator op;
    void*         instance_data;
};

void Operator::pre_eval(Environment environment, NodeReporter reporter)
{
    m_data.reset(new data_t());

    // Validation guarantees that the first two children are string
    // literals and thus can be evaluated with default EvalContext.

    node_list_t::const_iterator child_i = children().begin();
    Value op_name_value = (*child_i)->eval(EvalContext());
    ++child_i;
    Value params_value = (*child_i)->eval(EvalContext());

    ConstByteString op_name = op_name_value.value_as_byte_string();
    ConstByteString params  = params_value.value_as_byte_string();

    if (! op_name) {
        reporter.error("Missing operator name.");
        return;
    }
    if (! params) {
        reporter.error("Missing parameters.");
        return;
    }

    try {
        m_data->op =
            ConstOperator::lookup(environment, op_name.to_s().c_str());
    }
    catch (IronBee::enoent) {
        reporter.error("No such operator: " + op_name.to_s());
        return;
    }

    if (! (m_data->op.capabilities() & IB_OP_CAPABILITY_NON_STREAM)) {
        reporter.error("Only non-stream operator currently supported.");
        return;
    }

    m_data->instance_data = m_data->op.create_instance(
        environment.main_context(),
        IB_OP_CAPABILITY_NON_STREAM,
        params.to_s().c_str()
    );
}

Value Operator::calculate(EvalContext context)
{
    static const char* c_capture_name = "predicate_operator_capture";

    if (! m_data) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Evaluation without pre evaluation!"
            )
        );
    }

    Value input = children().back()->eval(context);
    IronBee::Field capture = IronBee::Field::create_no_copy_list<void *>(
        context.memory_pool(),
        c_capture_name,
        sizeof(c_capture_name) - 1,
        IronBee::List<void *>::create(context.memory_pool())
    );

    int result = m_data->op.execute_instance(
        m_data->instance_data,
        context,
        input,
        capture
    );

    if (result) {
        return capture;
    }
    else {
        return Value();
    }
}

SpecificOperator::SpecificOperator(const std::string& op) :
    m_operator(op)
{
    // nop
}

std::string SpecificOperator::name() const
{
    return m_operator;
}

bool SpecificOperator::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    assert(children().size() == 2);

    const node_cp& me = shared_from_this();
    node_p replacement(new Operator());
    replacement->add_child(node_p(new String(m_operator)));
    replacement->add_child(children().front());
    replacement->add_child(children().back());

    merge_graph.replace(me, replacement);
    return true;
}

Value SpecificOperator::calculate(EvalContext context)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "SpecificOperator must transform."
        )
    );
}

string Transformation::name() const
{
    return "transformation";
}

struct Transformation::data_t
{
    ConstTransformation transformation;
};

void Transformation::pre_eval(Environment environment, NodeReporter reporter)
{
    m_data.reset(new data_t());

    // Validation guarantees that the first child is a string interval
    // and thus can be evaluated with default EvalContext.

    Value name_value = children().front()->eval(EvalContext());
    ConstByteString name = name_value.value_as_byte_string();

    if (! name) {
        reporter.error("Missing transformation name.");
        return;
    }

    m_data->transformation = ConstTransformation::lookup(
        environment, string(name.const_data(), name.length()).c_str()
    );
}

Value Transformation::calculate(EvalContext context)
{
    Value input = children().back()->eval(context);

    if (! input) {
        return Value();
    }

    return m_data->transformation.execute(context.memory_pool(), input);
}

SpecificTransformation::SpecificTransformation(const std::string& tfn) :
    m_transformation(tfn)
{
    // nop
}

std::string SpecificTransformation::name() const
{
    return m_transformation;
}

bool SpecificTransformation::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    assert(children().size() == 1);

    const node_cp& me = shared_from_this();
    node_p replacement(new Transformation());
    replacement->add_child(node_p(new String(m_transformation)));
    replacement->add_child(children().front());

    merge_graph.replace(me, replacement);
    return true;
}

Value SpecificTransformation::calculate(EvalContext context)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "SpecificTransformation must transform."
        )
    );
}

string SetName::name() const
{
    return "set_name";
}

Value SetName::calculate(EvalContext context)
{
    Value name  = children().front()->eval(context);
    Value value = children().back()->eval(context);

    ConstByteString name_bs = name.value_as_byte_string();
    return value.dup(
        value.memory_pool(),
        name_bs.const_data(), name_bs.length()
    );
}

string List::name() const
{
    return "list";
}

Value List::calculate(EvalContext context)
{
    IronBee::List<Value> results =
        IronBee::List<Value>::create(context.memory_pool());
    BOOST_FOREACH(const node_p& child, children()) {
        results.push_back(child->eval(context));
    }

    return IronBee::Field::create_no_copy_list(
        context.memory_pool(),
        "", 0,
        results
    );
}

string Sub::name() const
{
    return "sub";
}

Value Sub::calculate(EvalContext context)
{
    Value collection = children().back()->eval(context);
    if (collection.type() != Value::LIST) {
        return Value();
    }

    Value subfield_name = children().front()->eval(context);

    ConstByteString subfield_name_bs = subfield_name.value_as_byte_string();

    if (collection.is_dynamic()) {
        ConstList<Value> result = collection.value_as_list<Value>(
            subfield_name_bs.const_data(), subfield_name_bs.length()
        );
        if (! result || result.empty()) {
            return Value();
        }
        return result.front();
    }
    else {
        BOOST_FOREACH(const Value& v, collection.value_as_list<Value>()) {
            if (
                v.name_length() == subfield_name_bs.length() &&
                equal(
                    v.name(), v.name() + v.name_length(),
                    subfield_name_bs.const_data(),
                    caseless_compare
                )
            )
            {
                return v;
            }
        }
        return Value();
    }
}

string SubAll::name() const
{
    return "suball";
}

Value SubAll::calculate(EvalContext context)
{
    Value collection = children().back()->eval(context);
    if (collection.type() != Value::LIST) {
        return Value();
    }

    Value subfield_name = children().front()->eval(context);

    ConstByteString subfield_name_bs = subfield_name.value_as_byte_string();

    IronBee::List<Value> results =
        IronBee::List<Value>::create(context.memory_pool());
    if (collection.is_dynamic()) {
        ConstList<Value> const_results = collection.value_as_list<Value>(
            subfield_name_bs.const_data(), subfield_name_bs.length()
        );
        if (const_results) {
            // @todo This copy should be removable with appropriate updates to
            // IronBee const correctness.  All we need is fields of const
            // lists.
            copy(
                const_results.begin(), const_results.end(),
                back_inserter(results)
            );
        }
    }
    else {
        BOOST_FOREACH(const Value& v, collection.value_as_list<Value>()) {
            if (
                v.name_length() == subfield_name_bs.length() &&
                equal(
                    v.name(), v.name() + v.name_length(),
                    subfield_name_bs.const_data(),
                    caseless_compare
                )
            )
            {
                results.push_back(v);
            }
        }
    }
    if (results.empty()) {
        return Value();
    }
    else {
        return IronBee::Field::create_no_copy_list<Value>(
            context.memory_pool(),
            "", 0,
            results
        );
    }
}

namespace {

call_p generate_specific_operator(const std::string& name)
{
    return call_p(new SpecificOperator(name));
}
call_p generate_specific_transformation(const std::string& name)
{
    return call_p(new SpecificTransformation(name));
}

}

void load(CallFactory& to)
{
    load_boolean(to);

    to
        .add<Field>()
        .add<Operator>()
        .add<Transformation>()
        .add<SetName>()
        .add<List>()
        .add<Sub>()
    ;

    // IronBee SpecificOperators
    to
        .add("streq", generate_specific_operator)
        .add("istreq", generate_specific_operator)
        .add("rx", generate_specific_operator)
        .add("lt", generate_specific_operator)
        .add("gt", generate_specific_operator)
        .add("ge", generate_specific_operator)
        .add("le", generate_specific_operator)
        .add("eq", generate_specific_operator)
        .add("ne", generate_specific_operator)
    ;

    // IronBee SpecificTransformations
    to
        .add("normalizePathWin", generate_specific_transformation)
        .add("normalizePath", generate_specific_transformation)
        .add("htmlEntityDecode", generate_specific_transformation)
        .add("urlDecode", generate_specific_transformation)
        .add("min", generate_specific_transformation)
        .add("max", generate_specific_transformation)
        .add("count", generate_specific_transformation)
        .add("length", generate_specific_transformation)
        .add("compressWhitespace", generate_specific_transformation)
        .add("removeWhitespace", generate_specific_transformation)
        .add("trim", generate_specific_transformation)
        .add("trimRight", generate_specific_transformation)
        .add("trimLeft", generate_specific_transformation)
        .add("lowercase", generate_specific_transformation)
        .add("name", generate_specific_transformation)
        .add("names", generate_specific_transformation)
        .add("round", generate_specific_transformation)
        .add("ceil", generate_specific_transformation)
        .add("floor", generate_specific_transformation)
        .add("toString", generate_specific_transformation)
        .add("toInteger", generate_specific_transformation)
        .add("toFloat", generate_specific_transformation)
        ;
}

} // Standard
} // Predicate
} // IronBee
