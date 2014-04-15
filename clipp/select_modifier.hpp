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
 * @brief IronBee --- CLIPP Select Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__SELECT_MODIFIER__
#define __IRONBEE__CLIPP__SELECT_MODIFIER__

#include <clipp/input.hpp>

#include <list>

namespace IronBee {
namespace CLIPP {

//! Select certain inputs.
class SelectModifier
{
public:
    //! Range of indices.  Inclusive: [first, second]
    typedef std::pair<size_t, size_t> range_t;
    //! List of ranges.
    typedef std::list<range_t> range_list_t;

    /**
     * Constructor.
     *
     * Indices are 1 based.
     *
     * @param[in] select Which inputs to select.
     **/
    explicit
    SelectModifier(const range_list_t& select);

    //! Call operator.
    bool operator()(Input::input_p& in_out);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
