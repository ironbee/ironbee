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
 * @brief IronBee --- CLIPP Connection Modifiers
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__CONNECTION_MODIFIERS__
#define __IRONBEE__CLIPP__CONNECTION_MODIFIERS__

#include <clipp/input.hpp>

#include <string>
#include <vector>

namespace IronBee {
namespace CLIPP {

//! Change local_ip of connection opened events.
class SetLocalIPModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] ip IP to use for all connection opened events.
     **/
    explicit
    SetLocalIPModifier(const std::string& ip);

    //! Call operator.
    /**
     * Changes local IP address of all connection opened events of @a in_out.
     *
     * @param[in,out] in_out Input to modify.
     **/
    bool operator()(Input::input_p& in_out);

private:
    std::string m_ip;
};

//! Change local_port of connection opened events.
class SetLocalPortModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] port Port to use for all connection opened events.
     **/
    explicit
    SetLocalPortModifier(uint32_t port);

    //! Call operator.
    /**
     * Changes local port of all connection opened events of @a in_out.
     *
     * @param[in,out] in_out Input to modify.
     **/
    bool operator()(Input::input_p& in_out);

private:
    uint32_t m_port;
};

//! Change remote_ip of connection opened events.
class SetRemoteIPModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] ip IP to use for all connection opened events.
     **/
    explicit
    SetRemoteIPModifier(const std::string& ip);

    //! Call operator.
    /**
     * Changes remote IP address of all connection opened events of @a in_out.
     *
     * @param[in,out] in_out Input to modify.
     **/
    bool operator()(Input::input_p& in_out);

private:
    std::string m_ip;
};

//! Change remote_port of connection opened events.
class SetRemotePortModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] port Port to use for all connection opened events.
     **/
    explicit
    SetRemotePortModifier(uint32_t port);

    //! Call operator.
    /**
     * Changes remote port of all connection opened events of @a in_out.
     *
     * @param[in,out] in_out Input to modify.
     **/
    bool operator()(Input::input_p& in_out);

private:
    uint32_t m_port;
};


} // CLI
} // IronBee

#endif
