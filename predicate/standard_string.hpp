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
 * @brief Predicate --- Standard String
 *
 * See reference.md for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_STRING__
#define __PREDICATE__STANDARD_STRING__

#include <predicate/call_factory.hpp>
#include <predicate/dag.hpp>
#include <predicate/meta_call.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Regexp based replacement.
 *
 * First child is expression, second child is replacement, third child is
 * text.  Result is substitution applied to each string value of text child.
 **/
class StringReplaceRx :
    public MapCall
{
public:
    //! Constructor.
    StringReplaceRx();

    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::pre_eval()
    virtual void pre_eval(Environment environment, NodeReporter reporter);

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

protected:
    //! See MapCall::value_calculate()
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * Load all standard string calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_string(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
