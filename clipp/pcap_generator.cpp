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
 * @brief IronBee --- CLIPP Generator for PCAP.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "pcap_generator.hpp"

#include <clipp/parse_modifier.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <nids.h>

#include <vector>

#include <cstring>

#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#elif defined(HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif


using namespace std;

// libNIDS makes excessive use of global state and can not be run
// simultaneously.  We get around these limitations by (a) using global state
// and (b) knowing that only one generator is instantiated at a time.

namespace IronBee {
namespace CLIPP {

namespace {
static const size_t c_ip_string_length = 16;

struct data_t
{
    enum last_seen_e {
        REQUEST,
        RESPONSE
    };
    last_seen_e last_seen;

    char local_ip[c_ip_string_length];
    char remote_ip[c_ip_string_length];

    typedef pair<string, string> tx_t;
    typedef list<tx_t> tx_list_t;
    tx_list_t txs;
};

typedef boost::shared_ptr<data_t> data_p;

struct PCAPGlobalState
{
    Input::input_p input;

    size_t input_count;
    string path;
    string filter;
};
static boost::scoped_ptr<PCAPGlobalState> s_global_state;

extern "C" {

void nids_tcp(tcp_stream* ts, void** param)
{
    switch (ts->nids_state) {
    case NIDS_JUST_EST: {
        ts->client.collect = 1;
        ts->server.collect = 1;
        data_t* data = new data_t();
        *param = data;
        data->txs.push_back(data_t::tx_t());
        data->last_seen = data_t::REQUEST;
        break;
    }
    case NIDS_DATA: {
        bool seen_request = (ts->server.count_new != 0);
        bool seen_response = (ts->client.count_new != 0);

        if (seen_request && seen_response) {
            throw logic_error(
                "Misunderstood libNIDS.  Please report as bug."
            );
        }

        data_t* data = reinterpret_cast<data_t*>(*param);

        // Request
        if (seen_request) {
            if (data->last_seen == data_t::RESPONSE) {
                // New transaction
                data->txs.push_back(data_t::tx_t());
            }
            data->txs.back().first.append(
                ts->server.data, ts->server.count_new
            );
            data->last_seen = data_t::REQUEST;
        }

        // Response
        if (seen_response) {
            data->txs.back().second.append(
                ts->client.data, ts->client.count_new
            );
            data->last_seen = data_t::RESPONSE;
        }
        break;
    }
    case NIDS_CLOSE: {
        // Generate input.
        s_global_state->input = boost::make_shared<Input::Input>();
        Input::input_p input = s_global_state->input;
        data_t* data = reinterpret_cast<data_t*>(*param);

        ++s_global_state->input_count;
        input->id = s_global_state->path + ":" +
            boost::lexical_cast<string>(s_global_state->input_count);
        // shared_ptrify data.
        input->source = data_p(data);
        *param = NULL;

        uint32_t ip;
        ip = ts->addr.daddr;
        inet_ntop(AF_INET, &ip, data->local_ip, c_ip_string_length);
        ip = ts->addr.saddr;
        inet_ntop(AF_INET, &ip, data->remote_ip, c_ip_string_length);

        input->connection.connection_opened(
            Input::Buffer(data->local_ip, strlen(data->local_ip)),
            ts->addr.dest,
            Input::Buffer(data->remote_ip, strlen(data->remote_ip)),
            ts->addr.source
        );

        BOOST_FOREACH(
            const data_t::tx_t& tx,
            data->txs
        ) {
            Input::Transaction& itx = input->connection.add_transaction();
            if (! tx.first.empty()) {
                itx.connection_data_in(Input::Buffer(tx.first));
            }
            if (! tx.second.empty()) {
                itx.connection_data_out(Input::Buffer(tx.second));
            }
        }

        input->connection.connection_closed();

        break;
    }
    case NIDS_RESET:
    case NIDS_TIMED_OUT:
        if (*param) {
            data_t* data = reinterpret_cast<data_t*>(*param);
            delete data;
            *param = NULL;
        }
        break;
    }
}

}

}

PCAPGenerator::PCAPGenerator()
{
    // nop
}

PCAPGenerator::PCAPGenerator(
    const std::string& path,
    const std::string& filter
)
{
    s_global_state.reset(new PCAPGlobalState);
    s_global_state->path        = path;
    s_global_state->filter      = filter;
    s_global_state->input_count = 0;

    if (nids_params.filename) {
        free(nids_params.filename);
    }
    nids_params.filename    = strdup(s_global_state->path.c_str());
    if (nids_params.pcap_filter) {
        free(nids_params.pcap_filter);
    }
    nids_params.pcap_filter = strdup(s_global_state->filter.c_str());
    int result = nids_init();
    if (result == 0) {
        throw runtime_error(string("nids_init failed: ") + nids_errbuf);
    }
    // libNIDS...
    nids_register_tcp(reinterpret_cast<void(*)>(&nids_tcp));
}

//! Produce an input.
bool PCAPGenerator::operator()(Input::input_p& input) const
{
    s_global_state->input.reset();

    while (! s_global_state->input) {
        int processed = nids_dispatch(1);
        if (processed == -1) {
            throw runtime_error(
                string("Error in nids_dispatch: ") + nids_errbuf
            );
        }
        else if (processed == 0) {
            // eof
            return false;
        }
    }

    input.swap(s_global_state->input);

    ParseModifier()(input);

    return true;
}

} // CLIPP
} // IronBee
