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
 * @brief IronBee --- CLIPP Limit Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <clipp/limit_modifier.hpp>
#include <clipp/control.hpp>

#include <boost/make_shared.hpp>

using namespace std;

namespace IronBee {
namespace CLIPP {

struct LimitModifier::State
{
    size_t n;
};

LimitModifier::LimitModifier(size_t n) :
    m_state(boost::make_shared<State>())
{
    m_state->n = n;
}

bool LimitModifier::operator()(Input::input_p& input)
{
    // It's important to let NULLs through even if n == 0 to allow for
    // agregate-style modifiers later in the chain.  However, it is good
    // practice to put the limit modifier last in the chain.
    if (! input) {
        return true;
    }

    if (m_state->n == 0) {
        throw clipp_break();
    }

    --m_state->n;

    return true;
}

} // CLIPP
} // IronBee
