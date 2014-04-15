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
 * @brief IronBee --- CLIPP Time Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__TIME_MODIFIER__
#define __IRONBEE__CLIPP__TIME_MODIFIER__

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * Output timing information.
 *
 * Outputs id, a time stamp and difference from previous timestamp for every
 * input.  Does not modify input.
 **/
class TimeModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] out ostream to output to.
     **/
    explicit
    TimeModifier(std::ostream* out = &std::cout);

    //! Process an input.
    bool operator()(Input::input_p& input);

public:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
