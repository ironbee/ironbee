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
 * @brief IronBee++ --- Server Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/server.hpp>

#include <ironbeepp/catch.hpp>

namespace IronBee {

// ConstServer

ConstServer::ConstServer() :
    m_ib(NULL)
{
    // nop
}

ConstServer::ConstServer(ib_type ib_server) :
    m_ib(ib_server)
{
    // nop
}


uint32_t ConstServer::version_number() const
{
    return ib()->vernum;
}

uint32_t ConstServer::abi_number() const
{
    return ib()->abinum;
}

const char* ConstServer::version() const
{
    return ib()->version;
}

const char* ConstServer::filename() const
{
    return ib()->filename;
}

const char* ConstServer::name() const
{
    return ib()->name;
}

// Server

Server Server::remove_const(ConstServer server)
{
    return Server(const_cast<ib_type>(server.ib()));
}

Server::Server() :
    m_ib(NULL)
{
    // nop
}

Server::Server(ib_type ib_server) :
    ConstServer(ib_server),
    m_ib(ib_server)
{
    // nop
}

namespace {

template <typename T>
void destroy_callback(void* data)
{
    if (data) {
        T functional;
        try {
            functional = data_to_value<T>(data);
        }
        catch (...) {
            // Not our data, do nothing.
            return;
        }

        boost::any* data_any = reinterpret_cast<boost::any*>(data);
        delete data_any;
    }
}

}

extern "C" {

static ib_status_t server_error_translator(
    ib_tx_t* tx,
    int status,
    void* cbdata
)
{
    try {
        data_to_value<Server::error_callback_t>(cbdata)(
            Transaction(tx),
            status
        );
    }
    catch (...) {
        return convert_exception(tx->ib);
    }
    return IB_OK;
}

static ib_status_t server_error_header_translator(
    ib_tx_t* tx,
    const char* name, size_t name_length,
    const char* value, size_t value_length,
    void* cbdata
)
{
    try {
        data_to_value<Server::error_header_callback_t>(cbdata)(
            Transaction(tx),
            name, name_length,
            value, value_length
        );
    }
    catch (...) {
        return convert_exception(tx->ib);
    }
    return IB_OK;
}

static ib_status_t server_error_data_translator(
    ib_tx_t* tx,
    const char* data, size_t data_length,
    void* cbdata
)
{
    try {
        data_to_value<Server::error_data_callback_t>(cbdata)(
            Transaction(tx),
            data, data_length
        );
    }
    catch (...) {
        return convert_exception(tx->ib);
    }
    return IB_OK;
}

static ib_status_t server_header_translator(
    ib_tx_t* tx,
    ib_server_direction_t dir,
    ib_server_header_action_t action,
    const char* name, size_t name_length,
    const char* value, size_t value_length,
    void* cbdata
)
{
    try {
        data_to_value<Server::header_callback_t>(cbdata)(
            Transaction(tx),
            Server::direction_e(dir),
            Server::header_action_e(action),
            name, name_length,
            value, value_length
        );
    }
    catch (...) {
        return convert_exception(tx->ib);
    }
    return IB_OK;
}

static ib_status_t server_close_translator(
    ib_conn_t* conn,
    ib_tx_t* tx,
    void* cbdata
)
{
    try {
        data_to_value<Server::close_callback_t>(cbdata)(
            Connection(conn),
            Transaction(tx)
        );
    }
    catch (...) {
        return convert_exception(tx->ib);
    }
    return IB_OK;
}

#ifdef HAVE_FILTER_DATA_API

static ib_status_t server_filter_init_translator(
    ib_tx_t* tx,
    ib_server_direction_t dir,
    void* cbdata
)
{
    try {
        data_to_value<Server:filter_init_callback_t>(cbdata)(
            Transaction(tx),
            Server::direction_e(dir)
        );
    }
    catch (...) {
        return convert_exception(tx->ib);
    }
    return IB_OK;
}

static ib_status_t server_filter_data_translator(
    ib_tx_t* tx,
    ib_server_direction_t dir,
    const char* block, size_t block_length,
    void* cbdata
)
{
    try {
        data_to_value<Server::filter_data_callback_t>(cbdata)(
            Transaction(tx),
            Server::direction_e(dir),
            block, block_length
        );
    }
    catch (...) {
        return convert_exception(tx->ib);
    }
    return IB_OK;
}

#endif

}

void Server::destroy_callbacks() const
{
    destroy_callback<error_callback_t>(ib()->err_data);
    destroy_callback<error_header_callback_t>(ib()->err_hdr_data);
    destroy_callback<error_data_callback_t>(ib()->err_body_data);
    destroy_callback<header_callback_t>(ib()->hdr_data);
    destroy_callback<close_callback_t>(ib()->close_data);
#ifdef HAVE_FILTER_DATA_API
    destroy_callback<filter_init_callback_t>(ib()->init_data);
    destroy_callback<filter_data_callback_t>(ib()->data_data);
#endif
}

void Server::set_error_callback(error_callback_t callback) const
{
    ib()->err_fn = server_error_translator;
    ib()->err_data = value_to_data(callback, NULL);
}

void Server::set_error_header_callback(error_header_callback_t callback) const
{
    ib()->err_hdr_fn = server_error_header_translator;
    ib()->err_hdr_data = value_to_data(callback, NULL);
}

void Server::set_error_data_callback(error_data_callback_t callback) const
{
    ib()->err_body_fn = server_error_data_translator;
    ib()->err_body_data = value_to_data(callback, NULL);
}

void Server::set_header_callback(header_callback_t callback) const
{
    ib()->hdr_fn = server_header_translator;
    ib()->hdr_data = value_to_data(callback, NULL);
}

void Server::set_close_callback(close_callback_t callback) const
{
    ib()->close_fn = server_close_translator;
    ib()->close_data = value_to_data(callback, NULL);
}

#ifdef HAVE_FILTER_DATA_API

void Server::set_filter_init_callback(filter_init_callback_t callback) const
{
    ib()->init_fn = server_filter_init_translator;
    ib()->init_data = value_to_data(callback, NULL);
}

void Server::set_filter_data_callback(filter_data_callback_t callback) const
{
    ib()->data_fn = server_filter_data_translator;
    ib()->data_data = value_to_data(callback, NULL);
}

#endif

std::ostream& operator<<(std::ostream& o, const ConstServer& server)
{
    if (! server) {
        o << "IronBee::Server[!singular!]";
    } else {
        o << "IronBee::Server[" << server.name() << "]";
    }
    return o;
}

//! ServerValue

ServerValue::ServerValue(
    const char* filename,
    const char* name
)
{
    memset(&m_value, 0, sizeof(m_value));
    m_value.vernum = IB_VERNUM;
    m_value.abinum = IB_ABINUM;
    m_value.version = IB_VERSION;
    m_value.filename = filename;
    m_value.name = name;
}

Server ServerValue::get()
{
    return Server(&m_value);
}

ConstServer ServerValue::get() const
{
    return ConstServer(&m_value);
}

} // IronBee
