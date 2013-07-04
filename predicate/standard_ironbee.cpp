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
 * @brief Predicate --- Standard IronBee Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_ironbee.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/validate.hpp>

#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

struct Field::data_t {
    bool   is_indexed;
    size_t index;
};

Field::Field() :
    m_data(new data_t)
{
    // nop
}

string Field::name() const
{
    return "field";
}

bool Field::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 1) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

void Field::pre_eval(Environment environment, NodeReporter reporter)
{
    const ib_data_config_t* config;
    ib_status_t rc;
    size_t i;

    config = ib_engine_data_config_get_const(environment.ib());
    assert(config != NULL);

    // Key must be static.
    Value key_field = literal_value(children().front());
    IronBee::ConstByteString key = key_field.value_as_byte_string();

    rc = ib_data_lookup_index_ex(
        config,
        key.const_data(), key.size(),
        &i
    );

    if (rc == IB_ENOENT) {
        /* Not an indexed field. */
        m_data->is_indexed = false;
    }
    else if (rc == IB_OK) {
        m_data->is_indexed = true;
        m_data->index = i;
    }
    else {
        IronBee::throw_if_error(rc);
    }
}

void Field::calculate(EvalContext context)
{
    Value key_field = literal_value(children().front());
    IronBee::ConstByteString key = key_field.value_as_byte_string();
    ib_field_t* data_field;
    ib_status_t rc;

    if (m_data->is_indexed) {
        IronBee::throw_if_error(
            ib_data_get_indexed(
                context.ib()->data,
                m_data->index,
                &data_field
            )
        );
    }
    else {
        rc = ib_data_get_ex(
            context.ib()->data,
            key.const_data(), key.size(),
            &data_field
        );
        if (rc == IB_ENOENT) {
            finish();
            return;
        }
        else {
            IronBee::throw_if_error(rc);
        }
    }

    add_value(Value(data_field));
    finish();
}

struct Operator::data_t
{
    ConstOperator op;
    void*         instance_data;
};

Operator::Operator() :
    m_data(new data_t())
{
    // nop
}

string Operator::name() const
{
    return "operator";
}

bool Operator::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 3) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;
    result = Validate::nth_child_is_string(reporter, 1) && result;

    return result;
}

void Operator::pre_eval(Environment environment, NodeReporter reporter)
{
    // Validation guarantees that the first two children are string
    // literals and thus can be evaluated with default EvalContext.

    node_list_t::const_iterator child_i = children().begin();
    Value op_name_value = literal_value(*child_i);
    ++child_i;
    Value params_value = literal_value(*child_i);

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

Value Operator::value_calculate(Value v, EvalContext context)
{
    static const char* c_capture_name = "predicate_operator_capture";

    if (! m_data) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Evaluation without pre evaluation!"
            )
        );
    }

    IronBee::Field capture = IronBee::Field::create_no_copy_list<void *>(
        context.memory_pool(),
        c_capture_name,
        sizeof(c_capture_name) - 1,
        IronBee::List<void *>::create(context.memory_pool())
    );

    int success = m_data->op.execute_instance(
        m_data->instance_data,
        context,
        v,
        capture
    );
    if (success) {
        return capture;
    }
    else {
        return Value();
    }
}

void Operator::calculate(EvalContext context)
{
    const node_p& input_node = children().back();

    map_calculate(input_node, context);
}

struct Transformation::data_t
{
    ConstTransformation transformation;
};

Transformation::Transformation() :
    m_data(new data_t())
{
    // nop
}

string Transformation::name() const
{
    return "transformation";
}

bool Transformation::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

void Transformation::pre_eval(Environment environment, NodeReporter reporter)
{
    // Validation guarantees that the first child is a string interval
    // and thus can be evaluated with default EvalContext.

    Value name_value = literal_value(children().front());
    ConstByteString name = name_value.value_as_byte_string();

    if (! name) {
        reporter.error("Missing transformation name.");
        return;
    }

    m_data->transformation = ConstTransformation::lookup(
        environment, string(name.const_data(), name.length()).c_str()
    );
}

Value Transformation::value_calculate(Value v, EvalContext context)
{
    if (! m_data) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Reset without pre evaluation!"
            )
        );
    }

    return m_data->transformation.execute(context.memory_pool(), v);
}

void Transformation::calculate(EvalContext context)
{
    map_calculate(children().back(), context);
}

void load_ironbee(CallFactory& to)
{
    to
        .add<Field>()
        .add<Operator>()
        .add<Transformation>()
        ;
}

} // Standard
} // Predicate
} // IronBee
