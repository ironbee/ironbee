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
 * @brief IronBee --- CLIPP PB Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "pb_generator.hpp"

#include <clipp/clipp.pb.h>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_array.hpp>

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

struct PBGenerator::State
{
    State(const std::string& path_) :
        path(path_)
    {
        if (path == "-") {
            input = &cin;
        }
        else {
            input = new ifstream(path.c_str(), ios::binary);
            if (! *input) {
                throw runtime_error("Could not open " + path + " for reading.");
            }
        }
    }

    ~State()
    {
        if (path != "-") {
            delete input;
        }
    }

    string   path;
    istream* input;
};

PBGenerator::PBGenerator()
{
    // nop
}

PBGenerator::PBGenerator(const std::string& input_path) :
    m_state(boost::make_shared<State>(input_path))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

namespace {

struct data_t
{
    PB::Input                 pb_input;
};

struct pb_to_event :
    public unary_function<const PB::Event, Input::event_p>
{
    Input::event_p operator()(const PB::Event& pb_event) const
    {
        Input::event_p generic;

        Input::event_e which = static_cast<Input::event_e>(pb_event.which());
        switch (which) {
            case Input::UNKNOWN:
                throw runtime_error("Event of UNKNOWN type.");
            case Input::CONNECTION_DATA_IN:
            case Input::CONNECTION_DATA_OUT:
            case Input::REQUEST_BODY:
            case Input::RESPONSE_BODY:
            {
                // DataEvent
                boost::shared_ptr<Input::DataEvent> specific =
                    boost::make_shared<Input::DataEvent>(which);
                generic = specific;
                if (! pb_event.has_data_event()) {
                    throw runtime_error("DataEvent lacking specific data.");
                }
                const PB::DataEvent& pb = pb_event.data_event();
                specific->data.data   = pb.data().data();
                specific->data.length = pb.data().length();
                break;
            }
            case Input::CONNECTION_CLOSED:
            case Input::REQUEST_HEADER_FINISHED:
            case Input::RESPONSE_HEADER_FINISHED:
            case Input::REQUEST_FINISHED:
            case Input::RESPONSE_FINISHED:
            {
                // NUllEvent
                boost::shared_ptr<Input::NullEvent> specific =
                    boost::make_shared<Input::NullEvent>(which);
                generic = specific;
                break;
            }
            case Input::CONNECTION_OPENED:
            {
                // ConnectionEvent
                boost::shared_ptr<Input::ConnectionEvent> specific =
                    boost::make_shared<Input::ConnectionEvent>(which);
                generic = specific;
                if (! pb_event.has_connection_event()) {
                    throw runtime_error(
                        "ConnectionEvent lacking specific data."
                    );
                }
                const PB::ConnectionEvent& pb = pb_event.connection_event();
                if (pb.has_local_ip()) {
                    specific->local_ip = Input::Buffer(pb.local_ip());
                }
                if (pb.has_local_port()) {
                    specific->local_port = pb.local_port();
                }
                if (pb.has_remote_ip()) {
                    specific->remote_ip = Input::Buffer(pb.remote_ip());
                }
                if (pb.has_remote_port()) {
                    specific->remote_port = pb.remote_port();
                }
                break;
            }
            case Input::REQUEST_STARTED:
            {
                // RequestEvent
                boost::shared_ptr<Input::RequestEvent> specific =
                    boost::make_shared<Input::RequestEvent>(which);
                generic = specific;
                if (! pb_event.has_request_event()) {
                    throw runtime_error(
                        "RequestEvent lacking specific data."
                    );
                }
                const PB::RequestEvent& pb = pb_event.request_event();
                if (pb.has_raw()) {
                    specific->raw = Input::Buffer(pb.raw());
                }
                if (pb.has_method()) {
                    specific->method = Input::Buffer(pb.method());
                }
                if (pb.has_uri()) {
                    specific->uri = Input::Buffer(pb.uri());
                }
                if (pb.has_protocol()) {
                    specific->protocol = Input::Buffer(pb.protocol());
                }
                break;
            }
            case Input::RESPONSE_STARTED:
            {
                // ResponseEvent
                boost::shared_ptr<Input::ResponseEvent> specific =
                    boost::make_shared<Input::ResponseEvent>(which);
                generic = specific;
                if (! pb_event.has_response_event()) {
                    throw runtime_error(
                        "ResponseEvent lacking specific data."
                    );
                }
                const PB::ResponseEvent& pb = pb_event.response_event();
                if (pb.has_raw()) {
                    specific->raw = Input::Buffer(pb.raw());
                }
                if (pb.has_status()) {
                    specific->status = Input::Buffer(pb.status());
                }
                if (pb.has_message()) {
                    specific->message = Input::Buffer(pb.message());
                }
                if (pb.has_protocol()) {
                    specific->protocol = Input::Buffer(pb.protocol());
                }
                break;
            }
            case Input::REQUEST_HEADER:
            case Input::RESPONSE_HEADER:
            {
                // HeaderEvent
                boost::shared_ptr<Input::HeaderEvent> specific =
                    boost::make_shared<Input::HeaderEvent>(which);
                generic = specific;
                if (! pb_event.has_header_event()) {
                    throw runtime_error("HeaderEvent lacking specific data.");
                }
                const PB::HeaderEvent& pb = pb_event.header_event();
                BOOST_FOREACH(const PB::Header& pb_header, pb.header()) {
                    specific->headers.push_back(Input::header_t());
                    Input::header_t& header = specific->headers.back();
                    header.first = Input::Buffer(pb_header.name());
                    header.second = Input::Buffer(pb_header.value());
                }
                break;
            }
            default:
                throw runtime_error(
                    "Invalid event type: "
                    + boost::lexical_cast<string>(pb_event.which())
                );
        }

        if (pb_event.has_pre_delay()) {
            generic->pre_delay = pb_event.pre_delay();
        }
        if (pb_event.has_post_delay()) {
            generic->post_delay = pb_event.post_delay();
        }

        return generic;
    }
};

}

bool PBGenerator::operator()(Input::input_p& input)
{
    if (! *m_state->input) {
        return false;
    }

    // Reset Input
    *input = Input::Input();

    uint32_t raw_size;
    uint32_t size;

    m_state->input->read(reinterpret_cast<char*>(&raw_size), sizeof(uint32_t));
    if (! *m_state->input) {
        return false;
    }
    size = ntohl(raw_size);

    boost::shared_ptr<data_t> data = boost::make_shared<data_t>();
    input->source = data;

    boost::scoped_array<char> buffer(new char[size]);

    m_state->input->read(buffer.get(), size);

    google::protobuf::io::ArrayInputStream in(buffer.get(), size);
    google::protobuf::io::GzipInputStream unzipped_in(&in);

    if (! data->pb_input.ParseFromZeroCopyStream(&unzipped_in)) {
        throw runtime_error("Failed to parse input.");
    }

    // Input
    if (data->pb_input.has_id()) {
        input->id = data->pb_input.id();
    }

    // Connection
    const PB::Connection& pb_conn = data->pb_input.connection();

    transform(
        pb_conn.pre_transaction_event().begin(),
        pb_conn.pre_transaction_event().end(),
        back_inserter(input->connection.pre_transaction_events),
        pb_to_event()
    );


    BOOST_FOREACH(const PB::Transaction& pb_tx, pb_conn.transaction())
    {

        Input::Transaction& tx = input->connection.add_transaction();
        transform(
            pb_tx.event().begin(), pb_tx.event().end(),
            back_inserter(tx.events),
            pb_to_event()
        );
    }

    transform(
        pb_conn.post_transaction_event().begin(),
        pb_conn.post_transaction_event().end(),
        back_inserter(input->connection.post_transaction_events),
        pb_to_event()
    );

    return true;
}

} // CLIPP
} // IronBee
