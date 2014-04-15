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
 * @brief IronBee --- CLIPP Time Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "time_modifier.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>

using namespace std;

namespace IronBee {
namespace CLIPP {

struct TimeModifier::State
{
    //! Output stream.
    ostream* out;

    //! Start of modifier.
    boost::posix_time::ptime start_at;

    //! Last input time.
    boost::posix_time::ptime last_at;
};

TimeModifier::TimeModifier(ostream* out) :
    m_state(new State())
{
    m_state->out      = out;
    m_state->last_at  =
        boost::posix_time::microsec_clock::universal_time();
    m_state->start_at = m_state->last_at;
}

bool TimeModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    boost::posix_time::ptime now =
        boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::time_duration since_start = now - m_state->start_at;
    boost::posix_time::time_duration since_last  = now - m_state->last_at;

    (*m_state->out) << boost::format("%s %8d ms %8d ms\n") %
            input->id %
            since_start.total_microseconds() %
            since_last.total_microseconds()
            ;

    m_state->last_at = now;

    return true;
}

} // CLIPP
} // IronBee
