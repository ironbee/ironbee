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
 * @brief IronBee --- CLIPP Aggregate Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__AGGREGATE_MODIFIER__
#define __IRONBEE__CLIPP__AGGREGATE_MODIFIER__

#include <clipp/input.hpp>

#include <boost/shared_ptr.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * Aggregates transactions from multiple connections into a single connection.
 *
 * Pre- and post- transaction events are @e only taken from the first
 * connection in each aggregate.
 *
 * Specifically, this modifier returns false until it has amassed more than
 * n connections, at which points it changes the input to a new aggregate
 * input and returns true.  If end-of-input occurs, produces an input with
 * all amassed inputs.
 **/
class AggregateModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] n If 0, aggregate all connections together, otherwise
     *              aggregate such that every connection has at least @a n
     *              transactions.
     **/
    explicit
    AggregateModifier(size_t n = 0);

    //! Process an input.
    bool operator()(Input::input_p& input);

    /**
     * Aggregate with sizes chosen uniformly from [@a min, @a max].
     *
     * @param[in] min Minimum size (except for last).
     * @param[in] max Maximum size.
     **/
    static
    AggregateModifier uniform(unsigned int min, unsigned int max);

    /**
     * Aggregate with sizes chosen from binomial distribution.
     *
     * n is chosen as the number of successful trials out of @a t where
     * success occurs with probability @a p.
     *
     * @param[in] t Number of trials.
     * @param[in] p Probability of success.
     **/
    static
    AggregateModifier binomial(unsigned int t, double p);

    /**
     * Aggregate with sizes chosen from geometric distribution.
     *
     * n is chosen as the number trials before a failure of probability
     * 1-@a p.
     *
     * @param[in] p Probability of success.
     **/
    static
    AggregateModifier geometric(double p);

    /**
     * Aggregate with sizes chosen from poisson distribution.
     *
     * n is chosen from a poisson distribution with mean @a mean.
     *
     * @param[in] mean Mean of distribution.
     **/
    static
    AggregateModifier poisson(double mean);

public:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
