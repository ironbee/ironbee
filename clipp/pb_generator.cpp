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
 * @brief IronBee &mdash; CLIPP PB Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "pb_generator.hpp"
#include "clipp.pb.h"

#include <boost/make_shared.hpp>
#include <boost/scoped_array.hpp>
#include <boost/foreach.hpp>

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#elif defined(HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif
#include <fstream>
#include <stdexcept>

using namespace std;
using boost::make_shared;

namespace IronBee {
namespace CLIPP {

struct PBGenerator::State
{
    State(const std::string& path) :
        input(path.c_str(), ios::binary)
    {
        if (! input) {
            throw runtime_error("Could not open " + path + " for reading.");
        }
    }

    ifstream input;
};

PBGenerator::PBGenerator()
{
    // nop
}

PBGenerator::PBGenerator(const std::string& input_path) :
    m_state(make_shared<State>(input_path))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

namespace {

struct data_t
{
    explicit
    data_t(size_t buffer_size) :
        buffer(new char[buffer_size])
    {
        // nop
    }

    boost::scoped_array<char> buffer;
    PB::Input                 pb_input;
};

}

bool PBGenerator::operator()(input_p& input)
{
    if (! m_state->input) {
        return false;
    }

    uint32_t raw_size;
    uint32_t size;

    m_state->input.read(reinterpret_cast<char*>(&raw_size), sizeof(uint32_t));
    if (! m_state->input) {
        return false;
    }
    size = ntohl(raw_size);

    boost::shared_ptr<data_t> data = make_shared<data_t>(size);
    input->source = data;

    m_state->input.read(data->buffer.get(), size);

    google::protobuf::io::ArrayInputStream in(data->buffer.get(), size);
    google::protobuf::io::GzipInputStream unzipped_in(&in);

    if (! data->pb_input.ParseFromZeroCopyStream(&unzipped_in)) {
        throw runtime_error("Failed to parse input.");
    }

    input->id.clear();
    if (data->pb_input.has_id()) {
        input->id = data->pb_input.id();
    }
    input->local_ip.data    = data->pb_input.local_ip().data();
    input->local_ip.length  = data->pb_input.local_ip().length();
    input->local_port       = data->pb_input.local_port();
    input->remote_ip.data   = data->pb_input.remote_ip().data();
    input->remote_ip.length = data->pb_input.remote_ip().length();
    input->remote_port      = data->pb_input.remote_port();

    input->transactions.clear();
    BOOST_FOREACH(
        const PB::Transaction& pb_tx,
        data->pb_input.transaction()
    ) {
        input->transactions.push_back(input_t::transaction_t());
        input_t::transaction_t& tx = input->transactions.back();

        tx.request.data    = pb_tx.raw_request().data();
        tx.request.length  = pb_tx.raw_request().length();
        tx.response.data   = pb_tx.raw_response().data();
        tx.response.length = pb_tx.raw_response().length();
    }

    return true;
}

} // CLIPP
} // IronBee
