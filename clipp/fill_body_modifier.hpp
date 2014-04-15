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
 * @brief IronBee --- CLIPP Fill Body Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__FILL_BODY_MODIFIER__
#define __IRONBEE__CLIPP__FILL_BODY_MODIFIER__

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * Fill bodies with @s.
 **/
class FillBodyModifier
{
public:
    //! Call operator.
    bool operator()(Input::input_p& in_out);
};

} // CLIPP
} // IronBee

#endif
