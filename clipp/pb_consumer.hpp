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
 * @brief IronBee --- CLIPP Protobuf Consumer
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE_CLIPP__PB_CONSUMER__
#define __IRONBEE_CLIPP__PB_CONSUMER__

#include <iostream>

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * CLIPP consumer that writes inputs to a protobuf stream.
 *
 * This consumer writes inputs to a protobuf stream.  A protobuf stream is the
 * size of the message in as a uint32_t followed by the message as a gzipped
 * protobuf Input object (see clipp.proto).
 **/
class PBConsumer
{
public:
    PBConsumer();

    explicit
    PBConsumer(const std::string& output_path);

    explicit
    PBConsumer(std::ostream& output);

    bool operator()(const Input::input_p& input);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
