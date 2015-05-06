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

#include <ironbee/predicate/dag.hpp>

#include <string>

namespace IronBee {
namespace Predicate {

/**
 * Comparison functional for S-Expressions.  See operator()().
 **/
struct less_sexpr {
    // Intentionally inline.
    /**
     * Canonical order of S-Expressions.
     *
     * Order S-Expressions by length and then lexicographically by
     * alternating characters from the start and the middle of the strings.
     * This, admittedly confusing, ordering was chosen after profiling
     * S-Expressions gathered from live systems.
     *
     * The issue with using less<string> for ordering S-Expressions is that it
     * can perform poorly in situations where strings are often prefixes of
     * other strings; a situation true in S-Expressions.
     *
     * @param[in] a First string to compare.
     * @param[in] b Second string to compare.
     * @return true iff a < b in the order described above.
     **/
    bool operator()(const std::string& a, const std::string& b) const
    {
        const size_t a_length = a.length();
        const size_t b_length = b.length();
        const size_t h_length = a_length / 2;

        if (a_length < b_length) {
            return true;
        }
        if (a_length > b_length) {
            return false;
        }

        // Check the middle character for odd-length strings.
        if (a_length % 2) {
            if (a[h_length] < b[h_length]) {
                return true;
            }

            if (a[h_length] > b[h_length]) {
                return false;
            }
        }

        // Check from 0 and the middle of the string to the end.
        for (size_t i = 0; i < h_length; ++i) {
            if (a[i] < b[i]) {
                return true;
            }
            if (a[i] > b[i]) {
                return false;
            }
            if (a[a_length - h_length + i] < b[a_length - h_length + i]) {
                return true;
            }
            if (a[a_length - h_length + i] > b[a_length - h_length + i]) {
                return false;
            }
        }

        return false;
    }
};

/**
 * Order node_p or node_cp by less_sexpr on sexpr.
 **/
struct less_node_by_sexpr {
    // Intentionally inline.
    // The use of two overloads allows for pass-by-reference saving on
    // a large number of shared_ptr increment and decrements.
    //! See less_sexpr::operator()().
    bool operator()(const node_p& a, const node_p& b) const
    {
        return less_sexpr()(a->to_s(), b->to_s());
    }

    //! See less_sexpr::operator()().
    bool operator()(const node_cp& a, const node_cp& b) const
    {
        return less_sexpr()(a->to_s(), b->to_s());
    }
};

} // Predicate
} // IronBee

#endif
