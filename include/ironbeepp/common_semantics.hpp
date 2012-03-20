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
 * @brief IronBee++ &mdash; ByteString
 *
 * This file defines the CommonSemantics template that provides common
 * pointer-like semantics for most IronBee++ types.
 *
 * @sa CommonSemantics
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__COMMON_SEMANTICS__
#define __IBPP__COMMON_SEMANTICS__

#include <boost/operators.hpp>

namespace IronBee {

/**
 * CRTP template to provide most comparison operators and singularity testing.
 *
 * This base class should be used with the Curiously Recurring Template
 * Pattern, e.g.,
 *
 * @code
 * class MyClass : public CommonSemantics<MyClass>
 * @endcode
 *
 * Furthermore, @a SubclassType must provide a public ib() method that ==, <,
 * and boolean evaluation.  For IronBee++ classes, this will be a pointer.
 *
 * This template then provides the following operators: <, >, ==, !=, <=, >=.
 * It also provides boolean evaluation for singularity (i.e., true iff
 * ib() != NULL).  It does so in way that does not allow implicit conversion
 * to an integer type, e.g.,
 * @code
 * if (foo) {...}
 * @endcode
 * is legal, but
 * @code
 * int x = foo;
 * @endcode
 * is not.
 *
 * @remark This template uses a number of C++ techniques including
 * CRTP, friendly injection, and boolean-like types.
 *
 * @tparam SubclassType Type of subclass.
 **/
template <typename SubclassType>
class CommonSemantics :
    boost::less_than_comparable<SubclassType>,
    boost::equality_comparable<SubclassType>
{
public:
    // Friendly injection trick.
    /**
     * Equality operator for @a SubclassType.
     *
     * Also provides operator!= via boost operators.
     *
     * @param[in] a First object.
     * @param[in] b Second object.
     * @returns true iff a.ib() == b.ib()
     **/
    friend bool operator==(const SubclassType& a,const SubclassType& b)
    {
        return a.ib() == b.ib();
    }

    /**
     * Less than operator for @a SubclassType.
     *
     * Also provides operator>, operator<=, operator>= via boost operators.
     *
     * @param[in] a First object.
     * @param[in] b Second object.
     * @returns true iff a.ib() < b.ib()
     **/
    friend bool operator<(const SubclassType& a,const SubclassType& b)
    {
        return a.ib() < b.ib();
    }

    ///@cond Internal
    typedef void (*unspecified_bool_type)(SubclassType***);
    ///@endcond

    /**
     * Test singularity.
     *
     * Evaluates as truthy if this->ib() != NULL.  Does so in a way that
     * prevents implicit conversion to integral types.
     *
     * @returns truthy iff this->ib() != NULL.
     **/
    operator unspecified_bool_type() const
    {
        if (static_cast<const SubclassType&>(*this).ib()) {
            return unspecified_bool;
        }
        else {
            return NULL;
        }
    }

private:
    // Used for unspecified_bool_type.
    static void unspecified_bool(SubclassType***) {};
};

} // IronBee

#endif
