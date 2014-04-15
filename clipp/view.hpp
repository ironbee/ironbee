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
 * @brief IronBee --- CLIPP View Consumer
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE_CLIPP__VIEW_CONSUMER__
#define __IRONBEE_CLIPP__VIEW_CONSUMER__

#include <clipp/input.hpp>

#include <boost/shared_ptr.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * CLIPP consumer that writes inputs to cout in human readable form.
 *
 * view:id displays only the id.
 **/
class ViewConsumer
{
public:
    explicit
    ViewConsumer(const std::string& arg);

    bool operator()(const Input::input_p& input);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

/**
 * CLIPP modifier that writes inputs to cout in human readable form.
 *
 * view:id displays only the id.
 **/
class ViewModifier
{
public:
    explicit
    ViewModifier(const std::string& arg);

    bool operator()(Input::input_p& input);

private:
    ViewConsumer m_consumer;
};

} // CLIPP
} // IronBee

#endif
