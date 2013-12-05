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
 * @brief IronBee++ --- Server
 *
 * This file defines (Const)Server, a wrapper for ib_server_t.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__SERVER__
#define __IBPP__SERVER__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/regex.h>
#include <ironbee/server.h>

#include <boost/noncopyable.hpp>

#include <ostream>

namespace IronBee {

/**
 * Const Server; equivalent to a const pointer to ib_server_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Server for discussion of Server
 *
 * @sa Server
 * @sa ironbeepp
 * @sa ib_server_t
 * @nosubgrouping
 **/
class ConstServer :
    public CommonSemantics<ConstServer>
{
public:
    //! C Type.
    typedef const ib_server_t* ib_type;

    /**
     * Construct singular ConstServer.
     *
     * All behavior of a singular ConstServer is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstServer();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_server_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Server from ib_server_t.
    explicit
    ConstServer(ib_type ib_server);

    ///@}

    // XXX
    enum direction_e {
        REQUEST  = IB_SERVER_REQUEST,
        RESPONSE = IB_SERVER_RESPONSE
    };

    // XXX
    enum header_action_e {
        SET    = IB_HDR_SET,
        APPEND = IB_HDR_APPEND,
        MERGE  = IB_HDR_MERGE,
        ADD    = IB_HDR_ADD,
        UNSET  = IB_HDR_UNSET,
        EDIT   = IB_HDR_EDIT
    };

    //! IronBee version number server was compiled with.
    uint32_t version_number() const;
    //! IronBee ABI number server was compiled with.
    uint32_t abi_number() const;
    //! IronBee version string server was compiled with.
    const char* version() const;
    //! Name of file defining server.
    const char* filename() const;
    //! Name of server.
    const char* name() const;

private:
    ib_type m_ib;
};

/**
 * Server; equivalent to a pointer to ib_server_t.
 *
 * Server can be treated as ConstServer.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Server will eventually provide an interface for IronBee/Server
 * interactions.  At present, it is in flux.  These classes provides the
 * minimal functionality needed to get an IronBee server running (See
 * Engine::create()).
 *
 * @sa ConstServer
 * @sa ironbeepp
 * @sa ib_server_t
 * @sa Engine::create()
 * @nosubgrouping
 **/
class Server :
    public ConstServer
{
public:
    //! C Type.
    typedef ib_server_t* ib_type;

    /**
     * Remove the constness of a ConstServer.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] server ConstServer to remove const from.
     * @returns Server pointing to same underlying server as @a server.
     **/
    static Server remove_const(ConstServer server);

    /**
     * Construct singular Server.
     *
     * All behavior of a singular Server is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Server();

    /** @name Callbacks
     * Methods to set callbacks.
     *
     * These methods allocate data to handle the C to C++ mapping.  To free
     * this memory, call destroy_callbacks().
     **/
    ///@{

    /**
     * Destroy memory associated with callbacks.
     *
     * Call this to free up the memory allocated by the set callback methods
     * below.
     **/
    void destroy_callbacks() const;

    //! See @ref ib_server_error_fn_t
    typedef boost::function<
        void (Transaction, int)
    > error_callback_t;
    //! See @ref ib_server_error_hdr_fn_t
    typedef boost::function<
        void (Transaction, const char*, size_t, const char*, size_t)
    > error_header_callback_t;
    //! See @ref ib_server_error_data_fn_t
    typedef boost::function<
        void (Transaction, const char*, size_t)
    > error_data_callback_t;
    //! See @ref ib_server_header_fn_t
    typedef boost::function<
        void (
            Transaction,
            direction_e,
            header_action_e,
            const char*, size_t,
            const char*, size_t,
            ib_rx_t*
        )
    > header_callback_t;
    //! See @ref ib_server_close_fn_t
    typedef boost::function<
        void (Connection, Transaction)
    > close_callback_t;

#ifdef HAVE_FILTER_DATA_API
    //! See ib_server_filter_init_fn_t
    typedef boost::function<
        void (Transaction, direction_e)
    > filter_init_callback_t;
    //! See @ref ib_server_filter_data_fn_t
    typedef boost::function<
        void (Transaction, direction_e, const char*, size_t)
    > filter_data_callback_t;
#endif

    //! See error callback.
    void set_error_callback(error_callback_t callback) const;
    //! See error header callback.
    void set_error_header_callback(error_header_callback_t callback) const;
    //! See error data callback.
    void set_error_data_callback(error_data_callback_t callback) const;
    //! See header callback.
    void set_header_callback(header_callback_t callback) const;
    //! See close callback.
    void set_close_callback(close_callback_t callback) const;

#ifdef HAVE_FILTER_DATA_API
    //! See filter init callback.
    void set_filter_init_callback(filter_init_callback_t callback) const;
    //! See filter data callback.
    void set_filter_data_callback(filter_data_callback_t callback) const;
#endif

    ///@}

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_server_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Server from ib_server_t.
    explicit
    Server(ib_type ib_server);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output operator for Server.
 *
 * Output IronBee::Server[@e value] where @e value is name().
 *
 * @param[in] o Ostream to output to.
 * @param[in] server Server to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstServer& server);

/**
 * Server Value; equivalent to ib_server_t.
 *
 * Note that Server is equivalent to an @c ib_server_t*.
 *
 * This class is intended to be use to allocate a server value.  It can be
 * used to allocate a value on the stack, via new, via a static variable,
 * etc.  The get() method can be used to fetch a Server or ConstServer for
 * actual use.  The underlying value (an ib_server_t) will persist as long
 * as this instance persists.
 **/
class ServerValue :
   boost::noncopyable
{
public:
    /**
     * Initialize a Server with the current IronBee version and ABI.
     *
     * @param[in] filename Should be __FILE__.
     * @param[in] name     Name of server.
     **/
    ServerValue(
        const char* filename,
        const char* name
    );

    //! Fetch Server.
    Server get();

    //! Fetch ConstServer
    ConstServer get() const;

private:
    ib_server_t m_value;
};

} // IronBee

#endif
