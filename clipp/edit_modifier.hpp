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
 * @brief IronBee --- CLIPP Edit Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__EDIT_MODIFIER__
#define __IRONBEE__CLIPP__EDIT_MODIFIER__

#include <clipp/input.hpp>

#include <boost/shared_ptr.hpp>

namespace IronBee {
namespace CLIPP {

//! Edit portions of events.
class EditModifier
{
public:
    /**
     * Constructor.
     *
     * Possible values for @a what:
     * - request -- Request line.
     * - request_body -- Request body.
     * - request_header -- Request headers.
     * - response -- Response line.
     * - response_body -- Response body.
     * - response_header -- Response headers.
     * - connection_in -- Connection Data in
     * - connection_out -- Connection data out.
     **/
    explicit
    EditModifier(const std::string& what);

    //! Call operator.
    bool operator()(Input::input_p& in_out);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
