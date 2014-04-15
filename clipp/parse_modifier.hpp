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
 * @brief IronBee --- CLIPP Parse Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__PARSE_MODIFIER__
#define __IRONBEE__CLIPP__PARSE_MODIFIER__

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

//! Convert connection data events to parsed events.
class ParseModifier
{
public:
    //! Call operator.
    bool operator()(Input::input_p& in_out);

    /**
     * Parse block of header text into list of headers.
     *
     * @param[out]    headers List to append headers to.
     * @param[in,out] begin   Beginning of block; will be updated to point to
     *                        just past end of headers.
     * @param[end]    end     End of block.  Can be past last header.
     **/
    static
    void parse_header_block(
        Input::header_list_t& headers,
        const char*& begin, const char* end
    );

    /**
     * Parse request line block into RequestEvent.  Does not set @c which.
     *
     * @param[out] event Event to fill.
     * @param[in]  begin Beginning of block.
     * @param[in]  end   End of block.
     **/
    static
    void parse_request_line(
        Input::RequestEvent& event,
        const char* begin, const char* end
    );

    /**
     * Parse response line block into ResponseEvent.  Does not set @c which.
     *
     * @param[out] event Event to fill.
     * @param[in]  begin Beginning of block.
     * @param[in]  end   End of block.
     **/
    static
    void parse_response_line(
        Input::ResponseEvent& event,
        const char* begin, const char* end
    );
};

} // CLIPP
} // IronBee

#endif
