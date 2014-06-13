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
 * @brief IronBee --- ClippTest support module
 *
 * Module with actions that are useful for testing, especially clipp_test.
 *
 * @note This module is automatically loaded by clipp_test.
 *
 * - The `clipp_announce` action takes an expansion string and outputs it
 *   to standard out if fired.
 * - The `clipp_print` operator prints a message and its input to standard
 *   out.
 * - The `clipp_print_type` operator prints a message and the type of its
 *   input to standard out.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/all.hpp>

using namespace std;
using namespace IronBee;

namespace {

//! clipp_announce action name.
const char* c_clipp_announce = "clipp_announce";
//! clipp_print operator name.
const char* c_clipp_print = "clipp_print";
//! clipp_print_type operator name.
const char* c_clipp_print_type = "clipp_print_type";

//! Called on module load.
void module_load(IronBee::Module module);

} // Anonymous

IBPP_BOOTSTRAP_MODULE("clipp_test_support", module_load)

// Implementation

namespace {

void clipp_announce_action_instance(
    ConstVarExpand        var_expand,
    const ib_rule_exec_t* rule_exec
)
{
    Transaction tx(rule_exec->tx);
    cout << "CLIPP ANNOUNCE: "
         << var_expand.execute_s(
               tx.memory_manager(),
               tx.var_store()
            )
         << endl;
}

Action::action_instance_t clipp_announce_action_generator(
    Engine        engine,
    MemoryManager mm,
    const char*   parameters
)
{
    return boost::bind(
        clipp_announce_action_instance,
        VarExpand::acquire(
            mm,
            parameters, strlen(parameters),
            engine.var_config()
        ),
        _1
    );
}

int clipp_print_type_op_executor(
    const char* args,
    ConstField field
)
{
    std::string type_name;

    if (field) {
        switch(field.type()) {
            case ConstField::GENERIC:
                type_name = "GENERIC";
                break;
            case ConstField::NUMBER:
                type_name = "NUMBER";
                break;
            case ConstField::TIME:
                type_name = "TIME";
                break;
            case ConstField::FLOAT:
                type_name = "FLOAT";
                break;
            case ConstField::NULL_STRING:
                type_name = "STRING";
                break;
            case ConstField::BYTE_STRING:
                type_name = "BYTE_STRING";
                break;
            case ConstField::LIST:
                type_name = "LIST";
                break;
            case ConstField::STREAM_BUFFER:
                type_name = "STREAM_BUFFER";
                break;
            default:
                type_name = "UNSUPPORTED TYPE";
        }
    }
    else {
        type_name = "NULL";
    }

    cout << "clipp_print_type [" << args << "]: " << type_name << endl;
    return 1;
}

Operator::operator_instance_t clipp_print_type_op_generator(
    Context,
    MemoryManager,
    const char* args
)
{
    return boost::bind(
        clipp_print_type_op_executor,
        args,
        _2
    );
};

int clipp_print_op_executor(
    const char* args,
    ConstField field
)
{
    cout << "clipp_print [" << args << "]: "
         << (field ? field.to_s() : "NULL")
         << endl;

    return 1;
}

Operator::operator_instance_t clipp_print_op_generator(
    Context,
    MemoryManager,
    const char* args
)
{
    return boost::bind(
        clipp_print_op_executor,
        args,
        _2
    );
};

void module_load(IronBee::Module module)
{
    MemoryManager mm = module.engine().main_memory_mm();

    Action::create(
        mm,
        c_clipp_announce,
        boost::bind(clipp_announce_action_generator, module.engine(), _1, _3)
    ).register_with(module.engine());

    Operator::create(
        mm,
        c_clipp_print,
        IB_OP_CAPABILITY_ALLOW_NULL,
        clipp_print_op_generator
    ).register_with(module.engine());
    Operator::create(
        mm,
        c_clipp_print,
        IB_OP_CAPABILITY_ALLOW_NULL,
        clipp_print_op_generator
    ).register_stream_with(module.engine());

    Operator::create(
        mm,
        c_clipp_print_type,
        IB_OP_CAPABILITY_ALLOW_NULL,
        clipp_print_type_op_generator
    ).register_with(module.engine());
    Operator::create(
        mm,
        c_clipp_print_type,
        IB_OP_CAPABILITY_ALLOW_NULL,
        clipp_print_type_op_generator
    ).register_stream_with(module.engine());
}

} // Anonymous
