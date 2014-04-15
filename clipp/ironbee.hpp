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
 * @brief IronBee --- CLIPP IronBee Consumer and Modifier.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE_CLIPP__IRONBEE__
#define __IRONBEE_CLIPP__IRONBEE__

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * CLIPP consumer that feeds inputs to an internal IronBee Engine.
 *
 * This consumer constructs an IronBee engine, loads @a config_path as the
 * configuration file, and then feeds inputs to it.  Each input is treated
 * as a single connection.
 *
 * Only connection_opened, connection_closed, connection_data_in, and
 * connection_data_out events are notified.  This means that the configuration
 * will need to load a parser (e.g., modhtp).
 *
 * As IronBee requires that its inputs be mutable, the input data will be
 * copied to a mutable buffer before being passed in.
 **/
class IronBeeConsumer
{
public:
    explicit
    IronBeeConsumer(const std::string& config_path);

    bool operator()(const Input::input_p& input);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

/**
 * CLIPP consumer that feeds inputs to an internal threaded IronBee Engine.
 *
 * This consumer is as IronBeeConsumer except that it will spawn multiple
 * threads to feed data to IronBee.  It will wait until at least one thread
 * is free, and then pass on the input and return.
 **/
class IronBeeThreadedConsumer
{
public:
    IronBeeThreadedConsumer(
        const std::string& config_path,
        size_t             num_inputs
    );

    bool operator()(const Input::input_p& input);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

/**
 * CLIPP modifier that feeds inputs to an internal IronBee Engine.
 *
 * This behaves as IronBeeConsumer (see above), but as a modifier.  Default
 * behavior is to pass data on, but this can be changed to block.  IronBee
 * rules can use the @c clipp rule action to change behavior on a per-input
 * basis.  The @c clipp rule action takes a parameter: @c pass, @c block, or
 * @c break.
 **/
class IronBeeModifier
{
public:
    //! Behavior in absence of @c clipp rule actions.
    enum behavior_e {
        ALLOW,
        BLOCK
    };

    IronBeeModifier(
        const std::string& config_path,
        behavior_e         behavior = ALLOW
    );

    bool operator()(Input::input_p& input);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
