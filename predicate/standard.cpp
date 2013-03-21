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

#include "standard.hpp"

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

string False::name() const
{
    return "false";
}

Value False::calculate(Context)
{
    return Value();
}

string True::name() const
{
    return "true";
}

Value True::calculate(Context)
{
    static node_p s_true_literal;
    if (! s_true_literal) {
        s_true_literal = node_p(new String(""));
        s_true_literal->eval(Context());
    }

    return s_true_literal->value();
}

string Or::name() const
{
    return "or";
}

Value Or::calculate(Context context)
{
    if (children().size() < 2) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "or requires two or more arguments."
            )
        );
    }
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->eval(context)) {
            // We are true.
            return True().eval(context);
        }
    }
    return False().eval(context);
}

string And::name() const
{
    return "and";
}

Value And::calculate(Context context)
{
    if (children().size() < 2) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "or requires two or more arguments."
            )
        );
    }
    BOOST_FOREACH(const node_p& child, children()) {
        if (! child->eval(context)) {
            // We are false.
            return False().eval(context);
        }
    }
    return True().eval(context);
}

string Not::name() const
{
    return "not";
}

Value Not::calculate(Context context)
{
    if (children().size() != 1) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "not requires exactly one argument."
            )
        );
    }
    if (children().front()->eval(context)) {
        return False().eval(context);
    }
    else {
        return True().eval(context);
    }
}

void load(CallFactory& to)
{
    to
        .add<False>()
        .add<True>()
        .add<Or>()
        .add<And>()
        .add<Not>()
    ;
}

} // Standard
} // Predicate
} // IronBee
