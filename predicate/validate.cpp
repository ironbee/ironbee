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

#include <predicate/validate.hpp>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Validate {

namespace  {

node_cp nth_child(
    NodeReporter  reporter,
    size_t         n
)
{
    size_t num_children = reporter.node()->children().size();

    if (reporter.node()->children().size() <= n) {
        reporter.error(
            "Wanted " + boost::lexical_cast<string>(n+1) +
            "th child.  But there are only " +
            boost::lexical_cast<string>(num_children) + " children."
        );
        return node_cp();
    }

    return *boost::next(reporter.node()->children().begin(), n);
}

template <typename BaseClass>
bool is_a(const node_cp& node)
{
    return boost::dynamic_pointer_cast<const BaseClass>(node);
}

}

void n_children(NodeReporter reporter, size_t n)
{
    size_t actual_children = reporter.node()->children().size();
    if (actual_children != n) {
        reporter.error(
            "Expected " + boost::lexical_cast<string>(n) + " children " +
            "but have " + boost::lexical_cast<string>(actual_children) +
            "."
        );
    }
}

void n_or_more_children(NodeReporter reporter, size_t n)
{
    size_t actual_children = reporter.node()->children().size();
    if (actual_children < n) {
        reporter.error(
            "Expected at least " + boost::lexical_cast<string>(n) +
            " children  but have " +
            boost::lexical_cast<string>(actual_children) + "."
        );
    }
}

void n_or_fewer_children(NodeReporter reporter, size_t n)
{
    size_t actual_children = reporter.node()->children().size();
    if (actual_children > n) {
        reporter.error(
            "Expected at most " + boost::lexical_cast<string>(n) +
            " children but have " +
            boost::lexical_cast<string>(actual_children) + "."
        );
    }
}

void nth_child_is_string(NodeReporter reporter, size_t n)
{
    node_cp child = nth_child(reporter, n);
    if (child && ! is_a<String>(child)) {
        reporter.error(
            "Child " + boost::lexical_cast<string>(n+1) + " must be a "
            "string literal."
        );
    }
}

void nth_child_is_null(NodeReporter reporter, size_t n)
{
    node_cp child = nth_child(reporter, n);
    if (child && ! is_a<Null>(child)) {
        reporter.error(
            "Child " + boost::lexical_cast<string>(n+1) + " must be a null."
        );
    }
}

void nth_child_is_not_null(NodeReporter reporter, size_t n)
{
    node_cp child = nth_child(reporter, n);
    if (! child || is_a<Null>(child)) {
        reporter.error(
            "Child " + boost::lexical_cast<string>(n+1) + " must not be a null."
        );
    }
}

void no_child_is_literal(NodeReporter reporter)
{
    size_t i = 0;
    BOOST_FOREACH(const node_cp& child, reporter.node()->children()) {
        if (is_a<Literal>(child)) {
            reporter.error(
                "Child " + boost::lexical_cast<string>(i+1) + " must  not be"
                "literal."
            );
        }
        ++i;
    }
}

void no_child_is_null(NodeReporter reporter)
{
    size_t i = 0;
    BOOST_FOREACH(const node_cp& child, reporter.node()->children()) {
        if (is_a<Null>(child)) {
            reporter.error(
                "Child " + boost::lexical_cast<string>(i+1) + " must  not be"
                "null."
            );
        }
        ++i;
    }
}

} // Validate
} // Predicate
} // IronBee
