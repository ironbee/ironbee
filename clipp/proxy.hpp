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
 * @brief IronBee --- CLIPP Proxy Consumer
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#ifndef __IRONBEE_CLIPP__PROXY_CONSUMER__
#define __IRONBEE_CLIPP__PROXY_CONSUMER__

#include <clipp/input.hpp>

#include <boost/shared_ptr.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * CLIPP consumer that will act as a client and origin server for an external
 * http proxy.
 *
 **/
class ProxyConsumer
{
public:
    explicit
    ProxyConsumer(const std::string& proxy_host,
                  uint32_t proxy_port,
                  uint32_t listen_port);

    bool operator()(const Input::input_p& input);

private:
    const std::string m_proxy_host;
    uint32_t m_proxy_port;
    uint32_t m_listen_port;;
};

} // CLIPP
} // IronBee

#endif
