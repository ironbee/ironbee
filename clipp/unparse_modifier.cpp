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
 * @brief IronBee --- CLIPP Unparse Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <clipp/unparse_modifier.hpp>

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace {

static const char* c_eol("\r\n");

struct data_t
{
    boost::any old_source;
    typedef pair<string, string> txdata_t;
    typedef list<txdata_t> txdata_list_t;
    txdata_list_t txdatas;
};

class UnparseDelegate :
    public Input::Delegate
{
private:
    static
    void headers(string& out, const Input::HeaderEvent& event)
    {
        UnparseModifier::unparse_headers(out, event.headers);
    }

    static
    void body(string& out, const Input::DataEvent& event)
    {
        if (event.data.data) {
            out += c_eol;
            out.append(event.data.data, event.data.length);
        }
    }

public:
    explicit
    UnparseDelegate(
        data_t::txdata_t& txdata
    ) :
        m_txdata(txdata)
    {
        // nop
    }

    void request_started(const Input::RequestEvent& event)
    {
        UnparseModifier::unparse_request_line(m_txdata.first, event);
        m_txdata.first += c_eol;
    }

    void request_header(const Input::HeaderEvent& event)
    {
        headers(m_txdata.first, event);
    }

    void request_body(const Input::DataEvent& event)
    {
        body(m_txdata.first, event);
    }

    void response_started(const Input::ResponseEvent& event)
    {
        UnparseModifier::unparse_response_line(m_txdata.second, event);
        m_txdata.second += c_eol;
    }

    void response_header(const Input::HeaderEvent& event)
    {
        headers(m_txdata.second, event);
    }

    void response_body(const Input::DataEvent& event)
    {
        body(m_txdata.second, event);
    }

private:
    data_t::txdata_t&   m_txdata;
};

void add_events(Input::Transaction& tx, const data_t::txdata_t txdata)
{
    tx.connection_data_in(Input::Buffer(txdata.first));
    tx.connection_data_out(Input::Buffer(txdata.second));
}

}

bool UnparseModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    boost::shared_ptr<data_t> data = boost::make_shared<data_t>();
    data->old_source   = input->source;

    Input::transaction_list_t new_transactions;
    BOOST_FOREACH(Input::Transaction& tx, input->connection.transactions) {
        new_transactions.push_back(Input::Transaction());
        Input::Transaction& new_tx = new_transactions.back();

        data_t::txdata_t* current_txdata = NULL;
        BOOST_FOREACH(Input::event_p& event, tx.events) {
            switch (event->which) {
                case Input::CONNECTION_DATA_IN:
                case Input::CONNECTION_DATA_OUT:
                    new_tx.events.push_back(event);
                    break;
                case Input::REQUEST_STARTED: {
                    if (current_txdata) {
                        add_events(new_tx, *current_txdata);
                    }
                    data->txdatas.push_back(data_t::txdata_t());
                    current_txdata = &data->txdatas.back();
                    UnparseDelegate delegate(*current_txdata);
                    event->dispatch(delegate);
                    break;
                }
                case Input::REQUEST_FINISHED:
                case Input::RESPONSE_FINISHED:
                    // nop
                    break;
                default: {
                    if (! current_txdata) {
                        throw runtime_error(
                            "Expecting connection data in/out or request "
                            "started but got something else."
                        );
                    }
                    UnparseDelegate delegate(*current_txdata);
                    event->dispatch(delegate);
                }
            }
        }
        if (current_txdata) {
            add_events(new_tx, *current_txdata);
        }
    }
    input->connection.transactions.swap(new_transactions);

    return true;
}

void UnparseModifier::unparse_headers(
    std::string&                out,
    const Input::header_list_t& headers
)
{
    BOOST_FOREACH(const Input::header_t& header, headers) {
        if (header.first.data) {
            out.append(header.first.data, header.first.length);
            out += ": ";
            if (header.second.data) {
                out.append(
                    header.second.data,
                    header.second.length
                );
            }
            out += c_eol;
        }
    }
}


void UnparseModifier::unparse_request_line(
    std::string&               out,
    const Input::RequestEvent& event
)
{
    if (event.raw.data) {
        out.append(event.raw.data, event.raw.length);
    }
    else {
        if (event.method.data) {
            out.append(event.method.data, event.method.length);
            out.append(" ");
        }
        if (event.uri.data) {
            out.append(event.uri.data, event.uri.length);
            out.append(" ");
        }
        if (event.protocol.data) {
            out.append(
                event.protocol.data,
                event.protocol.length
            );
        }
    }
}

void UnparseModifier::unparse_response_line(
    std::string&               out,
    const Input::ResponseEvent& event
)
{
    if (event.raw.data) {
        out.append(event.raw.data, event.raw.length);
    }
    else {
        if (event.protocol.data) {
            out.append(
                event.protocol.data,
                event.protocol.length
            );
            out.append(" ");
        }
        if (event.status.data) {
            out.append(
                event.status.data,
                event.status.length
            );
            out.append(" ");
        }
        if (event.message.data) {
            out.append(
                event.message.data,
                event.message.length
            );
        }
    }
}

} // CLIPP
} // IronBee