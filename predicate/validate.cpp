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
 * @brief Predicate --- Validate implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "validate.hpp"

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Validate {

void validate_n_children(NodeReporter& reporter, size_t n)
{
    size_t actual_children = reporter.node()->children().size();
    if (actual_children != n) {
        reporter.error(
            "Expected " + boost::lexical_cast<string>(n) + " children " +
            " but have " + boost::lexical_cast<string>(actual_children) +
            "."
        );
    }
}

} // Validate
} // Predicate
} // IronBee
