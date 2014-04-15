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
 * @brief IronBee --- CLIPP Input
 *
 * Defines Input, the fundamental unit of input that CLIPP will give to
 * IronBee and related structures.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__INPUT__
#define __IRONBEE__CLIPP__INPUT__

#include <boost/any.hpp>
#include <boost/operators.hpp>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/shared_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <iostream>
#include <list>
#include <stdexcept>
#include <string>

#include <stdint.h>

namespace IronBee {
namespace CLIPP {
namespace Input {

struct ConnectionEvent;
struct NullEvent;
struct DataEvent;
struct RequestEvent;
struct ResponseEvent;
struct HeaderEvent;

/**
 * This is the parent class of all read-only delegates for dispatch().
 *
 * To use, subclass, override the methods for events you wish to handle, and
 * pass to a dispatch() call (e.g., Event::dispatch) or (Input::dispatch).
 *
 * Note that default behavior is to do nothing.
 *
 * @sa ModifierDelegate
 **/
class Delegate
{
public:
    //! CONNECTION_OPENED
    virtual
    void connection_opened(const ConnectionEvent& event) {}

    //! CONNECTION_CLOSED
    virtual
    void connection_closed(const NullEvent& event) {}

    /* IronBee no longer supports a connection data interface.  CLIPP retains
     * it, however, to allow easy generator writing.  Any use of it should
     * pass through the @parse modifier to convert to an IronBee compatible
     * interface.
     */

    //! CONNECTION_DATA_IN
    virtual
    void connection_data_in(const DataEvent& event) {}

    //! CONNECTION_DATA_OUT
    virtual
    void connection_data_out(const DataEvent& event) {}

    //! REQUEST_STARTED
    virtual
    void request_started(const RequestEvent& event) {}

    //! REQUEST_HEADER
    virtual
    void request_header(const HeaderEvent& event) {}

    //! REQUEST_HEADER_FINISHED
    virtual
    void request_header_finished(const NullEvent& event) {}

    //! REQUEST_BODY
    virtual
    void request_body(const DataEvent& event) {}

    //! REQUEST_FINISHED
    virtual
    void request_finished(const NullEvent& event) {}

    //! RESPONSE_STARTED
    virtual
    void response_started(const ResponseEvent& event) {}

    //! RESPONSE_HEADER
    virtual
    void response_header(const HeaderEvent& event) {}

    //! RESPONSE_HEADER_FINISHED
    virtual
    void response_header_finished(const NullEvent& event) {}

    //! RESPONSE_BODY
    virtual
    void response_body(const DataEvent& event) {}

    //! RESPONSE_FINISHED
    virtual
    void response_finished(const NullEvent& event) {}
};

/**
 * This is the parent class of all mutating delegates for dispatch().
 *
 * Unlike Delegate, this allows mutation of the events.
 *
 * To use, subclass, override the methods for events you wish to handle, and
 * pass to a dispatch() call (e.g., Event::dispatch) or (Input::dispatch).
 *
 * Note that default behavior is to do nothing.
 *
 * @sa Delegate.
 **/
class ModifierDelegate
{
public:
    //! CONNECTION_OPENED
    virtual
    void connection_opened(ConnectionEvent& event) {}

    //! CONNECTION_CLOSED
    virtual
    void connection_closed(NullEvent& event) {}

    //! CONNECTION_DATA_IN
    virtual
    void connection_data_in(DataEvent& event) {}

    //! CONNECTION_DATA_OUT
    virtual
    void connection_data_out(DataEvent& event) {}

    //! REQUEST_STARTED
    virtual
    void request_started(RequestEvent& event) {}

    //! REQUEST_HEADER
    virtual
    void request_header(HeaderEvent& event) {}

    //! REQUEST_HEADER_FINISHED
    virtual
    void request_header_finished(NullEvent& event) {}

    //! REQUEST_BODY
    virtual
    void request_body(DataEvent& event) {}

    //! REQUEST_FINISHED
    virtual
    void request_finished(NullEvent& event) {}

    //! RESPONSE_STARTED
    virtual
    void response_started(ResponseEvent& event) {}

    //! RESPONSE_HEADER
    virtual
    void response_header(HeaderEvent& event) {}

    //! RESPONSE_HEADER_FINISHED
    virtual
    void response_header_finished(NullEvent& event) {}

    //! RESPONSE_BODY
    virtual
    void response_body(DataEvent& event) {}

    //! RESPONSE_FINISHED
    virtual
    void response_finished(NullEvent& event) {}
};

/**
 * Simple representation of memory buffer.
 *
 * This structure is a data pointer and length.  It's primary use is to refer
 * to substrings without copying.
 **/
struct Buffer :
    boost::equality_comparable<Buffer, std::string>,
    boost::equality_comparable<Buffer>
{
    //! Default constructor.  Buffer is empty string.
    Buffer();

    //! Constructor.
    /**
     * @param[in] data   Pointer to buffer.  Not necessarily null terminated.
     * @param[in] length Length of buffer.
     **/
    Buffer(const char* data_, size_t length_);

    //! Construct from string.
    /**
     * The parameter @a s will need to outlive this Buffer.
     *
     * @param[in] s String to initialize buffer from.
     **/
    explicit
    Buffer(const std::string& s);

    //! Convert to string.  Makes a copy.
    std::string to_s() const;

    //! Compare buffer and buffer.
    bool operator==(const Input::Buffer& other) const;

    //! Compare buffer and string.
    bool operator==(const std::string& s) const;

    //! Pointer to buffer.  Not necessarily null terminated.
    const char* data;
    //! Length of buffer.
    size_t      length;
};

//! Output operator for buffers.
std::ostream& operator<<(std::ostream& out, const Buffer& buffer);

//! Event identifier.
enum event_e {
    UNKNOWN,
    CONNECTION_OPENED,
    CONNECTION_DATA_IN,
    CONNECTION_DATA_OUT,
    CONNECTION_CLOSED,
    REQUEST_STARTED,
    REQUEST_HEADER,
    REQUEST_HEADER_FINISHED,
    REQUEST_BODY,
    REQUEST_FINISHED,
    RESPONSE_STARTED,
    RESPONSE_HEADER,
    RESPONSE_HEADER_FINISHED,
    RESPONSE_BODY,
    RESPONSE_FINISHED
};

/**
 * Base class of all events.
 *
 * This class can be used for dispatching, identication of which event it
 * represents, and delays.  All event specific information requires it to be
 * cast to the appropriate subclass, preferably via dispatch().
 **/
struct Event
{
    //! Construct Event.
    explicit
    Event(event_e which_);

    //! Dispatch event without delay.  Must be overriden by subclass.
    virtual void dispatch(Delegate& to) const = 0;

    //! Dispatch event with optional delay.
    void dispatch(Delegate& to, bool with_delay) const;

    //! Dispatch event for modification.
    virtual void dispatch(ModifierDelegate& to) = 0;

    //! Which event we are.
    event_e which;

    //! Seconds to delay before firing event.
    double pre_delay;

    //! Seconds to delay after firing event.
    double post_delay;
};

/**
 * No associated data:
 *   REQUEST_HEADER_FINISHED, REQUEST_FINISHED,
 *   RESPONSE_HEADER_FINISHED, RESPONSE_FINISHED,
 *   CONNECTION_CLOSED.
 *
 * @sa Event
 **/
struct NullEvent :
    public Event
{
    //! Constructor.
    explicit
    NullEvent(event_e which_);

    //! Dispatch.
    inline
    void dispatch(Delegate& to) const
    {
        switch (which) {
            case REQUEST_HEADER_FINISHED:  to.request_header_finished(*this); break;
            case REQUEST_FINISHED:         to.request_finished(*this); break;
            case RESPONSE_HEADER_FINISHED: to.response_header_finished(*this); break;
            case RESPONSE_FINISHED:        to.response_finished(*this); break;
            case CONNECTION_CLOSED:        to.connection_closed(*this); break;
            default:
                throw std::logic_error("Invalid NullEvent.");
        }
    }

    //! Modifier Dispatch.
    inline
    void dispatch(ModifierDelegate& to)
    {
        switch (which) {
            case REQUEST_HEADER_FINISHED:  to.request_header_finished(*this); break;
            case REQUEST_FINISHED:         to.request_finished(*this); break;
            case RESPONSE_HEADER_FINISHED: to.request_header_finished(*this); break;
            case RESPONSE_FINISHED:        to.response_finished(*this); break;
            case CONNECTION_CLOSED:        to.connection_closed(*this); break;
            default:
                throw std::logic_error("Invalid NullEvent.");
        }
    }
};

/**
 * Connection data: CONNECTION_OPENED.
 *
 * @sa Event
 **/
struct ConnectionEvent :
    public Event
{
    //! Constructor.
    explicit
    ConnectionEvent(event_e which_);

    ConnectionEvent(
        event_e       which_,
        const Buffer& local_ip_,
        uint32_t      local_port_,
        const Buffer& remote_ip_,
        uint32_t      remote_port_
    );

    //! Dispatch.
    inline
    void dispatch(Delegate& to) const
    {
        switch (which) {
            case CONNECTION_OPENED: to.connection_opened(*this); break;
            default:
                throw std::logic_error("Invalid ConnectionEvent.");
        }
    }

    //! ModifierDispatch.
    inline
    void dispatch(ModifierDelegate& to)
    {
        switch (which) {
            case CONNECTION_OPENED: to.connection_opened(*this); break;
            default:
                throw std::logic_error("Invalid ConnectionEvent.");
        }
    }

    //! Local IP address.
    Buffer local_ip;
    //! Local port.
    uint32_t local_port;
    //! Remote IP address.
    Buffer remote_ip;
    //! Remote port.
    uint32_t remote_port;
};

/**
 * ConnectionData data: CONNECTION_DATA_IN, CONNECTION_DATA_OUT,
 *                      REQUEST_BODY, RESPONSE_BODY.
 *
 * @sa Event
 **/
struct DataEvent :
    public Event
{
    //! Constructor.
    explicit
    DataEvent(event_e which_);

    //! Constructor.
    DataEvent(event_e which_, const Buffer& data_);

    //! Dispatch.
    inline
    void dispatch(Delegate& to) const
    {
        switch (which) {
            case CONNECTION_DATA_IN:  to.connection_data_in(*this); break;
            case CONNECTION_DATA_OUT: to.connection_data_out(*this); break;
            case REQUEST_BODY:        to.request_body(*this); break;
            case RESPONSE_BODY:       to.response_body(*this); break;
            default:
                throw std::logic_error("Invalid DataEvent.");
        }
    }

    //! Dispatch.
    inline
    void dispatch(ModifierDelegate& to)
    {
        switch (which) {
            case CONNECTION_DATA_IN:  to.connection_data_in(*this); break;
            case CONNECTION_DATA_OUT: to.connection_data_out(*this); break;
            case REQUEST_BODY:        to.request_body(*this); break;
            case RESPONSE_BODY:       to.response_body(*this); break;
            default:
                throw std::logic_error("Invalid DataEvent.");
        }
    }

    Buffer data;
};

/**
 * Request line data: REQUEST_STARTED.
 *
 * @sa Event
 **/
struct RequestEvent :
    public Event
{
    //! Constructor.
    explicit
    RequestEvent(event_e which_);
    //! Constructor.
    RequestEvent(
        event_e       which_,
        const Buffer& raw_,
        const Buffer& method_,
        const Buffer& uri_,
        const Buffer& protocol_
    );

    //! Dispatch.
    inline
    void dispatch(Delegate& to) const
    {
        switch (which) {
            case REQUEST_STARTED: to.request_started(*this); break;
            default:
                throw std::logic_error("Invalid RequestEvent.");
        }
    }

    //! ModifierDispatch.
    inline
    void dispatch(ModifierDelegate& to)
    {
        switch (which) {
            case REQUEST_STARTED: to.request_started(*this); break;
            default:
                throw std::logic_error("Invalid RequestEvent.");
        }
    }

    //! Raw request line.
    Buffer raw;
    //! Method, e.g., GET.
    Buffer method;
    //! URI.
    Buffer uri;
    //! Protocol, e.g., HTTP/1.0.
    Buffer protocol;
};

/**
 * Response line data: RESPONSE_STARTED.
 *
 * @sa Event
 **/
struct ResponseEvent :
    public Event
{
    //! Constructor.
    explicit
    ResponseEvent(event_e which_);

    //! Constructor.
    ResponseEvent(
        event_e       which_,
        const Buffer& raw_,
        const Buffer& protocol_,
        const Buffer& status_,
        const Buffer& message_
    );

    //! Dispatch.
    inline
    void dispatch(Delegate& to) const
    {
        switch (which) {
            case RESPONSE_STARTED: to.response_started(*this); break;
            default:
                throw std::logic_error("Invalid ResponseEvent.");
        }
    }

    //! ModifierDispatch.
    inline
    void dispatch(ModifierDelegate& to)
    {
        switch (which) {
            case RESPONSE_STARTED: to.response_started(*this); break;
            default:
                throw std::logic_error("Invalid ResponseEvent.");
        }
    }

    //! Raw response line.
    Buffer raw;
    //! Protocol, e.g, HTTP/1.0.
    Buffer protocol;
    //! Status, e.g., 200.
    Buffer status;
    //! Message, e.g., OK.
    Buffer message;
};

//! A header is a pair of buffers: name and value.
typedef std::pair<Buffer, Buffer> header_t;

//! A list of headers.
typedef std::list<header_t> header_list_t;

/**
 * Header data: REQUEST_HEADER, RESPONSE_HEADER.
 **/
struct HeaderEvent :
    public Event
{
    //! Constructor.
    explicit
    HeaderEvent(event_e which_);

    //! Add a header.
    header_t& add(const Buffer& name, const Buffer& value);

    //! Dispatch.
    inline
    void dispatch(Delegate& to) const
    {
        switch (which) {
            case REQUEST_HEADER: to.request_header(*this); break;
            case RESPONSE_HEADER: to.response_header(*this); break;
            default:
                throw std::logic_error("Invalid HeaderEvent.");
        }
    }

    //! Dispatch.
    inline
    void dispatch(ModifierDelegate& to)
    {
        switch (which) {
            case REQUEST_HEADER: to.request_header(*this); break;
            case RESPONSE_HEADER: to.response_header(*this); break;
            default:
                throw std::logic_error("Invalid HeaderEvent.");
        }
    }

    //! Headers.
    header_list_t headers;
};

//! Shared pointer to an event.
typedef boost::shared_ptr<Event> event_p;

//! List of event pointers.
typedef std::list<event_p> event_list_t;

/**
 * Transaction: Events to occur in a transaction.
 **/
struct Transaction
{
    //! Events.
    event_list_t events;

    //! Add CONNECTION_DATA_IN to back of events.
    DataEvent& connection_data_in(const Buffer& data);

    //! Add CONNECTION_DATA_OUT to back of events.
    DataEvent& connection_data_out(const Buffer& data);

    //! Add REQUEST_STARTED to back of events.
    RequestEvent& request_started(
        const Buffer& raw,
        const Buffer& method,
        const Buffer& uri,
        const Buffer& protocol
    );

    //! Add REQUEST_HEADER to back of events.
    HeaderEvent& request_header();

    //! Add REQUEST_HEADER_FINISHED to back of events.
    NullEvent& request_header_finished();

    //! Add REQUEST_BODY to back of events.
    DataEvent& request_body(const Buffer& data);

    //! Add REQUEST_FINISHED to back of events.
    NullEvent& request_finished();

    //! Add RESPONSE_STARTED to back of events.
    ResponseEvent& response_started(
        const Buffer& raw,
        const Buffer& protocol,
        const Buffer& status,
        const Buffer& message
    );

    //! Add RESPONSE_HEADER to back of events.
    HeaderEvent& response_header();

    //! Add RESPONSE_HEADER_FINISHED to back of events.
    NullEvent& response_header_finished();

    //! Add RESPONSE_BODY to back of events.
    DataEvent& response_body(const Buffer& data);

    //! Add RESPONSE_FINISHED to back of events.
    NullEvent& response_finished();

    //! Dispatch events.
    void dispatch(Delegate& to, bool with_delay = false) const;

    //! Dispatch events for modification
    void dispatch(ModifierDelegate& to);
};

//! List of transactions.
typedef std::list<Transaction> transaction_list_t;

/**
 * Connection: Events to occur before, during, and after transactions.
 **/
struct Connection
{
    //! Events to fire before any transaction.
    event_list_t pre_transaction_events;

    //! Transactions.
    transaction_list_t transactions;

    //! Events to fire after all transactions.
    event_list_t post_transaction_events;

    //! Default constructor.
    Connection();

    //! Adds conn_opened and conn_closed events.
    Connection(
        const Buffer& local_ip,
        uint32_t      local_port,
        const Buffer& remote_ip,
        uint32_t      remote_port
    );

    //! Add transaction.
    Transaction& add_transaction();

    //! Add transaction with conn_data_in and conn_data_out events.
    Transaction& add_transaction(
        const Buffer& request,
        const Buffer& response
    );

    //! Add CONNECTION_OPENED to pre_transaction_events.
    ConnectionEvent& connection_opened(
        const Buffer& local_ip,
        uint32_t      local_port,
        const Buffer& remote_ip,
        uint32_t      remote_port
    );

    //! Add CONNECTION_CLOSED to post_transaction_events.
    NullEvent& connection_closed();

    //! Dispatch.
    void dispatch(Delegate& to, bool with_delay = false) const;

    //! Dispatch for modification.
    void dispatch(ModifierDelegate& to);
};

/**
 * Input: Fundamental input type.
 **/
struct Input
{
    //! Default constructor.
    Input();

    //! Constructor.
    explicit
    Input(const std::string& id_);

    //! ID.  Optional.  For human consumption.
    std::string id;
    //! Connection.
    Connection  connection;

    //! Source.  Used for memory management.
    boost::any source;
};


//! Shared pointer to input_t.
typedef boost::shared_ptr<Input> input_p;

} // Input
} // CLIPP
} // IronBee

#endif
