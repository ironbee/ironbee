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
 * @brief IronBee &mdash; CLIPP IronBee Consumer
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE_CLIPP__IRONBEE_CONSUMER__
#define __IRONBEE_CLIPP__IRONBEE_CONSUMER__

#include "input.hpp"

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
    IronBeeConsumer();

    explicit
    IronBeeConsumer(const std::string& config_path);

    bool operator()(const input_p& input);

private:
    struct EngineState;
    boost::shared_ptr<EngineState> m_engine_state;
};

} // CLIPP
} // IronBee

#endif
