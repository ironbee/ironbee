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
 * @brief IronBee --- CLIPP Random Support
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__RANDOM_SUPPORT__
#define __IRONBEE__CLIPP__RANDOM_SUPPORT__

#include <boost/function.hpp>
#include <boost/random.hpp>

namespace IronBee {
namespace CLIPP {

//! A random distribution.
typedef boost::function<size_t()> distribution_t;

//! The constant distribution.
inline
size_t constant_distribution(size_t n)
{
    return n;
}

//! Adapt a boost::random distribution to a distribution_t.
template <typename DistributionType>
class random_distribution
{
public:
    explicit
    random_distribution(DistributionType dist) :
        m_rng(clock()),
        m_distribution(dist)
    {
        // nop
    }

    size_t operator()()
    {
        size_t result = m_distribution(m_rng);
        if (result < 1) {
            result = 1;
        }
        return result;
    }

protected:
    boost::random::mt19937 m_rng;
    DistributionType       m_distribution;
};

//! Helper function to make @c random_distributions.
template <typename DistributionType>
random_distribution<DistributionType>
make_random_distribution(DistributionType dist)
{
    return random_distribution<DistributionType>(dist);
}

} // CLIPP
} // IronBee

#endif
