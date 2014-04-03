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

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>
#include <predicate/value.hpp>

#include <ironbee/log.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

using namespace std;
using boost::bind;
using boost::ref;

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Output children and take value of final child.
 **/
class P :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const
    {
        return "p";
    }

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const
    {
        return Validate::n_or_more_children(reporter, 1);
    }

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const
    {
        list<string> value_strings;
        BOOST_FOREACH(const node_p& n, children()) {
            value_strings.push_back(
                graph_eval_state.eval(n, context).to_s()
            );
        }

         cerr << boost::algorithm::join(value_strings, "; ") << endl;

         const NodeEvalState& primary =
             graph_eval_state.final(children().back()->index());
         NodeEvalState& me = graph_eval_state[index()];
         if (primary.is_finished()) {
             if (! me.is_aliased()) {
                 me.finish(primary.value());
             }
             else {
                 me.finish();
             }
         }
         else {
             Value v = primary.value();
             assert(! v.to_field() || v.type() == Value::LIST);
             if (v.to_field()) {
                 me.alias(v);
             }
         }
    }
};


/**
 * Take values of child; do not transform.
 **/
class Identity :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const
    {
        return "identity";
    }

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const
    {
        return Validate::n_children(reporter, 1);
    }

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const
    {
        const NodeEvalState& primary =
            graph_eval_state.final(children().front()->index());
        NodeEvalState& me = graph_eval_state[index()];
        if (primary.is_finished()) {
            if (! me.is_aliased()) {
                me.finish(primary.value());
            }
            else {
                me.finish();
            }
        }
        else {
            Value v = primary.value();
            assert(! v.to_field() || v.type() == Value::LIST);
            if (v.to_field()) {
                me.alias(v);
            }
        }
    }
};


/**
 * Sequence of values; potentially infinite.
 **/
class Sequence :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_initialize().
    virtual void eval_initialize(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

string Sequence::name() const
{
    return "sequence";
}

void Sequence::eval_initialize(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& node_eval_state = graph_eval_state[index()];
    node_eval_state.state() =
        literal_value(children().front()).as_number();
    node_eval_state.setup_local_list(context.memory_manager());
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

void Sequence::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    // Figure out parameters.
    int64_t start;
    int64_t end = -1;
    int64_t step = 1;

    node_list_t::const_iterator i = children().begin();
    start = literal_value(*i).as_number();
    ++i;
    if (i != children().end()) {
        end = literal_value(*i).as_number();
        ++i;
        if (i != children().end()) {
            step = literal_value(*i).as_number();
        }
    }
    else {
        end = start - 1;
    }

    // Output current.
    ib_num_t current = boost::any_cast<ib_num_t>(my_state.state());
    my_state.append_to_list(
        Value::create_number(context.memory_manager(), current)
    );

    // Advance current.
    current += step;
    my_state.state() = current;

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
        (step > 0 && current > end)  ||
        (step < 0 && current < end)
    ) {
        my_state.finish();
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
