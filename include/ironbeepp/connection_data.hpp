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
 * @brief IronBee++ &mdash; ConnectionData
 *
 * This file defines (Const)ConnectionData, a wrapper for ib_conndata_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONNECTION_DATA__
#define __IBPP__CONNECTION_DATA__

#include <ironbeepp/common_semantics.hpp>

#include <ostream>

// IronBee C Type
typedef struct ib_conndata_t ib_conndata_t;

namespace IronBee {

class Engine;
class MemoryPool;
class Connection;

/**
 * Const ConnectionData; equivalent to a const pointer to ib_conndata_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See ConnectionData for discussion of Connection Data
 *
 * @sa ConnectionData
 * @sa ironbeepp
 * @sa ib_conndata_t
 * @nosubgrouping
 **/
class ConstConnectionData :
    public CommonSemantics<ConstConnectionData>
{
public:
    //! C Type.
    typedef const ib_conndata_t* ib_type;

    /**
     * Construct singular ConstConnectionData.
     *
     * All behavior of a singular ConstConnectionData is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstConnectionData();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_conndata_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ConnectionData from ib_conndata_t.
    explicit
    ConstConnectionData(ib_type ib_connection_data);

    ///@}

    //! Associated Engine.
    Engine engine() const;

    //! Associated MemoryPool.
    MemoryPool memory_pool() const;

    //! Associated Connection.
    Connection connection() const;

    //! Amount of memory allocated for data.
    size_t allocated() const;

    //! Length of data.
    size_t length() const;

    //! Pointer to data.
    char* data() const;

private:
    ib_type m_ib;
};

/**
 * ConnectionData; equivalent to a pointer to ib_conndata_t.
 *
 * ConnectionData can be treated as ConstConnectionData.  See @ref ironbeepp
 * for details on IronBee++ object semantics.
 *
 * This class provides no functionality over ConstConnectionData except for
 * providing a non-const @c ib_conndata_t* via ib().
 *
 * @sa ConstConnectionData
 * @sa ironbeepp
 * @sa ib_conndata_t
 * @nosubgrouping
 **/
class ConnectionData :
    public ConstConnectionData
{
public:
    //! C Type.
    typedef ib_conndata_t* ib_type;

    /**
     * Remove the constness of a ConstConnectionData.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] connection_data ConstConnectionData to remove const from.
     * @returns ConnectionData pointing to same underlying connection data as
     *          @a connection_data.
     **/
    static ConnectionData remove_const(ConstConnectionData connection_data);

    /**
     * Construct singular ConnectionData.
     *
     * All behavior of a singular ConnectionData is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConnectionData();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_conndata_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct ConnectionData from ib_conndata_t.
    explicit
    ConnectionData(ib_type ib_connection_data);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output operator for ConnectionData.
 *
 * Outputs IronBee::ConnectionData[@e value] where @e value is the data.
 *
 * @param[in] o Ostream to output to.
 * @param[in] connection_data ConnectionData to output.
 * @return @a o
 **/
std::ostream& operator<<(
    std::ostream& o,
    const ConstConnectionData& connection_data
);

} // IronBee

#endif
