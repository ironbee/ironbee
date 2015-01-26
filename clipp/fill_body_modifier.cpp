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
 * @brief IronBee --- CLIPP Fill Body Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "fill_body_modifier.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <vector>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace  {

static const int64_t c_context_length_limit = 1e6; // 1 MB

size_t extract_longest_content_length(const Input::HeaderEvent& event)
{
    static const string content_length("Content-Length");

    size_t result = 0;

    BOOST_FOREACH(const Input::header_t& header, event.headers) {
        if (boost::iequals(
            content_length,
            boost::make_iterator_range(
                header.first.data,
                header.first.data + header.first.length
            )
        )) {
            int64_t real_length = 0;
            try {
                real_length = boost::lexical_cast<int64_t>(
                    header.second.to_s()
                );
            }
            catch (boost::bad_lexical_cast) {
                // nop
            }
            if (real_length < 0) {
                real_length = 0;
            }
            else if (real_length > c_context_length_limit) {
                real_length = c_context_length_limit;
            }

            size_t length = real_length;

            if (length > result) {
                result = length;
            }
        }
    }
    return result;
}

class FillBodyLengthDelegate :
    public Input::Delegate
{
public:
    FillBodyLengthDelegate(
        size_t& max_length,
        bool&   has_data_event
    ) :
        m_max_length(max_length),
        m_has_data_event(has_data_event)
    {
        // nop
    }

    void response_header(const Input::HeaderEvent& event)
    {
        handle_header(event);
    }

    void request_header(const Input::HeaderEvent& event)
    {
        handle_header(event);
    }

    void request_body(const Input::DataEvent& event)
    {
        m_has_data_event = true;
    }

    void response_body(const Input::DataEvent& event)
    {
        m_has_data_event = true;
    }

private:
    void handle_header(const Input::HeaderEvent& event)
    {
        size_t length = extract_longest_content_length(event);
        if (length > m_max_length) {
            m_max_length = length;
        }
    }

    size_t& m_max_length;
    bool&  m_has_data_event;
};

class FillBodyDelegate :
    public Input::ModifierDelegate
{
public:
    explicit
    FillBodyDelegate(const vector<char>& data) :
        m_data(data),
        m_most_recent_length(0)
    {
        // nop
    }

    void response_header(Input::HeaderEvent& event)
    {
        handle_header(event);
    }

    void request_header(Input::HeaderEvent& event)
    {
        handle_header(event);
    }

    void request_body(Input::DataEvent& event)
    {
        handle_data(event);
    }

    void response_body(Input::DataEvent& event)
    {
        handle_data(event);
    }

private:
    void handle_header(const Input::HeaderEvent& event)
    {
        m_most_recent_length = extract_longest_content_length(event);
    }

    void handle_data(Input::DataEvent& event)
    {
        if (m_most_recent_length > m_data.size()) {
            throw logic_error(
                "Insanity error.  Found larger content length than expected."
                "  Please report as bug."
            );
        }
        event.data = Input::Buffer(m_data.data(), m_most_recent_length);
    }

    const vector<char>& m_data;
    size_t m_most_recent_length;
};

struct data_t
{
    //! Original source.
    boost::any original_source;

    //! Use a single set of @s for all bodies in connection.
    vector<char> data;
};

} // Anonymous

bool FillBodyModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    // Pass 1: Find maximum content length.
    size_t max_body_length = 0;
    bool has_data_event;
    FillBodyLengthDelegate length_delegate(max_body_length, has_data_event);

    input->connection.dispatch(length_delegate);

    if (max_body_length == 0) {
        // nothing to do;
        return true;
    }

    // Allocate a single big block of data to use for all bodies.
    boost::shared_ptr<data_t> data = boost::make_shared<data_t>();
    data->original_source = input->source;
    input->source = data;
    data->data.assign(max_body_length, '@');

    // Pass 2: Add missing data events.
    BOOST_FOREACH(
        Input::Transaction& tx,
        input->connection.transactions
    )
    {
        size_t content_length = 0;
        FillBodyLengthDelegate delegate(content_length, has_data_event);

        has_data_event = false;
        Input::event_list_t::iterator last_header_i = tx.events.begin();
        for (
            Input::event_list_t::iterator i = tx.events.begin();
            i != tx.events.end();
            ++i
        ) {
            const Input::Event& event = **i;
            switch (event.which) {
            case Input::REQUEST_STARTED:
            case Input::RESPONSE_STARTED:
                content_length = 0;
                has_data_event = false;
            case Input::REQUEST_FINISHED:
                if (content_length > 0 && ! has_data_event) {
                    tx.events.insert(
                        boost::next(last_header_i),
                        boost::make_shared<Input::DataEvent>(
                            Input::REQUEST_BODY,
                            Input::Buffer()
                        )
                    );
                }
                break;
            case Input::RESPONSE_FINISHED:
                if (content_length > 0 && ! has_data_event) {
                    tx.events.insert(
                        boost::next(last_header_i),
                        boost::make_shared<Input::DataEvent>(
                            Input::RESPONSE_BODY,
                            Input::Buffer()
                        )
                    );
                }
                break;
            case Input::REQUEST_HEADER_FINISHED:
            case Input::RESPONSE_HEADER_FINISHED:
                last_header_i = i;
                // fall through
            default:
                event.dispatch(delegate);
            }
        }
    }

    // Pass 3: Set all data events to point to bogus data.
    FillBodyDelegate fill_delegate(data->data);
    input->connection.dispatch(fill_delegate);

    return true;
}

} // CLIPP
} // IronBee
