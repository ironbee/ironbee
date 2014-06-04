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
 * @brief IronBee --- Predicate-Constant Module
 *
 * This module adds the `constant` function to Predicate.  It must be loaded
 * after both `ibmod_predicate` and `ibmod_constant`.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */


#include <ironbee/predicate/call_factory.hpp>
#include <ironbee/predicate/functional.hpp>
#include <ironbee/predicate/ibmod_predicate_core.hpp>
#include <ironbee/predicate/reporter.hpp>

#include <modules/constant.h>

#include <ironbeepp/module_bootstrap.hpp>

namespace {

namespace P = IronBee::Predicate;

//! Predicate `constant` function.
class Constant :
    public P::Functional::Simple
{
public:
    //! Constructor.
    Constant() : P::Functional::Simple(1, 0) {}

    //! Prepare -- do everything.
    bool prepare(
        IronBee::MemoryManager,
        const P::Functional::value_vec_t& static_args,
        P::Environment environment,
        P::NodeReporter
    )
    {
        P::Value key = static_args.front();
        m_value = P::Value(
            IronBee::Constant::get(environment, key.as_string())
        );
        return true;
    }

protected:
    //! Return the value we found in prepare().
    P::Value eval_simple(
        IronBee::MemoryManager,
        const P::Functional::value_vec_t&
    ) const
    {
        return m_value;
    }

private:
    //! Constant value found by prepare().
    P::Value m_value;
};

//! Module load function.
void load(IronBee::Module module)
{
    IBModPredicateCore::call_factory(module.engine()).add(
        "constant", P::Functional::generate<Constant>
    );
}

}

IBPP_BOOTSTRAP_MODULE("predicate_constant", load);
