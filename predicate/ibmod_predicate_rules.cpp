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
 * @brief IronBee --- Predicate Rules Module
 *
 * This module implements a rule injection system based on Predicate
 * expressions.  It defines a `predicate` action and takes ownership of any
 * rule containing that action.  The argument is interpreted as an
 * s-expression, and the rule is injected when the s-expression is true.
 * The module makes use of ibmod_predicate_core to acquire and query oracles
 * for each rule.
 *
 * When the value of an oracle is a list, the rule is injected once for each
 * occurrence in the list.
 *
 * Rules are allowed to be phaseless.  Phaseless rules are executed as soon
 * as their oracle becomes true.
 **/

#include <predicate/ibmod_predicate_core.hpp>

#include <ironbeepp/all.hpp>

#include <ironbee/rule_engine.h>

#include <boost/format.hpp>

using namespace std;
using boost::bind;

namespace IB = IronBee;
namespace P  = IB::Predicate;
using namespace IBModPredicateCore;

namespace {

// Configuration

//! Name of module.
const char* c_module_name = "predicate_rules";

//! Name of predicate action.
const char* c_predicate_action = "predicate";

/**
 * Phases supported by predicate.
 *
 * Any rule with the predicate action for a phase not in this list will cause
 * a configuration time error.
 *
 * @warning Adding a phase to this list not sufficient to make predicate work
 *          in that phase.  All predicate calls must function meaningfully in
 *          that phase as well.
 **/
const ib_rule_phase_num_t c_phases[] = {
    IB_PHASE_NONE, /* Special: Executed in every phase. */
    IB_PHASE_REQUEST_HEADER,
    IB_PHASE_REQUEST_HEADER_PROCESS,
    IB_PHASE_REQUEST,
    IB_PHASE_REQUEST_PROCESS,
    IB_PHASE_RESPONSE_HEADER,
    IB_PHASE_RESPONSE_HEADER_PROCESS,
    IB_PHASE_RESPONSE,
    IB_PHASE_RESPONSE_PROCESS,
    IB_PHASE_POSTPROCESS
};
//! Number of phases in c_phases.
const size_t c_num_phases = sizeof(c_phases) / sizeof(ib_rule_phase_num_t);

class Delegate;
struct PerTransaction;

//! List of values.
typedef IB::ConstList<IB::Field> value_list_t;

/**
 * Per context functionality.
 *
 * This class handles onwership and injection, maintaining the mapping of
 * rules to oracles.
 **/
class PerContext
{
public:
    //! Constructor.
    explicit
    PerContext(Delegate& delegate);

    /**
     * Determine ownership of @a rule.
     *
     * If @a rule contains the `predicate` action, will claim rule and
     * acquire an oracle for the sexpression that is the parameter of the
     * action.  If @a rule does not contain the action, will decline the rule
     * by throwing IronBee::declined.
     *
     * @param[in] rule Rule to determine ownership of.
     * @throw IronBee::declined if declining ownership of rule.
     **/
    void ownership(
        const ib_rule_t* rule
    );

    /**
     * Inject rules.
     *
     * Injects rules for the current phase whose sexpr has become true or
     * added a new subvalue to a list value.  Rules are injected for every
     * subvalue.
     *
     * @param[in] rule_exec Rule execution environment.
     * @param[in] rule_list List to append rules to.
     **/
    void injection(
        const ib_rule_exec_t*      rule_exec,
        IB::List<const ib_rule_t*> rule_list
    ) const;

    //! Delegate accessor.
    const Delegate& delegate() const;
    //! Delegate accessor.
    Delegate& delegate();

private:
    //! Fetch @ref PerTransaction associated with @a tx.
    PerTransaction& fetch_per_transaction(IB::Transaction tx) const;

    //! Delegate.
    Delegate& m_delegate;

    //! Rule and oracle.
    typedef pair<const ib_rule_t*, oracle_t> rule_info_t;
    //! Type of m_all_rules.
    typedef vector<rule_info_t> rules_t;

    //! List of rules with oracles.
    rules_t m_all_rules;

    //! Type of m_rules_by_phase.
    typedef vector<rules_t> rules_by_phase_t;

    //! Map of phase index to list of rules.
    rules_by_phase_t m_rules_by_phase;
};

/**
 * Per transaction data.
 *
 * Tracks how often a rule has fired and what subvalues it has been injected
 * for already.
 **/
struct PerTransaction
{
    /**
     * Constructor.
     *
     * @param[in] num_rules Number of rules.  Determines size of members.
     **/
    explicit
    PerTransaction(size_t num_rules);

    //! Type of fire_counts.
    typedef vector<size_t> fire_counts_t;

    /**
     * Map of rule to how often fired.
     *
     * Indexed by index in PerContext::m_rules_by_phase.
     **/
    fire_counts_t fire_counts;

    //! Type of value_iterators.
    typedef vector<value_list_t::const_iterator> value_iterators_t;

    /**
     * Map of rule to last value injected for.
     *
     * Indexed by index in PerContext::m_rules_by_phase.
     **/
    value_iterators_t value_iterators;
};

/**
 * Module delegate implementing the Predicate module.
 *
 * @sa ibmod_predicate.cpp
 * @sa IronBee::ModuleDelegate
 **/
class Delegate :
    public IB::ModuleDelegate
{
public:
    /**
     * Constructor.
     *
     * @param[in] module Module.
     **/
    explicit
    Delegate(IB::Module module);

private:
    //! Fetch @ref per_context_t associated with @a context.
    PerContext& fetch_per_context(IB::ConstContext context) const;

    //! Handle ownership.  Forwards to appropriate PerContext::ownership().
    void ownership(
        const ib_rule_t* rule,
        IB::ConstContext context
    ) const;

    //! Handle injection.  Forwards to appropriate PerContext::injection().
    void injection(
        const ib_rule_exec_t*      rule_exec,
        IB::List<const ib_rule_t*> rule_list
    ) const;

    //! Generator for predicate action.
    IB::Action::action_instance_t generate_action_predicate() const;

    //! Instance for predicate action.
    void action_predicate() const;
};

} // Anonymous

IBPP_BOOTSTRAP_MODULE_DELEGATE(c_module_name, Delegate);

// Implementation
namespace {

PerContext::PerContext(Delegate& delegate) :
    m_delegate(delegate),
    m_rules_by_phase(c_num_phases)
{
    // nop
}

void PerContext::ownership(
    const ib_rule_t* rule
)
{
    IB::ScopedMemoryPoolLite pool;
    IB::MemoryManager mm(pool);
    IB::List<ib_action_inst_t*> actions =
        IB::List<ib_action_inst_t*>::create(mm);

    IB::throw_if_error(
        ib_rule_search_action(
            m_delegate.module().engine().ib(),
            rule,
            IB_RULE_ACTION_TRUE,
            c_predicate_action,
            actions.ib(),
            NULL
        )
    );

    if (actions.empty()) {
        // Decline rule if no predicate action.
        BOOST_THROW_EXCEPTION(IB::declined());
    }

    if (actions.size() != 1) {
        // Multiple actions!
        ib_log_error(
            m_delegate.module().engine().ib(),
            "Multiple predicate actions: %s",
            rule->meta.full_id
        );
        BOOST_THROW_EXCEPTION(IB::einval());
    }

    const char* expr = IB::ActionInstance(actions.front()).parameters();
    const string origin = (boost::format("%s:%d %s") %
        rule->meta.config_file %
        rule->meta.config_line %
        expr
    ).str();
    oracle_t oracle = acquire(
        m_delegate.module().engine(),
        IB::Context(rule->ctx),
        expr,
        origin
    );

    ib_rule_phase_num_t phase = rule->meta.phase;
    if (
        find(c_phases, c_phases + c_num_phases, phase) ==
        c_phases + c_num_phases
    ) {
        BOOST_THROW_EXCEPTION(
            IB::einval() << IB::errinfo_what(
                "Rule " + string(rule->meta.full_id) +
                "is a predicate rule but has an unsupported "
                "phase: " +
                ib_rule_phase_name(rule->meta.phase)
            )
        );
    }

    m_all_rules.push_back(rule_info_t(rule, oracle));
    m_rules_by_phase[phase].push_back(rule_info_t(rule, oracle));
}

void PerContext::injection(
    const ib_rule_exec_t*      rule_exec,
    IB::List<const ib_rule_t*> rule_list
) const
{
    assert(rule_exec);
    assert(rule_list);

    const ib_rule_phase_num_t phases[2] = {
        IB_PHASE_NONE,
        rule_exec->phase
    };
    IB::Transaction tx(rule_exec->tx);
    assert(tx);
    PerTransaction& per_tx = fetch_per_transaction(tx);

    for (size_t i = 0; i < sizeof(phases)/sizeof(*phases); ++i) {
        ib_rule_phase_num_t phase = phases[i];
        for (size_t j = 0; j < m_rules_by_phase[phase].size(); ++j) {
            const rule_info_t& rule_info = m_rules_by_phase[phase][j];

            result_t result = rule_info.second(tx);

            size_t copies;
            P::Value value = result.first;
            if (! value) {
                continue;
            }

            size_t result_count = 1;
            if (value.type() == P::Value::LIST) {
                result_count = value.as_list().size();
            }

            // Check if fired enough already.
            if (phase == IB_PHASE_NONE)
            {
                size_t fire_count = per_tx.fire_counts[j];

                assert(fire_count <= result_count);
                copies = result_count - fire_count;
            }
            else {
                copies = result_count;
            }

            if (copies > 0) {
                for (size_t i = 0; i < copies; ++i) {
                    rule_list.push_back(rule_info.first);
                }
            }

            if (phase == IB_PHASE_NONE) {
                per_tx.fire_counts[j] = result_count;
            }
        }
    }
}

PerTransaction& PerContext::fetch_per_transaction(IB::Transaction tx) const
{
    typedef boost::shared_ptr<PerTransaction> per_transaction_p;

    per_transaction_p per_tx;
    try {
        per_tx = tx.get_module_data<per_transaction_p>(m_delegate.module());
    }
    catch (IB::enoent) {
        // nop
    }

    if (! per_tx) {
        per_tx.reset(new PerTransaction(m_all_rules.size()));
        tx.set_module_data(m_delegate.module(), per_tx);
    }

    return *per_tx;
}

PerTransaction::PerTransaction(size_t num_rules) :
    fire_counts(num_rules),
    value_iterators(num_rules)
{
    // nop
}

Delegate::Delegate(IB::Module module) :
    IB::ModuleDelegate(module)
{
    assert(module);

    IB::Engine engine = module.engine();

    // Configuration data.
    PerContext base(*this);

    module.set_configuration_data<PerContext>(base);

    // Rule ownership.
    engine.register_rule_ownership(
        c_module_name,
        boost::bind(&Delegate::ownership, this, _2, _3)
    );

    // Injection Functions.
    // Start at 1 to skip IB_PHASE_NONE.
    for (unsigned int i = 1; i < c_num_phases; ++i) {
        ib_rule_phase_num_t phase = c_phases[i];

        engine.register_rule_injection(
            c_module_name,
            phase,
            boost::bind(&Delegate::injection, this, _2, _3)
        );
    }

    // 'predicate' action.
    IB::Action::create(
        engine.main_memory_mm(),
        c_predicate_action,
        boost::bind(&Delegate::generate_action_predicate, this)
    ).register_with(engine);

    // 'set_predicate_vars' action
    // XXX
}

void Delegate::ownership(
    const ib_rule_t* rule,
    IB::ConstContext context
) const
{
    fetch_per_context(context).ownership(rule);
}

void Delegate::injection(
    const ib_rule_exec_t*      rule_exec,
    IB::List<const ib_rule_t*> rule_list
) const
{
    IB::ConstContext context(rule_exec->tx->ctx);
    fetch_per_context(context).injection(rule_exec, rule_list);
}

IB::Action::action_instance_t Delegate::generate_action_predicate() const
{
    return boost::bind(&Delegate::action_predicate, this);
}

void Delegate::action_predicate() const
{
    // Currently does nothing.  The action itself is searched for during
    // injection, but executing the action does nothing.
}

PerContext& Delegate::fetch_per_context(IB::ConstContext context) const
{
    return module().configuration_data<PerContext>(context);
}

} // Annonymous
