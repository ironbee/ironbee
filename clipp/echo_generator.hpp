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
 * @brief IronBee --- CLIPP Echo Generator
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__ECHO_GENERATOR__
#define __IRONBEE__CLIPP__ECHO_GENERATOR__

#include <clipp/input.hpp>

#include <string>
#include <vector>

namespace IronBee {
namespace CLIPP {

/**
 * @class EchoGenerator
 * \brief Input generator from a single string.
 *
 * Will use bogus connection information.  Response is empty.  Argument is
 * request line.
 *
 * This produces a single input.
 **/
class EchoGenerator
{
public:
    //! Local IP address to use for raw inputs.
    static const std::string local_ip;
    //! Remote IP address to use for raw inputs.
    static const std::string remote_ip;
    //! Remote port to use for raw inputs.
    static const uint16_t    local_port;
    //! Remote port to use for raw inputs.
    static const uint16_t    remote_port;

    //! Default Constructor.
    /**
     * Behavior except for assigning to is undefined.
     **/
    EchoGenerator();

    //! Constructor.
    /**
     * @param[in] request_line Request line.
     **/
    EchoGenerator(const std::string& request_line);

    //! Produce an input.  See input_t and input_generator_t.
    bool operator()(Input::input_p& out_input);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLI
} // IronBee

#endif
