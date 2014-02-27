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
 * @brief IronBee++ -- Data
 *
 * This file provides data_to_value() and value_to_data(), functions that
 * enable the storage of C++ data in void*'s with appropriate destructors
 * and runtime type checking.
 *
 * The values are both copied in and copied out.  As such, they should be
 * rapid-copying types, e.g., boost::shared_ptr.
 *
 * These functions are implemented by storing the values in boost::any's and
 * only casting between @c void* and @c boost::any*.  The boost::any semantics
 * ensure invalid casts are caught (at runtime) and destructors are properly
 * called.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__DATA__
#define __IBPP__DATA__

#include <ironbeepp/exception.hpp>

#include <ironbee/mm.h>

#include <boost/any.hpp>

namespace IronBee {

namespace Internal {

extern "C" {

/**
 * C function used by value_to_data().
 *
 * You should never need to use this directly.
 * @param[in] data Data to clean up (delete).
 **/
void ibpp_data_cleanup(void* data);

} // extern "C"

} // Internal

/**
 * Convert a @c void* generated with value_to_data() to a @a ValueType.
 *
 * This attempts to extract the value in @a data, assuming it is of type
 * @a ValueType.  If @a ValueType does not match the original type stored in
 * data, a einval exception will be thrown.
 *
 * @tparam ValueType Type to cast @a data to.
 * @param[in] data Data to turn into a value.
 * @return Copy of value stored in data.
 * @throw einval if @a ValueType is incorrect.
 **/
template <typename ValueType>
ValueType data_to_value(void* data)
{
    boost::any* data_any = reinterpret_cast<boost::any*>(data);

    try {
        return boost::any_cast<ValueType>(*data_any);
    }
    catch (boost::bad_any_cast) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Stored type mismatch."
            )
        );
    }
}

/**
 * Store a copy of @a value and provide a @c void* for data_to_value().
 *
 * This _copies_ @a value and returns a @c void* containing information to
 * recover the value.  It also registers a clean up function with @a mm so
 * that the copy is destroyed (and its destructor properly called) when
 * @a mm is destroyed.
 *
 * @tparam ValueType Type of @a value.
 * @param[in] value Value to store copy of.
 * @param[in] mm Memory manager to register clean up function with.
 * @returns Generic pointer suitable for use with data_to_value().
 **/
template <typename ValueType>
void* value_to_data(
    const ValueType& value,
    ib_mm_t          mm
)
{
    boost::any* value_any = new boost::any(value);

    if (! ib_mm_is_null(mm)) {
        ib_mm_register_cleanup(
            mm,
            Internal::ibpp_data_cleanup,
            reinterpret_cast<void*>(value_any)
        );
    }

    return reinterpret_cast<void*>(value_any);
}

/**
 * Store a copy of @a value and provide a @c void* for data_to_value().
 *
 * This _copies_ @a value and returns a @c void* containing information to
 * recover the value.  The user is responsible for casting return to @c any*
 * and deleting when appropriate.
 *
 * @tparam ValueType Type of @a value.
 * @param[in] value Value to store copy of.
 * @returns Generic pointer suitable for use with data_to_value().
 **/
template <typename ValueType>
void* value_to_data(
    const ValueType& value
)
{
    boost::any* value_any = new boost::any(value);

    return reinterpret_cast<void*>(value_any);
}

} // IronBee

#endif
