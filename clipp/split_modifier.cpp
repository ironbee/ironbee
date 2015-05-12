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
 * @brief IronBee --- CLIPP Split Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "split_modifier.hpp"

#include <clipp/random_support.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ctime>

using boost::bind;
using namespace std;

namespace IronBee {
namespace CLIPP {

// Split Data

struct SplitDataModifier::State
{
    //! Distribution of targets.
    distribution_t distribution;
};

SplitDataModifier::SplitDataModifier(size_t n) :
    m_state(new State())
{
    m_state->distribution = bind(constant_distribution, n);
}

bool SplitDataModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    BOOST_FOREACH(Input::Transaction& tx, input->connection.transactions) {
        Input::event_p event;
        for (
            Input::event_list_t::iterator i = tx.events.begin();
            i != tx.events.end();
            ++i
        ) {
            event = *i;
            Input::event_list_t::iterator at = boost::next(i);
            if (
                event->which == Input::REQUEST_BODY ||
                event->which == Input::RESPONSE_BODY ||
                event->which == Input::CONNECTION_DATA_IN ||
                event->which == Input::CONNECTION_DATA_OUT
            ) {
                double post_delay = event->post_delay;
                event->post_delay = 0;


                Input::DataEvent* new_event =
                    dynamic_cast<Input::DataEvent*>(event.get());
                if (! new_event) {
                    throw logic_error("Event had type/which mismatch.");
                }

                Input::Buffer original = new_event->data;

                while (original.length > 0) {
                    size_t length = min(
                        original.length,
                        m_state->distribution()
                    );

                    new_event->data = Input::Buffer(original.data, length);
                    original.data += length;
                    original.length -= length;

                    if (original.length > 0) {
                        new_event = new Input::DataEvent(event->which);
                        tx.events.insert(at, Input::event_p(new_event));
                    }
                }

                i = boost::prior(at);
                (*i)->post_delay = post_delay;
            }
        }
    }

    return true;
}

SplitDataModifier SplitDataModifier::uniform(
    unsigned int min,
    unsigned int max
)
{
    if (min > max) {
        throw runtime_error("Min must be less than or equal to max.");
    }
    if (min == 0 || max == 0) {
        throw runtime_error("Min and max must be positive.");
    }
    SplitDataModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::uniform_int_distribution<>(min, max)
    );
    return mod;
}

SplitDataModifier SplitDataModifier::binomial(unsigned int t, double p)
{
    if (t == 0 || p <= 0) {
        throw runtime_error("t and p must be positive.");
    }
    if (p > 1) {
        throw runtime_error("p must be less than or equal to 1.");
    }
    SplitDataModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::binomial_distribution<>(t, p)
    );
    return mod;
}

SplitDataModifier SplitDataModifier::geometric(double p)
{
    if (p < 0 || p >= 1) {
        throw runtime_error("p must be in [0,1)");
    }
    SplitDataModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::geometric_distribution<>(p)
    );
    return mod;
}

SplitDataModifier SplitDataModifier::poisson(double mean)
{
    SplitDataModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::poisson_distribution<>(mean)
    );
    return mod;
}

// SplitHeader

struct SplitHeaderModifier::State
{
    //! Distribution of targets.
    distribution_t distribution;
};

SplitHeaderModifier::SplitHeaderModifier(size_t n) :
    m_state(new State())
{
    m_state->distribution = bind(constant_distribution, n);
}

bool SplitHeaderModifier::operator()(Input::input_p& input)
{
    if (! input) {
        return true;
    }

    BOOST_FOREACH(Input::Transaction& tx, input->connection.transactions) {
        Input::event_p event;
        for (
            Input::event_list_t::iterator i = tx.events.begin();
            i != tx.events.end();
            ++i
        ) {
            event = *i;
            Input::event_list_t::iterator at = boost::next(i);
            if (
                event->which == Input::REQUEST_HEADER ||
                event->which == Input::RESPONSE_HEADER
            ) {
                double post_delay = event->post_delay;
                event->post_delay = 0;

                Input::HeaderEvent* new_event =
                    dynamic_cast<Input::HeaderEvent*>(event.get());
                if (! new_event) {
                    throw logic_error("Event had type/which mismatch.");
                }

                Input::header_list_t original = new_event->headers;
                Input::header_list_t::const_iterator original_i =
                    original.begin();
                size_t original_remaining = original.size();

                while (original_remaining > 0) {
                    size_t length = min(
                        original_remaining,
                        m_state->distribution()
                    );

                    new_event->headers.clear();
                    Input::header_list_t::const_iterator original_j =
                        boost::next(original_i, length);
                    copy(
                        original_i, original_j,
                        back_inserter(new_event->headers)
                    );
                    original_i = original_j;
                    original_remaining -= length;

                    if (original_remaining > 0) {
                        new_event = new Input::HeaderEvent(event->which);
                        tx.events.insert(at, Input::event_p(new_event));
                    }
                }

                i = boost::prior(at);
                (*i)->post_delay = post_delay;
            }
        }
    }

    return true;
}

SplitHeaderModifier SplitHeaderModifier::uniform(
    unsigned int min,
    unsigned int max
)
{
    if (min > max) {
        throw runtime_error("Min must be less than or equal to max.");
    }
    if (min == 0 || max == 0) {
        throw runtime_error("Min and max must be positive.");
    }
    SplitHeaderModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::uniform_int_distribution<>(min, max)
    );
    return mod;
}

SplitHeaderModifier SplitHeaderModifier::binomial(unsigned int t, double p)
{
    if (t == 0 || p <= 0) {
        throw runtime_error("t and p must be positive.");
    }
    if (p > 1) {
        throw runtime_error("p must be less than or equal to 1.");
    }
    SplitHeaderModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::binomial_distribution<>(t, p)
    );
    return mod;
}

SplitHeaderModifier SplitHeaderModifier::geometric(double p)
{
    if (p < 0 || p >= 1) {
        throw runtime_error("p must be in [0,1)");
    }
    SplitHeaderModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::geometric_distribution<>(p)
    );
    return mod;
}

SplitHeaderModifier SplitHeaderModifier::poisson(double mean)
{
    SplitHeaderModifier mod;
    mod.m_state->distribution = make_random_distribution(
        boost::random::poisson_distribution<>(mean)
    );
    return mod;
}

} // CLIPP
} // IronBee
