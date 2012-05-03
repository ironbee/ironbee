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
 * @brief IronBee &mdash; CLIPP Protobuf Consumer Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "pb_consumer.hpp"
#include "clipp.pb.h"

#include <boost/make_shared.hpp>
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

struct PBConsumer::State
{
    State(const std::string& path) :
        output(path.c_str(), ios::binary)
    {
        if (! output) {
            throw runtime_error("Could not open " + path + " for writing.");
        }
    }

    ofstream output;
};

PBConsumer::PBConsumer()
{
    // nop
}

PBConsumer::PBConsumer(const std::string& output_path) :
    m_state(make_shared<State>(output_path))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

bool PBConsumer::operator()(const input_t& input)
{
    if (! m_state || ! m_state->output) {
        return false;
    }

    PB::Input pb_input;
    pb_input.set_local_ip(input.local_ip.data, input.local_ip.length);
    pb_input.set_local_port(input.local_port);
    pb_input.set_remote_ip(input.remote_ip.data, input.remote_ip.length);
    pb_input.set_remote_port(input.remote_port);
    pb_input.set_id(input.id);

    BOOST_FOREACH(const input_t::transaction_t& tx, input.transactions) {
        PB::Transaction* pb_tx = pb_input.add_transaction();
        pb_tx->set_raw_request(tx.request.data, tx.request.length);
        pb_tx->set_raw_response(tx.response.data, tx.response.length);
    }

    string buffer;
    google::protobuf::io::StringOutputStream output(&buffer);
    google::protobuf::io::GzipOutputStream zipped_output(&output);

    pb_input.SerializeToZeroCopyStream(&zipped_output);
    zipped_output.Close();

    uint32_t size = buffer.length();
    uint32_t nsize = htonl(size);
    m_state->output.write(
        reinterpret_cast<const char*>(&nsize), sizeof(uint32_t)
    );
    m_state->output.write(buffer.data(), size);

    return true;
}

} // CLIPP
} // IronBee
