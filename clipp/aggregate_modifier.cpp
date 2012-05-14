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
 * @brief IronBee &mdash; CLIPP Aggregate Modifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "aggregate_modifier.hpp"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/random.hpp>
#include <boost/make_shared.hpp>

#include <ctime>

using boost::bind;
using namespace std;

namespace IronBee {
namespace CLIPP {

namespace  {

typedef boost::function<size_t()> distribution_t;

size_t constant_dist(size_t n)
{
    return n;
}

template <typename DistributionType>
class random_dist
{
public:
    explicit
    random_dist(DistributionType dist) :
        m_rng(clock()),
        m_dist(dist)
    {
        // nop
    }

    size_t operator()()
    {
        size_t result = m_dist(m_rng);
        if (result < 1) {
            result = 1;
        }
        return result;
    }

protected:
    boost::random::mt19937 m_rng;
    DistributionType       m_dist;
};

template <typename DistributionType>
random_dist<DistributionType> make_random_dist(DistributionType dist)
{
    return random_dist<DistributionType>(dist);
}

struct data_t
{
    list<boost::any> sources;
};

} // Anonymous

struct AggregateModifier::State
{
    //! Distribution of targets.
    distribution_t distribution;

    //! Current target.
    size_t n;
    //! Current aggregate.
    Input::input_p aggregate;
    //! Current data.
    boost::shared_ptr<data_t> data;
};

AggregateModifier::AggregateModifier(size_t n) :
    m_state(new State())
{
    m_state->distribution = bind(constant_dist, n);
}

bool AggregateModifier::operator()(Input::input_p& input)
{
    if (! input) {
        if (m_state->aggregate) {
            input.swap(m_state->aggregate);
        }
        return true;
    }

    if (! m_state->aggregate) {
        m_state->aggregate = input;
        m_state->n = m_state->distribution();
        m_state->data = boost::make_shared<data_t>();
        m_state->data->sources.push_back(input->source);
        m_state->aggregate->source = m_state->data;
    }
    else {
        copy(
            input->connection.transactions.begin(),
            input->connection.transactions.end(),
            back_inserter(m_state->aggregate->connection.transactions)
        );
        m_state->data->sources.push_back(input->source);
    }

    if (
        m_state->n != 0 &&
        m_state->aggregate->connection.transactions.size() >= m_state->n
    )
    {
        input.swap(m_state->aggregate);
        m_state->aggregate.reset();
        m_state->data.reset();
        return true;
    }

    return false;
}

AggregateModifier AggregateModifier::uniform(
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
    AggregateModifier mod;
    mod.m_state->distribution = make_random_dist(
        boost::random::uniform_int_distribution<>(min, max)
    );
    return mod;
}

AggregateModifier AggregateModifier::binomial(unsigned int t, double p)
{
    if (t == 0 || p <= 0) {
        throw runtime_error("t and p must be positive.");
    }
    if (p > 1) {
        throw runtime_error("p must be less than or equal to 1.");
    }
    AggregateModifier mod;
    mod.m_state->distribution = make_random_dist(
        boost::random::binomial_distribution<>(t, p)
    );
    return mod;
}

AggregateModifier AggregateModifier::geometric(double p)
{
    if (p < 0 || p >= 1) {
        throw runtime_error("p must be in [0,1)");
    }
    AggregateModifier mod;
    mod.m_state->distribution = make_random_dist(
        boost::random::geometric_distribution<>(p)
    );
    return mod;
}

AggregateModifier AggregateModifier::poisson(double mean)
{
    AggregateModifier mod;
    mod.m_state->distribution = make_random_dist(
        boost::random::poisson_distribution<>(mean)
    );
    return mod;
}

} // CLIPP
} // IronBee
