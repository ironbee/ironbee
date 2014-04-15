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
 * @brief IronBee --- CLIPP Header Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__HEADER_MODIFIER__
#define __IRONBEE__CLIPP__HEADER_MODIFIER__

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * Set header.
 **/
class SetModifier
{
public:
    //! Which type of header to modify.
    enum which_e {
        BOTH,    //!< Apply to both request and response.
        REQUEST, //!< Apply to request only.
        RESPONSE //!< Apply to response only.
    };

    //! What mode of modifcation.
    enum mode_e {
        REPLACE_EXISTING, //!< Only replace existing headers.
        ADD,              //!< Add additional headers.
        ADD_MISSING,      //!< Add if no existing headers.
    };

    /**
     * Constructor.
     *
     * @param[in] which Which type to modify.
     * @param[in] mode  What mode to run in.
     * @param[in] key   Key of header.
     * @param[in] value Value of header.
     **/
    SetModifier(
        which_e            which,
        mode_e             mode,
        const std::string& key,
        const std::string& value
    );

    //! Call operator.
    bool operator()(Input::input_p& in_out);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
