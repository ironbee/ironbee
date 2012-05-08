#include "input.hpp"

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

using namespace std;
using boost::make_shared;

namespace IronBee {
namespace CLIPP {
namespace Input {

ostream& operator<<(ostream& out, const Buffer& buffer)
{
    out << string(buffer.data, buffer.length);
    return out;
}

Buffer::Buffer()
{
    // nop
}

Buffer::Buffer(const char* data_, size_t length_) :
    data(data_), length(length_)
{
  // nop
}

Buffer::Buffer(const string& s) :
    data(s.c_str()), length(s.length())
{
  // nop
}

string Buffer::to_s() const
{
    return string(data, length);
}

Event::Event(event_e which_) :
    which(which_),
    pre_delay(0),
    post_delay(0)
{
    // nop
}

void Event::dispatch(Delegate& to, bool with_delay) const
{
    if (with_delay && pre_delay > 0) {
        usleep(useconds_t(pre_delay * 1e6));
    }
    this->dispatch(to);
    if (with_delay && post_delay > 0) {
        usleep(useconds_t(post_delay * 1e6));
    }
}

NullEvent::NullEvent(event_e which_) :
    Event(which_)
{
    // nop
}

ConnectionEvent::ConnectionEvent(event_e which_) :
    Event(which_)
{
    // nop
}

ConnectionEvent::ConnectionEvent(
    event_e       which_,
    const Buffer& local_ip_,
    uint32_t      local_port_,
    const Buffer& remote_ip_,
    uint32_t      remote_port_
) :
    Event(which_),
    local_ip(local_ip_),
    local_port(local_port_),
    remote_ip(remote_ip_),
    remote_port(remote_port_)
{
    // nop
}

ConnectionDataEvent::ConnectionDataEvent(event_e which_) :
    Event(which_)
{
    // nop
}

ConnectionDataEvent::ConnectionDataEvent(
    event_e       which_,
    const Buffer& data_
) :
    Event(which_),
    data(data_)
{
    // nop
}

TransactionDataEvent::TransactionDataEvent(event_e which_) :
    Event(which_)
{
    // nop
}

TransactionDataEvent::TransactionDataEvent(
    event_e            which_,
    transaction_data_type_e type_,
    const Buffer&      data_
) :
    Event(which_),
    type(type_),
    data(data_)
{
    // nop
}

RequestEvent::RequestEvent(event_e which_) :
    Event(which_)
{
    // nop
}

RequestEvent::RequestEvent(
    event_e       which_,
    const Buffer& raw_,
    const Buffer& method_,
    const Buffer& uri_,
    const Buffer& protocol_
) :
    Event(which_),
    raw(raw_),
    method(method_),
    uri(uri_),
    protocol(protocol_)
{
    // nop
}

ResponseEvent::ResponseEvent(event_e which_) :
    Event(which_)
{
    // nop
}

ResponseEvent::ResponseEvent(
    event_e       which_,
    const Buffer& raw_,
    const Buffer& protocol_,
    const Buffer& status_,
    const Buffer& message_
) :
    Event(which_),
    raw(raw_),
    protocol(protocol_),
    status(status_),
    message(message_)
{
    // nop
}

HeaderEvent::HeaderEvent(event_e which_) :
    Event(which_)
{
    // nop
}

header_t& HeaderEvent::add(const Buffer& name, const Buffer& value)
{
    headers.push_back(make_pair(name,value));
    return headers.back();
}

ConnectionDataEvent& Transaction::connection_data_in(
    const Buffer& data
)
{
    boost::shared_ptr<ConnectionDataEvent> event =
        make_shared<ConnectionDataEvent>(
            CONNECTION_DATA_IN,
            data
        );
    events.push_back(event);

    return *event;
}

ConnectionDataEvent& Transaction::connection_data_out(
    const Buffer& data
)
{
    boost::shared_ptr<ConnectionDataEvent> event =
        make_shared<ConnectionDataEvent>(
            CONNECTION_DATA_OUT,
            data
        );
    events.push_back(event);

    return *event;
}

TransactionDataEvent& Transaction::transaction_data_in(
    transaction_data_type_e type,
    const Buffer&           data
)
{
    boost::shared_ptr<TransactionDataEvent> event =
        make_shared<TransactionDataEvent>(
            TRANSACTION_DATA_IN,
            type,
            data
        );
    events.push_back(event);

    return *event;
}

TransactionDataEvent& Transaction::transaction_data_out(
    transaction_data_type_e type,
    const Buffer&           data
)
{
    boost::shared_ptr<TransactionDataEvent> event =
        make_shared<TransactionDataEvent>(
            TRANSACTION_DATA_OUT,
            type,
            data
        );
    events.push_back(event);

    return *event;
}

RequestEvent& Transaction::request_started(
    const Buffer& raw,
    const Buffer& method,
    const Buffer& uri,
    const Buffer& protocol
)
{
    boost::shared_ptr<RequestEvent> event =
        make_shared<RequestEvent>(
            REQUEST_STARTED,
            raw,
            method,
            uri,
            protocol
        );
    events.push_back(event);

    return *event;
}

HeaderEvent& Transaction::request_headers()
{
    boost::shared_ptr<HeaderEvent> event =
        make_shared<HeaderEvent>(REQUEST_HEADERS);
    events.push_back(event);

    return *event;
}

TransactionDataEvent& Transaction::request_body(
    const Buffer& data
)
{
    boost::shared_ptr<TransactionDataEvent> event =
        make_shared<TransactionDataEvent>(
            REQUEST_BODY,
            BODY_TYPE,
            data
        );
    events.push_back(event);

    return *event;
}

NullEvent& Transaction::request_finished()
{
    boost::shared_ptr<NullEvent> event =
        make_shared<NullEvent>(REQUEST_FINISHED);
    events.push_back(event);

    return *event;
}

ResponseEvent& Transaction::response_started(
    const Buffer& raw,
    const Buffer& protocol,
    const Buffer& status,
    const Buffer& message
)
{
    boost::shared_ptr<ResponseEvent> event = make_shared<ResponseEvent>(
        RESPONSE_STARTED,
        raw,
        protocol,
        status,
        message
    );
    events.push_back(event);

    return *event;
}

HeaderEvent& Transaction::response_headers()
{
    boost::shared_ptr<HeaderEvent> event =
        make_shared<HeaderEvent>(RESPONSE_HEADERS);
    events.push_back(event);

    return *event;
}

TransactionDataEvent& Transaction::response_body(
    const Buffer& data
)
{
    boost::shared_ptr<TransactionDataEvent> event =
        make_shared<TransactionDataEvent>(
            RESPONSE_BODY,
            BODY_TYPE,
            data
        );
    events.push_back(event);

    return *event;
}

NullEvent& Transaction::response_finished()
{
    boost::shared_ptr<NullEvent> event =
        make_shared<NullEvent>(RESPONSE_FINISHED);
    events.push_back(event);

    return *event;
}

void Transaction::dispatch(Delegate& to, bool with_delay) const
{
    BOOST_FOREACH(const event_p& event, events) {
        event->dispatch(to, with_delay);
    }
}

Connection::Connection()
{
    // nop
}

Connection::Connection(
    const Buffer& local_ip,
    uint32_t      local_port,
    const Buffer& remote_ip,
    uint32_t      remote_port
)
{
    connection_opened(local_ip, local_port, remote_ip, remote_port);
    connection_closed();
}

ConnectionEvent& Connection::connection_opened(
    const Buffer& local_ip,
    uint32_t      local_port,
    const Buffer& remote_ip,
    uint32_t      remote_port
)
{
    boost::shared_ptr<ConnectionEvent> event =
        make_shared<ConnectionEvent>(
            CONNECTION_OPENED,
            local_ip,
            local_port,
            remote_ip,
            remote_port
        );
    pre_transaction_events.push_back(event);

    return *event;
}

NullEvent& Connection::connection_closed()
{
    boost::shared_ptr<NullEvent> event =
        make_shared<NullEvent>(CONNECTION_CLOSED);
    post_transaction_events.push_back(event);

    return *event;
}

Transaction& Connection::add_transaction()
{
    transactions.push_back(Transaction());
    return transactions.back();
}

Transaction& Connection::add_transaction(
    const Buffer& request,
    const Buffer& response
)
{
    Transaction& tx = add_transaction();
    tx.connection_data_in(request);
    tx.connection_data_out(response);

    return tx;
}

void Connection::dispatch(Delegate& to, bool with_delay) const
{
    BOOST_FOREACH(const event_p& event, pre_transaction_events) {
        event->dispatch(to, with_delay);
    }
    BOOST_FOREACH(const Transaction& tx, transactions) {
        tx.dispatch(to, with_delay);
    }
    BOOST_FOREACH(const event_p& event, post_transaction_events) {
        event->dispatch(to, with_delay);
    }
}

Input::Input()
{
    // nop
}

Input::Input(const std::string& id_) :
    id(id_)
{
    // nop
}

} // Input
} // CLIPP
} // IronBee
