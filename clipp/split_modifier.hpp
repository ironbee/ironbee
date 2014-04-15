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
 * @brief IronBee --- CLIPP Split Modifier
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__SPLIT_MODIFIER__
#define __IRONBEE__CLIPP__SPLIT_MODIFIER__

#include <clipp/input.hpp>

namespace IronBee {
namespace CLIPP {

/**
 * Splits data events into multiple data events.
 *
 * First event retains pre-delay and final event gets post-delay.
 * Intermediate events are not delayed.
 **/
class SplitDataModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] n Number of bytes per data event.
     **/
    explicit
    SplitDataModifier(size_t n = 1);

    //! Process an input.
    bool operator()(Input::input_p& input);

    /**
     * Split with sizes chosen uniformly from [@a min, @a max].
     *
     * @param[in] min Minimum size (except for last).
     * @param[in] max Maximum size.
     **/
    static
    SplitDataModifier uniform(unsigned int min, unsigned int max);

    /**
     * Split with sizes chosen from binomial distribution.
     *
     * n is chosen as the number of successful trials out of @a t where
     * success occurs with probability @a p.
     *
     * @param[in] t Number of trials.
     * @param[in] p Probability of success.
     **/
    static
    SplitDataModifier binomial(unsigned int t, double p);

    /**
     * Split with sizes chosen from geometric distribution.
     *
     * n is chosen as the number trials before a failure of probability
     * 1-@a p.
     *
     * @param[in] p Probability of success.
     **/
    static
    SplitDataModifier geometric(double p);

    /**
     * Split with sizes chosen from poisson distribution.
     *
     * n is chosen from a poisson distribution with mean @a mean.
     *
     * @param[in] mean Mean of distribution.
     **/
    static
    SplitDataModifier poisson(double mean);

public:
    struct State;
    boost::shared_ptr<State> m_state;
};

/**
 * Splits header events into multiple header events.
 *
 * First event retains pre-delay and final event gets post-delay.
 * Intermediate events are not delayed.
 **/
class SplitHeaderModifier
{
public:
    /**
     * Constructor.
     *
     * @param[in] n Number of header lines per header event.
     **/
    explicit
    SplitHeaderModifier(size_t n = 1);

    //! Process an input.
    bool operator()(Input::input_p& input);

    /**
     * Split with sizes chosen uniformly from [@a min, @a max].
     *
     * @param[in] min Minimum size (except for last).
     * @param[in] max Maximum size.
     **/
    static
    SplitHeaderModifier uniform(unsigned int min, unsigned int max);

    /**
     * Split with sizes chosen from binomial distribution.
     *
     * n is chosen as the number of successful trials out of @a t where
     * success occurs with probability @a p.
     *
     * @param[in] t Number of trials.
     * @param[in] p Probability of success.
     **/
    static
    SplitHeaderModifier binomial(unsigned int t, double p);

    /**
     * Split with sizes chosen from geometric distribution.
     *
     * n is chosen as the number trials before a failure of probability
     * 1-@a p.
     *
     * @param[in] p Probability of success.
     **/
    static
    SplitHeaderModifier geometric(double p);

    /**
     * Split with sizes chosen from poisson distribution.
     *
     * n is chosen from a poisson distribution with mean @a mean.
     *
     * @param[in] mean Mean of distribution.
     **/
    static
    SplitHeaderModifier poisson(double mean);

public:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
