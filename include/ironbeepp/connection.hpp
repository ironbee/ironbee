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
 * @brief IronBee++ --- Connection
 *
 * This file defines (Const)Connection, a wrapper for ib_conn_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONNECTION__
#define __IBPP__CONNECTION__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>

#include <ironbee/engine.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/date_time/posix_time/ptime.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ostream>

// IronBee C Type
typedef struct ib_conn_t ib_conn_t;

namespace IronBee {

class Engine;
class MemoryPool;
class Context;
class Transaction;

/**
 * Const Connection; equivalent to a const pointer to ib_conn_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Connection for discussion of Connection
 *
 * @sa Connection
 * @sa ironbeepp
 * @sa ib_conn_t
 * @nosubgrouping
 **/
class ConstConnection :
    public CommonSemantics<ConstConnection>
{
public:
    //! C Type.
    typedef const ib_conn_t* ib_type;

    /**
     * Construct singular ConstConnection.
     *
     * All behavior of a singular ConstConnection is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstConnection();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_conn_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Connection from ib_conn_t.
    explicit
    ConstConnection(ib_type ib_connection);

    ///@}

    //! Associated Engine.
    Engine engine() const;

    //! Associated MemoryPool.
    MemoryPool memory_pool() const;

    //! Identifier.
    const char* id() const;

    //! Associated Context.
    Context context() const;

    //! When the connection started.
    boost::posix_time::ptime started_time() const;

    //! When the connection finished.
    boost::posix_time::ptime finished_time() const;

    //! Remote IP address as a dotted quad string.
    const char* remote_ip_string() const;

    //! Remote port.
    uint16_t remote_port() const;

    //! Local IP address as a dotted quad string.
    const char* local_ip_string() const;

    //! Local port.
    uint16_t local_port() const;

    //! Number of transactions.
    size_t transaction_count() const;

    /**
     * First transaction / beginning of transaction list.
     *
     * Later transaction can be accessed via Transaction::next().
     **/
    Transaction first_transaction() const;

    //! Last transaction / end of transaction list.
    Transaction last_transaction() const;

    //! Transaction most recently created/destroyed/modified.
    Transaction transaction() const;

   /**
     * @name Flags
     * Transaction Flags
     *
     * The masks for the flags are defined by the flags_e enum.  All flags
     * as a set of bits can be accessed via flags().  Individual flags can be
     * checked either via flags() @c & @c flag_X or via @c is_X().
     **/
    ///@{

    //! Possible flags.  Treat as bit masks.
    enum flags_e {
        flag_none             = IB_CONN_FNONE,
        flag_error            = IB_CONN_FERROR,
        flag_transaction      = IB_CONN_FTX,
        flag_data_in          = IB_CONN_FDATAIN,
        flag_data_out         = IB_CONN_FDATAOUT,
        flag_opened           = IB_CONN_FOPENED,
        flag_closed           = IB_CONN_FCLOSED
    };

    //! All flags.
    ib_flags_t flags() const;

    //! flags() & flag_none
    bool is_none() const
    {
        return flags() & flag_none;
    }

    //! flags() & flag_error
    bool is_error() const
    {
        return flags() & flag_error;
    }

    //! flags() & flag_transaction
    bool is_transaction() const
    {
        return flags() & flag_transaction;
    }

    //! flags() & flag_data_in
    bool is_data_in() const
    {
        return flags() & flag_data_in;
    }

    //! flags() & flag_data_out
    bool is_data_out() const
    {
        return flags() & flag_data_out;
    }

    //! flags() & flag_opened
    bool is_opened() const
    {
        return flags() & flag_opened;
    }

    //! flags() & flag_closed
    bool is_closed() const
    {
        return flags() & flag_closed;
    }

private:
    ib_type m_ib;
};

/**
 * Connection; equivalent to a pointer to ib_conn_t.
 *
 * Connection can be treated as ConstConnection.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * A connection is a sequence of transactions over a single stream between
 * a remote and a local entity.
 *
 * This class adds no functionality to ConstConnection beyond providing a
 * non-const @c ib_conn_t* via ib().
 *
 * @sa ConstConnection
 * @sa ironbeepp
 * @sa ib_conn_t
 * @nosubgrouping
 **/
class Connection :
    public ConstConnection
{
public:
    //! C Type.
    typedef ib_conn_t* ib_type;

    /**
     * Remove the constness of a ConstConnection.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] connection ConstConnection to remove const from.
     * @returns Connection pointing to same underlying connection as
     *          @a connection.
     **/
    static Connection remove_const(ConstConnection connection);

    /**
     * Construct singular Connection.
     *
     * All behavior of a singular Connection is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Connection();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_conn_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Connection from ib_conn_t.
    explicit
    Connection(ib_type ib_connection);

    ///@}

    /**
     * Create a new connection.
     *
     * The C API provides a plugin context @c void* parameter for connection
     * creation.  This is currently unsupported.  It is intended that C++
     * server plugins not need that context.
     *
     * @param[in] engine Engine to associate connection with.
     * @returns Connection
     **/
    static
    Connection create(Engine engine);

    /**
     * Set remote IP string.
     *
     * The memory pointed to by @a ip must have a lifetime that exceeds the
     * connection.
     *
     * @param[in] ip New remote IP string.
     **/
    void set_remote_ip_string(const char* ip) const;

    /**
     * Set remote port number.
     *
     * @param[in] port New port number.
     **/
    void set_remote_port(uint16_t port) const;

    /**
     * Set local IP string.
     *
     * The memory pointed to by @a ip must have a lifetime that exceeds the
     * connection.
     *
     * @param[in] ip New local IP string.
     **/
    void set_local_ip_string(const char* ip) const;

    /**
     * Set local port number.
     *
     * @param[in] port New port number.
     **/
    void set_local_port(uint16_t port) const;

    /**
     * Destroy connection.
     **/
    void destroy() const;

private:
    ib_type m_ib;
};

/**
 * Output operator for Connection.
 *
 * Output IronBee::Connection[@e value] where @e value is remote ip and port
 * -> local ip ad port.
 *
 * @param[in] o Ostream to output to.
 * @param[in] connection Connection to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstConnection& connection);

} // IronBee

#endif
