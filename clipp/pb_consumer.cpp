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
 * @brief IronBee --- CLIPP Protobuf Consumer Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "pb_consumer.hpp"

#include <clipp/clipp.pb.h>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <fstream>
#include <stdexcept>

#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#elif defined(HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace  {

void fill_event(PB::Event& pb_event, const Input::Event& event)
{
    if (event.pre_delay > 0) {
        pb_event.set_pre_delay(event.pre_delay);
    }
    if (event.post_delay > 0) {
        pb_event.set_post_delay(event.post_delay);
    }
    pb_event.set_which(event.which);
}


class PBConsumerDelegate :
    public Input::Delegate
{
public:
    explicit
    PBConsumerDelegate(PB::Event& pb_event) :
        m_pb_event(pb_event)
    {
        // nop
    }

    void connection_opened(const Input::ConnectionEvent& event)
    {
        PB::ConnectionEvent& pb_ce = *m_pb_event.mutable_connection_event();
        if (event.local_ip.length > 0) {
            pb_ce.set_local_ip(event.local_ip.data, event.local_ip.length);
        }
        if (event.remote_ip.length > 0) {
            pb_ce.set_remote_ip(event.remote_ip.data, event.remote_ip.length);
        }
        if (event.local_port > 0) {
            pb_ce.set_local_port(event.local_port);
        }
        if (event.remote_port > 0) {
            pb_ce.set_remote_port(event.remote_port);
        }
    }

    void connection_closed(const Input::NullEvent& event)
    {
        // nop
    }

    void connection_data_in(const Input::DataEvent& event)
    {
        PB::DataEvent& pb = *m_pb_event.mutable_data_event();
        if (event.data.length > 0) {
            pb.set_data(event.data.data, event.data.length);
        }
    }

    void connection_data_out(const Input::DataEvent& event)
    {
        // Forward to connection_data_in
        connection_data_in(event);
    }

    void request_started(const Input::RequestEvent& event)
    {
        PB::RequestEvent& pb =
            *m_pb_event.mutable_request_event();
        if (event.raw.length > 0) {
            pb.set_raw(event.raw.data, event.raw.length);
        }
        if (event.method.length > 0) {
            pb.set_method(event.method.data, event.method.length);
        }
        if (event.uri.length > 0) {
            pb.set_uri(event.uri.data, event.uri.length);
        }
        if (event.protocol.length > 0) {
            pb.set_protocol(event.protocol.data, event.protocol.length);
        }
    }

    void request_header(const Input::HeaderEvent& event)
    {
        PB::HeaderEvent& pb = *m_pb_event.mutable_header_event();
        BOOST_FOREACH(const Input::header_t& header, event.headers)
        {
            PB::Header& h = *pb.add_header();
            h.set_name(header.first.data, header.first.length);
            h.set_value(header.second.data, header.second.length);
        }
    }

    void request_header_finished(const Input::NullEvent& event)
    {
        // nop
    }

    void request_body(const Input::DataEvent& event)
    {
        // Forward to connection_data_in
        connection_data_in(event);
    }

    void request_finished(const Input::NullEvent& event)
    {
        // nop
    }

    void response_started(const Input::ResponseEvent& event)
    {
        PB::ResponseEvent& pb =
            *m_pb_event.mutable_response_event();
        if (event.raw.length > 0) {
            pb.set_raw(event.raw.data, event.raw.length);
        }
        if (event.protocol.length > 0) {
            pb.set_protocol(event.protocol.data, event.protocol.length);
        }
        if (event.status.length > 0) {
            pb.set_status(event.status.data, event.status.length);
        }
        if (event.message.length > 0) {
            pb.set_message(event.message.data, event.message.length);
        }
    }

    void response_header(const Input::HeaderEvent& event)
    {
        // Forward to request_header
        request_header(event);
    }

    void response_header_finished(const Input::NullEvent& event)
    {
        // nop
    }

    void response_body(const Input::DataEvent& event)
    {
        // Forward to connection_data_in
        connection_data_in(event);
    }

    void response_finished(const Input::NullEvent& event)
    {
        // nop
    }

private:
    PB::Event& m_pb_event;
};

}

struct PBConsumer::State
{
    explicit
    State(const std::string& path) :
        file(new ofstream(path.c_str(), ios::binary | ios::app)),
        output(file.get())
    {
        if (! output) {
            throw runtime_error("Could not open " + path + " for writing.");
        }
    }

    explicit
    State(ostream* out) :
        output(out)
    {
        // nop
    }

    boost::scoped_ptr<ofstream> file;
    ostream* output;
};

PBConsumer::PBConsumer()
{
    // nop
}

PBConsumer::PBConsumer(const std::string& output_path) :
    m_state(boost::make_shared<State>(output_path))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

PBConsumer::PBConsumer(ostream& out) :
    m_state(boost::make_shared<State>(&out))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

bool PBConsumer::operator()(const Input::input_p& input)
{
    if (! m_state || ! *m_state->output) {
        return false;
    }

    PB::Input pb_input;

    if (! input->id.empty()) {
        pb_input.set_id(input->id);
    }

    PB::Connection& pb_connection = *pb_input.mutable_connection();

    BOOST_FOREACH(
        const Input::event_p event,
        input->connection.pre_transaction_events
    )
    {
        PB::Event& pb_event = *pb_connection.add_pre_transaction_event();
        fill_event(pb_event, *event);
        PBConsumerDelegate delegate(pb_event);
        event->dispatch(delegate);
    }

    BOOST_FOREACH(
        const Input::Transaction& tx,
        input->connection.transactions
    )
    {
        PB::Transaction& pb_tx = *pb_connection.add_transaction();
        BOOST_FOREACH(const Input::event_p event, tx.events) {
            PB::Event& pb_event = *pb_tx.add_event();
            fill_event(pb_event, *event);
            PBConsumerDelegate delegate(pb_event);
            event->dispatch(delegate);
        }
    }

    BOOST_FOREACH(
        const Input::event_p event,
        input->connection.post_transaction_events
    )
    {
        PB::Event& pb_event = *pb_connection.add_post_transaction_event();
        fill_event(pb_event, *event);
        PBConsumerDelegate delegate(pb_event);
        event->dispatch(delegate);
    }

    string buffer;
    google::protobuf::io::StringOutputStream output(&buffer);
    google::protobuf::io::GzipOutputStream zipped_output(&output);

    pb_input.SerializeToZeroCopyStream(&zipped_output);
    zipped_output.Close();

    uint32_t size = buffer.length();
    uint32_t nsize = htonl(size);
    m_state->output->write(
        reinterpret_cast<const char*>(&nsize), sizeof(uint32_t)
    );
    m_state->output->write(buffer.data(), size);

    return true;
}

} // CLIPP
} // IronBee
