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
 * @brief IronBee --- CLIPP Generator for PCAP
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE_CLIPP__MODSEC_PCAP_GENERATOR__
#define __IRONBEE_CLIPP__MODSEC_PCAP_GENERATOR__

#include <clipp/input.hpp>

#include <string>

namespace IronBee {
namespace CLIPP {

/**
 * @class PCAPGenerator
 * @brief Input generator from PCAP.
 **/
class PCAPGenerator
{
public:
    //! Default Constructor.
    /**
     * Behavior except for assigning to is undefined.
     **/
    PCAPGenerator();

    //! Constructor.
    /**
     * @param[in] path   PCAP path.
     * @param[in] filter PCAP filter.
     **/
    explicit
    PCAPGenerator(
        const std::string& path,
        const std::string& filter
    );

    //! Produce an input.
    bool operator()(Input::input_p& input) const;
};

} // CLIPP
} // IronBee

#endif
