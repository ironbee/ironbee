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
#include <predicate/merge_graph.hpp>

#include <ironbeepp/operator.hpp>

#include <ironbee/transformation.h>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace  {

static const node_p c_true(new String(""));
static const node_p c_false(new Null());

bool caseless_compare(char a, char b)
{
    return (a == b || tolower(a) == tolower(b));
}

}

string False::name() const
{
    return "false";
}

bool False::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    node_p replacement = c_false;
    merge_graph.replace(me, replacement);

    return true;
}

Value False::calculate(EvalContext)
{
    return Value();
}

string True::name() const
{
    return "true";
}

bool True::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    node_p replacement = c_true;
    merge_graph.replace(me, replacement);

    return true;
}

Value True::calculate(EvalContext)
{
    static node_p s_true_literal;
    if (! s_true_literal) {
        s_true_literal = node_p(new String(""));
        s_true_literal->eval(EvalContext());
    }

    return s_true_literal->value();
}

AbelianCall::AbelianCall() :
    m_ordered(false)
{
    // nop
}

void AbelianCall::add_child(const node_p& child)
{
    if (
        m_ordered &&
        ! less_sexpr()(children().back()->to_s(), child->to_s())
    ) {
        m_ordered = false;
    }
    parent_t::add_child(child);
}

void AbelianCall::replace_child(const node_p& child, const node_p& with)
{
    parent_t::replace_child(child, with);
}

bool AbelianCall::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    bool parent_result =
        parent_t::transform(merge_graph, call_factory, reporter);

    if (m_ordered) {
        return parent_result;
    }

    vector<node_p> new_children(children().begin(), children().end());
    sort(new_children.begin(), new_children.end(), less_node_by_sexpr());
    assert(new_children.size() == children().size());
    if (
        equal(
            new_children.begin(), new_children.end(),
            children().begin()
        )
    ) {
        m_ordered = true;
        return parent_result;
    }

    node_p replacement = call_factory(name());
    boost::shared_ptr<AbelianCall> replacement_as_ac =
        boost::dynamic_pointer_cast<AbelianCall>(replacement);
    if (! replacement_as_ac) {
        // Insanity error so throw exception.
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "CallFactory produced a node of unexpected lineage."
            )
        );
    }
    BOOST_FOREACH(const node_p& child, new_children) {
        replacement->add_child(child);
    }
    replacement_as_ac->m_ordered = true;

    node_p me = shared_from_this();
    merge_graph.replace(me, replacement);

    return true;
}

string Or::name() const
{
    return "or";
}

Value Or::calculate(EvalContext context)
{
    assert(children().size() >= 2);
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->eval(context)) {
            // We are true.
            return True().eval(context);
        }
    }
    return False().eval(context);
}

bool Or::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (child->eval(EvalContext())) {
                node_p replacement = c_true;
                merge_graph.replace(me, replacement);
                return true;
            }
            else {
                to_remove.push_back(child);
            }
        }
    }

    BOOST_FOREACH(const node_p& child, to_remove) {
        result = true;
        merge_graph.remove(me, child);
    }

    if (children().size() == 1) {
        node_p replacement = children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (children().size() == 0) {
        node_p replacement = c_false;
        merge_graph.replace(me, replacement);
        return true;
    }

    return
        AbelianCall::transform(merge_graph, call_factory, reporter) ||
        result;

}

string And::name() const
{
    return "and";
}

Value And::calculate(EvalContext context)
{
    assert(children().size() >= 2);
    BOOST_FOREACH(const node_p& child, children()) {
        if (! child->eval(context)) {
            // We are false.
            return False().eval(context);
        }
    }
    return True().eval(context);
}

bool And::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (! child->eval(EvalContext())) {
                node_p replacement = c_false;
                merge_graph.replace(me, replacement);
                return true;
            }
            else {
                to_remove.push_back(child);
            }
        }
    }

    BOOST_FOREACH(const node_p& child, to_remove) {
        result = true;
        merge_graph.remove(me, child);
    }

    if (children().size() == 1) {
        node_p replacement = children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (children().size() == 0) {
        node_p replacement = c_true;
        merge_graph.replace(me, replacement);
        return true;
    }

    return
        AbelianCall::transform(merge_graph, call_factory, reporter) ||
        result;
}

string Not::name() const
{
    return "not";
}

Value Not::calculate(EvalContext context)
{
    assert(children().size() == 1);
    if (children().front()->eval(context)) {
        return False().eval(context);
    }
    else {
        return True().eval(context);
    }
}

bool Not::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    assert(children().size() == 1);
    const node_p& child = children().front();
    const node_cp& me = shared_from_this();

    if (child->is_literal()) {
        node_p replacement;
        if (child->eval(EvalContext())) {
            replacement.reset(new Null());
        }
        else {
            replacement.reset(new String(""));
        }
        merge_graph.replace(me, replacement);
        return true;
    }
    else {
        return false;
    }
}

string If::name() const
{
    return "if";
}

Value If::calculate(EvalContext context)
{
    assert(children().size() == 3);
    node_list_t::const_iterator i;
    i = children().begin();
    const node_p& pred = *i;
    ++i;
    const node_p& true_value = *i;
    ++i;
    const node_p& false_value = *i;
    if (pred->eval(context)) {
        return true_value->eval(context);
    }
    else {
        return false_value->eval(context);
    }
}

bool If::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    assert(children().size() == 3);
    const node_cp& me = shared_from_this();
    node_list_t::const_iterator i;
    i = children().begin();
    const node_p& pred = *i;
    ++i;
    const node_p& true_value = *i;
    ++i;
    const node_p& false_value = *i;

    if (pred->is_literal()) {
        node_p replacement;
        if (pred->eval(EvalContext())) {
            replacement = true_value;
        }
        else {
            replacement = false_value;
        }
        merge_graph.replace(me, replacement);
        return true;
    }
    else {
        return false;
    }
}

string Field::name() const
{
    return "field";
}

Value Field::calculate(EvalContext context)
{
    Value key_field = children().front()->eval(context);
    IronBee::ConstByteString key = key_field.value_as_byte_string();
    ib_field_t* data_field;
    ib_status_t rc;

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
    ib_tfn_t *transformation;
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

    try {
        IronBee::throw_if_error(
            ib_tfn_lookup_ex(
                environment.ib(),
                name.const_data(), name.length(),
                &(m_data->transformation)
            )
        );
    }
    catch (IronBee::enoent) {
        reporter.error("No such transformation: " + name.to_s());
        return;
    }
}

Value Transformation::calculate(EvalContext context)
{
    Value input = children().back()->eval(context);

    if (! input) {
        return Value();
    }

    ib_field_t* ib_output = NULL;
    ib_flags_t flags;

    IronBee::throw_if_error(
        ib_tfn_transform(
            context.engine().ib(),
            context.memory_pool().ib(),
            m_data->transformation,
            input.ib(),
            &ib_output,
            &flags
        )
    );

    if (ib_output) {
        return Value(ib_output);
    }
    else {
        return Value();
    }
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

string Name::name() const
{
    return "name";
}

Value Name::calculate(EvalContext context)
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

    return Value::create_no_copy_list(
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
        return Value::create_no_copy_list<Value>(
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
    to
        .add<False>()
        .add<True>()
        .add<Or>()
        .add<And>()
        .add<Not>()
        .add<If>()
        .add<Field>()
        .add<Operator>()
        .add<Transformation>()
        .add<Name>()
        .add<List>()
        .add<Sub>()
        .add<SubAll>()
    ;

    // IronBee SpecificOperators
    to
        .add("streq", generate_specific_operator)
        .add("istreq", generate_specific_operator)
        .add("rx", generate_specific_operator)
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
        ;
}

} // Standard
} // Predicate
} // IronBee
