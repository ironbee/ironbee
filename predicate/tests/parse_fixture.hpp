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
 * @brief Predicate --- Common test fixture.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/predicate/bfs.hpp>
#include <ironbee/predicate/eval.hpp>
#include <ironbee/predicate/parse.hpp>

#ifndef __PREDICATE__TEST_PARSE_FIXTURE__
#define __PREDICATE__TEST_PARSE_FIXTURE__

class NamedCall : public IronBee::Predicate::Call
{
public:
    explicit
    NamedCall(const std::string& name) :
        m_name(name)
    {
        // nop
    }

    virtual std::string name() const
    {
        return m_name;
    }

protected:
    virtual void eval_calculate(
        IronBee::Predicate::GraphEvalState& graph_eval_state,
        IronBee::Predicate::EvalContext     context
    ) const
    {
        graph_eval_state[index()].finish();
    }

private:
    std::string m_name;
};

class ParseFixture
{
protected:
    static
    IronBee::Predicate::call_p create(
        const std::string& name
    )
    {
        return IronBee::Predicate::call_p(new NamedCall(name));
    }

    IronBee::Predicate::node_p parse(const std::string& s) const
    {
        size_t i = 0;
        IronBee::Predicate::node_p node = parse_call(s, i, m_factory);
        if (! node) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << IronBee::errinfo_what(
                    "Parse failed."
                )
            );
        }
        if (i != s.length() - 1) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << IronBee::errinfo_what(
                    "Parse did not consume all input."
                )
            );

        }

        return node;
    }

    IronBee::Predicate::CallFactory& factory()
    {
        return m_factory;
    }

    const IronBee::Predicate::CallFactory& factory() const
    {
        return m_factory;
    }

private:
    IronBee::Predicate::CallFactory m_factory;
};

#endif
