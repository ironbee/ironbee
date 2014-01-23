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
 * @brief Predicate --- Standard implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard.hpp>

#include <predicate/standard_boolean.hpp>
#include <predicate/standard_development.hpp>
#include <predicate/standard_filter.hpp>
#include <predicate/standard_ironbee.hpp>
#include <predicate/standard_predicate.hpp>
#include <predicate/standard_string.hpp>
#include <predicate/standard_template.hpp>
#include <predicate/standard_valuelist.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

void load(CallFactory& to)
{
    load_boolean(to);
    load_development(to);
    load_filter(to);
    load_ironbee(to);
    load_template(to);
    load_valuelist(to);
    load_predicate(to);
    load_string(to);
}

} // Standard
} // Predicate
} // IronBee
