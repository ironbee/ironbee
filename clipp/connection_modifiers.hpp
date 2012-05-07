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
 * @brief IronBee &mdash; CLIPP Connection Modifiers
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__CONNECTION_MODIFIERS__
#define __IRONBEE__CLIPP__CONNECTION_MODIFIERS__

#include "input.hpp"

#include <string>
#include <vector>

namespace IronBee {
namespace CLIPP {

//! Change local_ip of connection information.
class SetLocalIPModifier
{
public:
    explicit
    SetLocalIPModifier(const std::string& ip);

    bool operator()(input_p& in_out);

private:
    std::string m_ip;
};

} // CLI
} // IronBee

#endif
