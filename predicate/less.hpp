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
 * @brief Predicate --- Order S-Expressions
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__LESS__
#define __PREDICATE__LESS__

#include <string>

namespace IronBee {
namespace Predicate {

/**
 * Comparsion functional for S-Expressions.  See operator()().
 **/
struct less_sexpr {
    // Intentionally inline.
    /**
     * Canonical order of S-Expressions.
     *
     * Order S-Expressions by length and then lexicographically by
     * alternating prefix and suffix characters, i.e., first, last, second,
     * second to last, ...
     *
     * The issue with using less<string> for ordering S-Expressions is that it
     * can perform poorly in situations where strings are often prefixes of
     * other strings; a situation true in S-Expressions.
     *
     * @param[in] a First string to compare.
     * @param[in] b Second string to compare.
     * @return true iff a < b in the order described above.
     **/
    inline bool operator()(const std::string& a, const std::string& b) const
    {
        size_t a_length = a.length();
        size_t b_length = b.length();
        if (a_length < b_length) {
            return true;
        }
        if (a_length > b_length) {
            return false;
        }
        // a_length == b_length
        for (size_t i = 0; i < a_length / 2; ++i) {
            if (a[i] < b[i]) {
                return true;
            }
            if (a[i] > b[i]) {
                return false;
            }
            if (a[a_length-i] < b[a_length-i]) {
                return true;
            }
            if (a[a_length-i] > b[a_length-i]) {
                return false;
            }
        }
        if ((a_length % 2) && (a[a_length/2] < b[a_length/2])) {
            return true;
        }

        return false;
    }
};

} // Predicate
} // IronBee

#endif
