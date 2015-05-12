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
 * @brief IronBee --- CLIPP Edit Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "edit_modifier.hpp"

#include <clipp/parse_modifier.hpp>
#include <clipp/unparse_modifier.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_array.hpp>
#include <boost/shared_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <fstream>
#include <iostream>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace  {

struct data_t
{
    boost::any old_source;

    typedef list<string> buffer_list_t;
    buffer_list_t buffers;
};

void read_all(ifstream& in, string& dst)
{
    in.seekg(0, ios::end);
    int length = in.tellg();
    in.seekg(0, ios::beg);

    boost::scoped_array<char> buffer(new char[length]);
    in.read(buffer.get(), length);

    dst = string(buffer.get(), length);
}

void textify_request(string& text, const Input::Event& event)
{
    const Input::RequestEvent& specific =
        dynamic_cast<const Input::RequestEvent&>(event);

    UnparseModifier::unparse_request_line(text, specific);
}

void untextify_request(Input::Event& event, const string& text)
{
    Input::RequestEvent& specific =
        dynamic_cast<Input::RequestEvent&>(event);

    const char* data = text.c_str();
    ParseModifier::parse_request_line(specific, data, data + text.length());
}

void textify_response(string& text, const Input::Event& event)
{
    const Input::ResponseEvent& specific =
        dynamic_cast<const Input::ResponseEvent&>(event);

    UnparseModifier::unparse_response_line(text, specific);
}

void untextify_response(Input::Event& event, const string& text)
{
    Input::ResponseEvent& specific =
        dynamic_cast<Input::ResponseEvent&>(event);

    const char* data = text.c_str();
    ParseModifier::parse_response_line(specific, data, data + text.length());
}

void textify_header(string& text, const Input::Event& event)
{
    const Input::HeaderEvent& specific =
        dynamic_cast<const Input::HeaderEvent&>(event);

    UnparseModifier::unparse_headers(text, specific.headers);
}

void untextify_header(Input::Event& event, const string& text)
{
    Input::HeaderEvent& specific =
        dynamic_cast<Input::HeaderEvent&>(event);

    const char* data = text.c_str();
    specific.headers.clear();
    ParseModifier::parse_header_block(
        specific.headers,
        data, data + text.length()
    );
}

void textify_data(string& text, const Input::Event& event)
{
    const Input::DataEvent& specific =
        dynamic_cast<const Input::DataEvent&>(event);

    text = specific.data.to_s();
}

void untextify_data(Input::Event& event, const string& text)
{
    Input::DataEvent& specific =
        dynamic_cast<Input::DataEvent&>(event);
    specific.data = Input::Buffer(text);
}

} // Anonymous

struct EditModifier::State
{
    typedef boost::function<void(string&, const Input::Event&)> textify_t;
    typedef boost::function<void(Input::Event&, const string&)> untextify_t;

    textify_t      textify;
    untextify_t    untextify;
    Input::event_e which;
};

EditModifier::EditModifier(const string& which) :
    m_state(boost::make_shared<State>())
{
    if (which == "request") {
        m_state->textify   = textify_request;
        m_state->untextify = untextify_request;
        m_state->which     = Input::REQUEST_STARTED;
    }
    else if (which == "response") {
        m_state->textify   = textify_response;
        m_state->untextify = untextify_response;
        m_state->which     = Input::RESPONSE_STARTED;
    }
    else if (which == "request_header") {
        m_state->textify   = textify_header;
        m_state->untextify = untextify_header;
        m_state->which     = Input::REQUEST_HEADER;
    }
    else if (which == "response_header") {
        m_state->textify   = textify_header;
        m_state->untextify = untextify_header;
        m_state->which     = Input::RESPONSE_HEADER;
    }
    else if (which == "request_body") {
        m_state->textify   = textify_data;
        m_state->untextify = untextify_data;
        m_state->which     = Input::REQUEST_BODY;
    }
    else if (which == "response_body") {
        m_state->textify   = textify_data;
        m_state->untextify = untextify_data;
        m_state->which     = Input::RESPONSE_BODY;
    }
    else if (which == "connection_in") {
        m_state->textify   = textify_data;
        m_state->untextify = untextify_data;
        m_state->which     = Input::CONNECTION_DATA_IN;
    }
    else if (which == "connection_out") {
        m_state->textify   = textify_data;
        m_state->untextify = untextify_data;
        m_state->which     = Input::CONNECTION_DATA_OUT;
    }
    else {
        throw runtime_error("Unknown which: " + which);
    }
}

bool EditModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    using namespace boost::filesystem;

    const char* editor = getenv("EDITOR");
    if (editor == NULL) {
        editor = "vi";
    }

    path tempdir = temp_directory_path();
    path tempfile = tempdir / (
        "clipp" + boost::lexical_cast<string>(getpid()) + ".txt"
    );

    boost::shared_ptr<data_t> data = boost::make_shared<data_t>();
    data->old_source = input->source;
    input->source = data;
    BOOST_FOREACH(Input::Transaction& tx, input->connection.transactions) {
        BOOST_FOREACH(Input::event_p& event, tx.events) {
            if (event->which == m_state->which) {
                data->buffers.push_back(string());
                string& text = data->buffers.back();
                m_state->textify(text, *event);
                ofstream of(tempfile.c_str());
                if (! of) {
                    throw runtime_error(
                        string("Could not open ") + tempfile.string() +
                            " for writing."
                    );
                }
                of.write(text.data(), text.length());
                of.close();

                int result = system(
                    (string(editor) + " " + tempfile.string()).c_str()
                );
                if (result != 0) {
                    continue;
                }

                ifstream in(tempfile.c_str());
                if (! in) {
                    throw runtime_error(
                        string("Could not open ") + tempfile.string() +
                            " for reading."
                    );
                }
                read_all(in, text);
                in.close();
                m_state->untextify(*event, text);

                remove(tempfile);
            }
        }
    }

    return true;
}

} // CLIPP
} // IronBee
