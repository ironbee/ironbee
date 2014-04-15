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
 * @brief IronBee --- CLIPP Parse Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "parse_modifier.hpp"

#include <boost/function.hpp>
#include <boost/fusion/adapted/boost_tuple.hpp>
#include <boost/make_shared.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace {

typedef boost::iterator_range<const char*> span_t;
typedef boost::tuple<span_t, span_t> two_span_t;
typedef boost::tuple<span_t, span_t, span_t> three_span_t;

Input::Buffer to_buffer(const span_t& span)
{
    if (span.empty()) {
        return Input::Buffer();
    }
    return Input::Buffer(span.begin(), span.size());
}

span_t from_buffer(const Input::Buffer& buffer)
{
    return span_t(buffer.data, buffer.data + buffer.length);
}

// Read until \n or \r\n.
// Updates @a span to start just after line.
span_t fetch_line(span_t& span)
{
    using namespace boost::spirit::qi;

    span_t line;

    const char* new_begin = span.begin();
    bool success = parse(
        new_begin, span.end(),
        raw[*(ascii::char_ - ascii::char_("\n"))]
            >> omit["\n"],
        line
    );
    if (success) {
        span = span_t(new_begin, span.end());
        const char* end = line.end();
        if (end > line.begin() && *(end-1) == '\r') {
            --end;
        }
        return span_t(line.begin(), end);
    }
    else {
        // No end of line, return entire buffer as line.
        span = span_t(span.end(), span.end());
        return span_t(new_begin, span.end());
    }
    // unreachable
}

three_span_t parse_first_line(const span_t& span)
{
    using namespace boost::spirit::qi;

    three_span_t result;
    // Parse should only fail on an empty span in which case an empty
    // result is fine.
    parse(
        span.begin(), span.end(),
        omit[*space] >>
        raw[+(ascii::char_-' ')] >> omit[*space] >>
        raw[*(ascii::char_-' ')] >> omit[*space] >>
        raw[*ascii::char_] >> omit[*space],
        result
    );

    return result;
}

two_span_t parse_header(const span_t& span)
{
    using namespace boost::spirit::qi;

    two_span_t result;
    // Parse should never fail.
    bool success = parse(
        span.begin(), span.end(),
        raw[*(ascii::char_ - ':')] >>
            -(omit[lit(':') >> *space] >> raw[+ascii::char_]),
        result
    );
    if (! success) {
        throw logic_error(
            "Insanity error: Parse header fialed."
            "Please report as bug."
        );
    }

    return result;
}

template <typename StartEventType>
three_span_t convert_first_line(
    Input::event_list_t& events,
    span_t&              input,
    Input::event_e       start_event,
    double               pre_delay
)
{
    span_t current_line = fetch_line(input);
    three_span_t info = parse_first_line(current_line);
    events.push_back(
        boost::make_shared<StartEventType>(
            start_event,
            to_buffer(current_line),
            to_buffer(info.get<0>()),
            to_buffer(info.get<1>()),
            to_buffer(info.get<2>())
        )
    );
    events.back()->pre_delay = pre_delay;

    return info;
}

void convert_headers(
    Input::event_list_t& events,
    span_t&              input,
    Input::event_e       header_event,
    Input::event_e       header_finished_event
)
{
    Input::header_list_t headers;
    const char* begin = input.begin();
    ParseModifier::parse_header_block(headers, begin, input.end());
    input = span_t(begin, input.end());
    if (! headers.empty()) {
        boost::shared_ptr<Input::HeaderEvent> specific =
            boost::make_shared<Input::HeaderEvent>(header_event);
        specific->headers.swap(headers);
        events.push_back(specific);
    }
    events.push_back(
        boost::make_shared<Input::NullEvent>(header_finished_event)
    );
}

void convert_remainder(
    Input::event_list_t& events,
    span_t&              input,
    Input::event_e       body_event,
    Input::event_e       finished_event,
    double               pre_delay,
    double               post_delay
)
{
    events.push_back(
        boost::make_shared<Input::DataEvent>(body_event, to_buffer(input))
    );
    events.back()->pre_delay = pre_delay;
    pre_delay = 0;

    events.push_back(
        boost::make_shared<Input::NullEvent>(finished_event)
    );
    events.back()->pre_delay  = pre_delay;
    events.back()->post_delay = post_delay;
}

struct data_t
{
    boost::any old_source;
    Input::transaction_list_t transactions;
};

}

bool ParseModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    boost::shared_ptr<data_t> data = boost::make_shared<data_t>();
    data->old_source   = input->source;
    data->transactions = input->connection.transactions;
    input->source      = data;

    Input::transaction_list_t new_transactions;
    enum last_seen_e {
        NOTHING,
        IN,
        OUT
    };
    last_seen_e last_seen = NOTHING;
    bool is_http09 = false;
    BOOST_FOREACH(Input::Transaction& tx, input->connection.transactions) {
        new_transactions.push_back(Input::Transaction());
        Input::Transaction& new_tx = new_transactions.back();
        BOOST_FOREACH(const Input::event_p& event, tx.events) {
            switch (event->which) {
                case Input::CONNECTION_DATA_IN: {
                    if (last_seen == IN) {
                        throw runtime_error(
                            "@parse does not support repeated connection "
                            "data in events."
                        );
                    }
                    last_seen = IN;
                    Input::DataEvent& specific =
                        dynamic_cast<Input::DataEvent&>(
                            *event
                        );
                    span_t input = from_buffer(specific.data);
                    three_span_t info =
                        convert_first_line<Input::RequestEvent>(
                            new_tx.events,
                            input,
                            Input::REQUEST_STARTED,
                            specific.pre_delay
                        );
                    is_http09 = info.get<2>().empty();

                    if (! is_http09) {
                        convert_headers(
                            new_tx.events,
                            input,
                            Input::REQUEST_HEADER,
                            Input::REQUEST_HEADER_FINISHED
                        );
                    }
                    else {
                        new_tx.events.push_back(
                            boost::make_shared<Input::HeaderEvent>(Input::REQUEST_HEADER)
                        );
                        new_tx.events.push_back(
                            boost::make_shared<Input::NullEvent>(Input::REQUEST_HEADER_FINISHED)
                        );
                    }

                    convert_remainder(
                        new_tx.events,
                        input,
                        Input::REQUEST_BODY,
                        Input::REQUEST_FINISHED,
                        0,
                        specific.post_delay
                    );
                    break;
                }
                case Input::CONNECTION_DATA_OUT: {
                    if (last_seen == OUT) {
                        throw runtime_error(
                            "@parse does not support repeated connection "
                            "data out events."
                        );
                    }
                    last_seen = OUT;
                    Input::DataEvent& specific =
                        dynamic_cast<Input::DataEvent&>(
                            *event
                        );

                    span_t input = from_buffer(specific.data);

                    if (! is_http09) {
                        convert_first_line<Input::ResponseEvent>(
                            new_tx.events,
                            input,
                            Input::RESPONSE_STARTED,
                            specific.pre_delay
                        );
                        convert_headers(
                            new_tx.events,
                            input,
                            Input::RESPONSE_HEADER,
                            Input::RESPONSE_HEADER_FINISHED
                        );
                    }
                    else {
                        new_tx.events.push_back(
                            boost::make_shared<Input::ResponseEvent>(
                                Input::RESPONSE_STARTED,
                                Input::Buffer(),
                                Input::Buffer(),
                                Input::Buffer(),
                                Input::Buffer()
                            )
                        );
                        new_tx.events.back()->pre_delay = specific.pre_delay;
                        new_tx.events.push_back(
                            boost::make_shared<Input::HeaderEvent>(Input::RESPONSE_HEADER)
                        );
                        new_tx.events.push_back(
                            boost::make_shared<Input::NullEvent>(Input::RESPONSE_HEADER_FINISHED)
                        );
                    }

                    convert_remainder(
                        new_tx.events,
                        input,
                        Input::RESPONSE_BODY,
                        Input::RESPONSE_FINISHED,
                        0,
                        specific.post_delay
                    );
                    break;
                }
                default:
                    new_tx.events.push_back(event);
            }
        }
    }
    input->connection.transactions.swap(new_transactions);

    return true;
}

void ParseModifier::parse_header_block(
    Input::header_list_t& headers,
    const char*& begin, const char* end
)
{
    span_t input(begin, end);
    while (! input.empty()) {
        span_t current_line = fetch_line(input);
        if (current_line.empty()) {
            // End of headers
            break;
        }
        two_span_t info = parse_header(current_line);
        headers.push_back(make_pair(
            to_buffer(info.get<0>()),
            to_buffer(info.get<1>())
        ));
    }
    begin = input.begin();
}

void ParseModifier::parse_request_line(
    Input::RequestEvent& event,
    const char* begin, const char* end
)
{
    span_t input(begin, end);
    three_span_t info = parse_first_line(input);
    event.raw      = to_buffer(input);
    event.method   = to_buffer(info.get<0>());
    event.uri      = to_buffer(info.get<1>());
    event.protocol = to_buffer(info.get<2>());
}

void ParseModifier::parse_response_line(
    Input::ResponseEvent& event,
    const char* begin, const char* end
)
{
    span_t input(begin, end);
    three_span_t info = parse_first_line(input);
    event.raw      = to_buffer(input);
    event.protocol = to_buffer(info.get<0>());
    event.status   = to_buffer(info.get<1>());
    event.message  = to_buffer(info.get<2>());
}

} // CLIPP
} // IronBee
