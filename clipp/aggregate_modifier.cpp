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
 * @brief IronBee &mdash; CLIPP Aggregate Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "aggregate_modifier.hpp"

using namespace std;

namespace IronBee {
namespace CLIPP {

struct AggregateModifier::State
{
    State(size_t n_) :
        n(n_)
    {
        // nop
    }

    const size_t   n;
    Input::input_p aggregate;
};

AggregateModifier::AggregateModifier(size_t n) :
    m_state(new State(n))
{
    // nop
}

bool AggregateModifier::operator()(Input::input_p& input)
{
    if (! input) {
        if (m_state->aggregate) {
            input.swap(m_state->aggregate);
        }
        return true;
    }

    if (! m_state->aggregate) {
        m_state->aggregate = input;
    }
    else {
        copy(
            input->connection.transactions.begin(),
            input->connection.transactions.end(),
            back_inserter(m_state->aggregate->connection.transactions)
        );
    }

    if (
        m_state->n != 0 &&
        m_state->aggregate->connection.transactions.size() >= m_state->n
    )
    {
        input.swap(m_state->aggregate);
        m_state->aggregate.reset();
        return true;
    }

    return false;
}

} // CLIPP
} // IronBee