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
 * @brief IronBee --- CLIPP Unparse Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__UNPARSE_MODIFIER__
#define __IRONBEE__CLIPP__UNPARSE_MODIFIER__

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

//! Convert parsed events to connection data events.
class UnparseModifier
{
public:
    //! Call operator.
    bool operator()(Input::input_p& in_out);

    /**
     * Unparse header list into header text block.
     *
     * @param[out] out     String to append text to.
     * @param[in]  headers Headers to unparse.
     **/
    static
    void unparse_headers(
        std::string&                out,
        const Input::header_list_t& headers
    );

    /**
     * Unparse request line into text block.
     *
     * @param[out] out   String to append text to.
     * @param[in]  event RequestEvent to unparse.
     **/
    static
    void unparse_request_line(
        std::string&               out,
        const Input::RequestEvent& event
    );

    /**
     * Unparse response line into text block.
     *
     * @param[out] out   String to append text to.
     * @param[in]  event ResponseEvent to unparse.
     **/
    static
    void unparse_response_line(
        std::string&               out,
        const Input::ResponseEvent& event
    );
};

} // CLIPP
} // IronBee

#endif
