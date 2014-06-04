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
 *
 * *To trace evaluation*
 *
 * - Use the `PredicateTrace` configuration directive.  First argument is a
 *   path of where to write the trace or `-` for stderr, subsequent arguments
 *   are rule ids to trace to.  With no arguments, defaults to all rules to
 *   stderr.  See `ptrace.pdf`.
 *
 * *To access the root value in a predicate rule*
 *
 * - Add the `set_predicate_vars` action with an empty parameter.  This action
 *   will cause the variables `PREDICATE_VALUE` and `PREDICATE_VALUE_NAME` to
 *   be set for all subsequent actions in this rule.  These variables hold the
 *   root value and name of that value, respectively.
 **/

#include <ironbee/predicate/dot2.hpp>
#include <ironbee/module/ibmod_predicate_core.hpp>

#include <ironbeepp/all.hpp>

#include <ironbee/rule_engine.h>

#include <boost/dynamic_bitset.hpp>
#include <boost/format.hpp>

#include <fstream>

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

//! Name of predicate vars action.
const char* c_vars_action = "set_predicate_vars";

//! Var holding the current value.
const char* c_var_value_name = "PREDICATE_VALUE_NAME";

//! Var holding the current value name
const char* c_var_value = "PREDICATE_VALUE";


//! Name of trace directive.
const char* c_trace_directive = "PredicateTrace";

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
typedef IB::ConstList<P::Value> value_list_t;

/**
 * Per context functionality.
 *
 * This class handles ownership and injection, maintaining the mapping of
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

    /**
     * Handle trace directive.
     *
     * @param[in] params Parameters of directive.
     **/
    void dir_trace(IB::List<const char*> params);

    //! Delegate accessor.
    const Delegate& delegate() const
    {
        // Intentionally inline.
        return m_delegate;
    }

    //! Delegate accessor.
    Delegate& delegate()
    {
        // Intentionally inline.
        return m_delegate;
    }

    //! Instance for set_predicate_vars action.
    void action_vars(const ib_rule_exec_t* rule_exec);

private:
    //! Root namer for dot2.
    list<string> root_namer(
        IB::ConstContext  context,
        const P::node_cp& node
    ) const;

    //! Fetch @ref PerTransaction associated with @a tx.
    PerTransaction& fetch_per_transaction(IB::Transaction tx) const;

    //! Delegate.
    Delegate& m_delegate;

    //! Rule and oracle.
    typedef pair<const ib_rule_t*, Oracle> rule_info_t;
    //! Type of m_all_rules.
    typedef multimap<size_t, rule_info_t> all_rules_t;

    //! Multimap of index to rules infos.
    all_rules_t m_all_rules;

    //! Vector of rule infos.
    typedef vector<rule_info_t> rules_t;
    //! Type of m_rules_by_phase.
    typedef vector<rules_t> rules_by_phase_t;

    //! Map of phase index to list of rules.
    rules_by_phase_t m_rules_by_phase;

    //! Type of m_oracle_by_rule_t;
    typedef map<const ib_rule_t*, Oracle> oracle_by_rule_t;
    //! Map of rules to oracle.
    oracle_by_rule_t m_oracle_by_rule;

    //! Whether to output a trace.
    bool m_write_trace;
    //! Where to write a trace.
    string m_trace_to;
    //! Which to trace.
    set<string> m_trace_which;
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

    //! Value information for set_predicate_vars.
    struct value_info_t
    {
        //! Default constructor.
        value_info_t();

        //! Has last_value been set.
        bool is_set;
        //! Last value.
        value_list_t::const_iterator last_value;
    };

    //! Type of value_infos.
    typedef map<const ib_rule_t*, value_info_t> value_infos_t;
    //! Map of rule to value info for that rule.
    value_infos_t value_infos;
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

    IB::VarSource value_name_source() const
    {
        // Intentionally inline.
        return m_value_name_source;
    }

    IB::VarSource value_source() const
    {
        // Intentionally inline.
        return m_value_source;
    }

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

    //! Handle trace.  Forwards to appropriate PerContext::dir_trace().
    void dir_trace(IB::ConfigurationParser& cp, IB::List<const char*> params);

    //! Generator for predicate action.
    IB::Action::action_instance_t generate_action_predicate() const;

    //! Instance for predicate action.
    void action_predicate() const;

    //! Generator for set_predicate_vars action.
    IB::Action::action_instance_t generate_action_vars(
        const char* params
    ) const;

    //! Instance for set_predicate_vars action.
    void action_vars(const ib_rule_exec_t* rule_exec) const;

    //! Var source for value name.
    IB::VarSource m_value_name_source;
    //! Var source for value value.
    IB::VarSource m_value_source;
};

} // Anonymous

IBPP_BOOTSTRAP_MODULE_DELEGATE(c_module_name, Delegate);

// Implementation
namespace {

PerContext::PerContext(Delegate& delegate) :
    m_delegate(delegate),
    m_rules_by_phase(c_num_phases),
    m_write_trace(false)
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
    Oracle oracle = acquire(
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

    rule_info_t rule_info(rule, oracle);
    m_all_rules.insert(make_pair(oracle.index(), rule_info));
    m_rules_by_phase[phase].push_back(rule_info);
    m_oracle_by_rule.insert(rule_info);
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
    size_t num_considered = 0;
    size_t num_injected = 0;

    for (size_t i = 0; i < sizeof(phases)/sizeof(*phases); ++i) {
        ib_rule_phase_num_t phase = phases[i];
        for (size_t j = 0; j < m_rules_by_phase[phase].size(); ++j) {
            const rule_info_t& rule_info = m_rules_by_phase[phase][j];

            result_t result = rule_info.second(tx);
            ++num_injected;


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
            if (phase == IB_PHASE_NONE) {
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
                ++num_injected;
            }

            if (phase == IB_PHASE_NONE) {
                per_tx.fire_counts[j] = result_count;
            }
        }
    }

    if (m_write_trace) {
        P::node_clist_t initial;
        for (size_t i = 0; i < sizeof(phases)/sizeof(*phases); ++i) {
            ib_rule_phase_num_t phase = phases[i];

            BOOST_FOREACH(
                PerContext::rules_t::const_reference rule_info,
                m_rules_by_phase[phase]
            ) {
                if (
                    m_trace_which.empty() ||
                    m_trace_which.count(rule_info.first->meta.id)
                ) {
                    initial.push_back(rule_info.second.node());
                }
            }
        }
        cout << "initial_size = " << initial.size() << endl;
        if (! initial.empty()) {
            ostream* trace_out;
            boost::scoped_ptr<ostream> trace_out_resource;

            if (! m_trace_to.empty() && m_trace_to != "-") {
                trace_out_resource.reset(
                    new ofstream(m_trace_to.c_str(), ofstream::app)
                );
                trace_out = trace_out_resource.get();
                if (! *trace_out) {
                    ib_log_error(delegate().module().engine().ib(),
                        "Could not open %s for writing.",
                        m_trace_to.c_str()
                    );
                    BOOST_THROW_EXCEPTION(IB::einval());
                }
            }
            else {
                trace_out = &cerr;
            }

            *trace_out << "PredicateTrace "
                       << ib_rule_phase_name(rule_exec->phase)
                       << " context=" << tx.context().full_name()
                       << " consider=" << num_considered
                       << " inject=" << num_injected
                       << endl;

            to_dot2_value(
                *trace_out,
                initial.begin(), initial.end(),
                IBModPredicateCore::graph_eval_state(tx),
                boost::bind(
                    &PerContext::root_namer,
                    this,
                    IB::ConstContext(rule_exec->tx->ctx),
                    _1
                )
            );

            *trace_out << "End PredicateTrace" << endl;
        }
    }
}

void PerContext::dir_trace(
    IB::List<const char*>    params
)
{
    const char* to = "-";
    set<string> rules;

    if (! params.empty()) {
        IB::List<const char*>::const_iterator i = params.begin();
        to = *i;
        ++i;
        while (i != params.end()) {
            rules.insert(*i);
            ++i;
        }
    }
    m_write_trace = true;
    m_trace_to = to;
    m_trace_which = rules;
}

void PerContext::action_vars(const ib_rule_exec_t* rule_exec)
{
    IB::Transaction tx(rule_exec->tx);
    ib_rule_t* rule = rule_exec->rule;

    PerTransaction& per_tx = fetch_per_transaction(tx);

    oracle_by_rule_t::const_iterator oracle_i =
        m_oracle_by_rule.find(rule);
    assert(oracle_i != m_oracle_by_rule.end());
    PerTransaction::value_info_t& value_info = per_tx.value_infos[rule];

    P::Value value = oracle_i->second(tx).first;
    assert(value);
    P::Value subvalue;

    if (value.type() == P::Value::LIST) {
        IB::ConstList<P::Value> values = value.as_list();
        if (! value_info.is_set) {
            value_info.last_value = values.begin();
            value_info.is_set = true;
        }
        else {
            ++value_info.last_value;
            assert(value_info.last_value != values.end());
        }
        subvalue = *value_info.last_value;
    }
    else {
        subvalue = value;
    }

    delegate().value_name_source().set(
        tx.var_store(),
        IB::Field::create_byte_string(
            tx.memory_manager(),
            subvalue.name(), subvalue.name_length(),
            IB::ByteString::create_alias(
                tx.memory_manager(),
                subvalue.name(), subvalue.name_length()
            )
        )
    );
    // Dup because setting a var renames the subvalue.
    delegate().value_source().set(
        tx.var_store(),
        // Have our own copy, so safe to pass the non-const version
        // var requires to allow for future mutation of subvalue.
        IB::Field::remove_const(
            subvalue.dup(tx.memory_manager()).to_field()
        )
    );
}

list<string> PerContext::root_namer(
    IB::ConstContext  context,
    const P::node_cp& node
) const
{
    list<string> result;

    try {
        BOOST_FOREACH(
            const Oracle& o,
            acquire_from_root(delegate().module().engine(), context, node)
        ) {
            BOOST_FOREACH(
                all_rules_t::const_reference v,
                m_all_rules.equal_range(o.index())
            ) {
                result.push_back(v.second.first->meta.full_id);
            }
        }
    }
    catch (IB::enoent) {
        // nop
    }

    return result;
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
    fire_counts(num_rules)
{
    // nop
}

PerTransaction::value_info_t::value_info_t() :
    is_set(false)
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
    IB::Action::create(
        engine.main_memory_mm(),
        c_vars_action,
        boost::bind(&Delegate::generate_action_vars, this, _3)
    ).register_with(engine);

    // Vars
    m_value_name_source = IB::VarSource::register_(
        module.engine().var_config(),
        c_var_value_name
    );
    m_value_source = IB::VarSource::register_(
        module.engine().var_config(),
        c_var_value
    );

    // Trace directive.
    engine.register_configuration_directives()
        .list(
            c_trace_directive,
            bind(&Delegate::dir_trace, this, _1, _3)
        )
        ;
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

IB::Action::action_instance_t Delegate::generate_action_vars(
    const char* params
) const
{
    if (params && params[0]) {
        BOOST_THROW_EXCEPTION(
            IB::einval() << IB::errinfo_what(
                string(c_vars_action) + " should have no parameter."
            )
        );
    }
    return boost::bind(&Delegate::action_vars, this, _1);
}

void Delegate::action_vars(const ib_rule_exec_t* rule_exec) const
{
    fetch_per_context(IB::Context(rule_exec->tx->ctx)).action_vars(rule_exec);
}

void Delegate::dir_trace(
    IB::ConfigurationParser& cp,
    IB::List<const char*>    params
)
{
    fetch_per_context(cp.current_context()).dir_trace(params);
}

PerContext& Delegate::fetch_per_context(IB::ConstContext context) const
{
    return module().configuration_data<PerContext>(context);
}

} // Anonymous
