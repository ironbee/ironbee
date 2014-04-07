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
 * @brief Predicate --- Standard Predicate Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_predicate.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/functional.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

//! Scoped Memory Pool Lite
static ScopedMemoryPoolLite s_mpl;
//! True Value
static const Value c_true_value =
    Value::create_string(s_mpl, ByteString::create(s_mpl, ""));
//! True literal.
static const node_p c_true(new Literal(c_true_value));
//! False literal.
static const node_p c_false(new Literal());

/**
 * Is argument a literal?
 **/
class IsLiteral :
    public Call
{
public:
    //! See Call::name()
    std::string name() const
    {
        return "isLiteral";
    }

    /**
     * See Node::transform().
     *
     * Will replace self with true or false based on child.
     **/
    bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        Environment        environment,
        NodeReporter       reporter
    )
    {
        node_p me = shared_from_this();
        node_p replacement = c_false;
        if (children().front()->is_literal()) {
            replacement = c_true;
        }
        merge_graph.replace(me, replacement);

        return true;
    }

    //! See Node::eval_calculate()
    void eval_calculate(GraphEvalState&, EvalContext) const
    {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "IsLiteral evaluated.  Did you not transform?"
            )
        );
    }

    //! See Node::validate().
    bool validate(NodeReporter reporter) const
    {
        return Validate::n_children(reporter, 1);
    }
};

/**
 * Is argument finished?
 **/
class IsFinished :
    public Functional::Primary
{
public:
    //! Constructor.
    IsFinished() : Functional::Primary(0, 1) {}

protected:
    //! See Functional::Primary::eval_primary().
    void eval_primary(
        MemoryManager                  mm,
        const node_cp&                 me,
        boost::any&                    substate,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        const NodeEvalState&           primary_arg
    ) const
    {
        if (primary_arg.is_finished()) {
            my_state.finish(c_true_value);
        }
    }
};

/**
 * Is primary argument a list longer than specified length.
 **/
class IsLonger :
    public Functional::Primary
{
public:
    //! Constructor.
    IsLonger() : Functional::Primary(0, 2) {}

protected:
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::NUMBER, reporter);
        }
    }

    //! See Functional::Primary::eval_primary().
    void eval_primary(
        MemoryManager                  mm,
        const node_cp&                 me,
        boost::any&                    substate,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        const NodeEvalState&           primary_arg
    ) const
    {
        if (! primary_arg.value()) {
            return;
        }
        if (primary_arg.value().type() != Value::LIST) {
            my_state.finish();
            return;
        }
        if (
            primary_arg.value().as_list().size() >
            size_t(secondary_args[0].as_number())
        ) {
            my_state.finish(c_true_value);
            return;
        }
        if (primary_arg.is_finished()) {
            my_state.finish();
        }
    }
};

/**
 * Is argument a list?
 **/
class IsList :
    public Functional::Primary
{
public:
    //! Constructor.
    IsList() : Functional::Primary(0, 1) {}

protected:
    //! See Functional::Primary::eval_primary().
    void eval_primary(
        MemoryManager                  mm,
        const node_cp&                 me,
        boost::any&                    substate,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        const NodeEvalState&           primary_arg
    ) const
    {
        if (! primary_arg.value().is_null()) {
            if (primary_arg.value().type() == Value::LIST) {
                my_state.finish(c_true_value);
            }
            else {
                my_state.finish();
            }
        }
    }
};

} // Anonymous

void load_predicate(CallFactory& to)
{
    to
        .add<IsLiteral>()
        .add("isFinished", Functional::generate<IsFinished>)
        .add("isLonger", Functional::generate<IsLonger>)
        .add("isList", Functional::generate<IsList>)
        ;
}

} // Standard
} // Predicate
} // IronBee
