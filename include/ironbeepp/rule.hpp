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
 * @brief IronBee++ -- Rule
 *
 * Wrap ib_rule_t.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __IBPP__RULE__
#define __IBPP__RULE__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/exception.hpp>

// IronBee C Type
typedef struct ib_rule_t ib_rule_t;

namespace IronBee {

class Engine;
class Context;
class ConstVarExpand;

class ConstRule:
    public CommonSemantics<ConstRule>
{
public:
    //! C Type.
    typedef const ib_rule_t* ib_type;

    /**
     * Construct singular ConstRule.
     *
     * All behavior of a singular ConstRule is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstRule();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_rule_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Rule from ib_rule_t.
    explicit
    ConstRule(ib_type ib_rule);

    ///@}

    /**
     * @name Meta accessors.
     * Methods to access members of the rule->meta struct.
     **/
    ///@{
    ConstVarExpand msg() const;

    //! Return the full rule id.
    const char * rule_id() const;

    //! Return the full rule id.
    const char * full_rule_id() const;

    //! Return the chain id for this rule.
    const char * chain_rule_id() const;

    ///@}

private:
    ib_type m_ib;

};

/**
 * Rule; equivalent to a pointer to ib_rule_t.
 *
 * Context can be treated as ConstRule.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * @sa ConstRule
 * @sa ironbeepp
 * @sa ib_rule_t
 * @nosubgrouping
 **/
class Rule :
    public ConstRule
{
public:
    //! C Type.
    typedef ib_rule_t* ib_type;

    /**
     * Remove the constness of a ConstRule.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] context ConstRule to remove const from.
     * @returns Rule pointing to same underlying context as @a context.
     **/
    static Rule remove_const(ConstRule rule);

    /**
     * Construct singular Rule.
     *
     * All behavior of a singular Rule is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Rule();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_rule_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Context from ib_rule_t.
    explicit
    Rule(ib_type ib_rule);

    /**
     * Lookup a rule by its ID.
     *
     * @param[in] engine The engine.
     * @param[in] context The context.
     * @param[in] rule_id The rule id the rule is registered under.
     *
     * @throws IronBee::error on errors.
     *
     * @sa ib_rule_lookup()
     **/
    static Rule lookup(
        Engine      engine,
        Context     context,
        const char* rule_id
    );

    ///@}

private:
    ib_type m_ib;
};

} // IronBee

#endif
