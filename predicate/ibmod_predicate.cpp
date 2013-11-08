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
 * @brief IronBee --- Predicate Module
 *
 * This module adds the Predicate rule system to IronBee.  See predicate.md
 * for detailed documentation on Predicate.  The remaining documentation
 * provides howto's for common tasks and discusses IronBee specific details,
 * but generally assumes familiarity with predicate.md.
 *
 * *To define a predicate rule*
 *
 * - Define a rule that always fires (e.g., with "Action") and add the
 *   `predicate` action to it.  The argument to `predicate` should be the
 *   S-expression which determines whether the rule should be injected.
 * - This module does understand configuration contexts and phases.  Common
 *   subexpression merging and transformations will be done across all
 *   contexts and phases, but predicate evaluation and rule injection will be
 *   limited by the current context and phase.
 *
 * *To add additional calls*
 *
 * - Third party calls must be provided as IronBee modules.  Simply load the
 *   module after this one.
 *
 * *To write an additional call*
 *
 * - Create an IronBee module.  Include "ibmod_predicate.hpp".  In
 *   initialization, call IBModPredicateCallFactory(), passing in the IronBee
 *   Engine.  It will return a reference to the CallFactory used by this
 *   module.  Add your calls to that CallFactory.
 *
 * *To check internal validity*
 *
 * - Use the `PredicateAssertValid` configuration directive.  Pass in a
 *   path to write the report to or "" for stderr.  The directive will error
 *   (probably aborting IronBee) if invalid.  See
 *   MergeGraph::write_validation_report().
 *
 * *To view the MergeGraph*
 *
 * - Use the `PredicateDebugReport` configuration directive.  Pass in a path
 *   to write the report to or "" for stderr. See
 *   MergeGraph::write_debug_report().
 *
 * Graph validation, transformation, and pre-evaluation all take place on the
 * close of the main context.  This means that syntactic errors will be
 * reported immediately, but semantic errors (such as invalid number of
 * arguments) will only be reported at the close of context.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/bfs.hpp>
#include <predicate/dag.hpp>
#include <predicate/dot2.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/parse.hpp>
#include <predicate/pre_eval_graph.hpp>
#include <predicate/standard.hpp>
#include <predicate/standard_template.hpp>
#include <predicate/transform_graph.hpp>
#include <predicate/validate_graph.hpp>

#include <ironbeepp/all.hpp>

#include <ironbee/rule_engine.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/shared_ptr.hpp>

#include <algorithm>
#include <fstream>

using namespace std;

namespace IB = IronBee;
namespace P  = IB::Predicate;
using boost::bind;
using boost::scoped_ptr;

namespace {

class Delegate;

/* Configuration */

//! Name of module.
const char* c_module_name = "predicate";

//! Action to mark a rule as a predicate rule.
const char* c_predicate_action = "predicate";

//! Directive to assert internal validity.
const char* c_assert_valid_directive = "PredicateAssertValid";

//! Directive to write output a debug report.
const char* c_debug_report_directive = "PredicateDebugReport";

//! Directive to define a template.
const char* c_define_directive = "PredicateDefine";

//! Directive to trace.
const char* c_trace_directive = "PredicateTrace";

/**
 * Phases supported by predicate.
 *
 * Any rule with the predicate action for a phase not in this list will cause
 * a configuration time error.
 *
 * @warning Adding a phase to this list not sufficient to make predicate work
 *          in that phase.  All predicate calls much function meaningfully in
 *          that phase as well.
 **/
const ib_rule_phase_num_t c_phases[] = {
    IB_PHASE_NONE, /* Special: Executed in every phase. */
    IB_PHASE_REQUEST_HEADER,
    IB_PHASE_REQUEST_BODY,
    IB_PHASE_RESPONSE_HEADER,
    IB_PHASE_RESPONSE_BODY
};
//! Number of phases in c_phases.
const size_t c_num_phases = sizeof(c_phases) / sizeof(ib_rule_phase_num_t);

/**
 * Per-context behavior and data.
 *
 * Used as the module configuration data.
 **/
class PerContext
{
public:
    // Public interface is what is used by Delegate.

    //! Constructor.
    PerContext();

    //! Constructor with delegate.
    explicit
    PerContext(Delegate& delegate);

    /**
     * Convert rules stored in @ref rules_by_index into @ref rules.
     *
     * Calling this method will result in an empty @ref rules_by_index and
     * will destroy any preexisting content in rules.
     **/
    void convert_rules();

    /**
     * Add a rule to @ref rules_by_index.
     *
     * Adds the root to the global MergeGraph and records the rule in
     * @ref rules_by_index accordingly.
     *
     * @param[in] root Root of expression tree.
     * @param[in] rule Rule to fire when @a root evaluates to true.
     **/
    void add_rule(P::node_p root, const ib_rule_t* rule);

    /**
     * Inject rules.
     *
     * @param[in] ib_context_t Context this PerContext belongs to.
     * @param[in] rule_exec    Rule execution environment.
     * @param[in] rule_list    List to write injected rules to.
     **/
    void inject(
        IB::ConstContext           context,
        const ib_rule_exec_t*      rule_exec,
        IB::List<const ib_rule_t*> rule_list
    ) const;

    //! Delegate accessor.
    Delegate* delegate() const {return m_delegate;}

    //! Delegate setter.
    void set_delegate(Delegate& delegate) {m_delegate = &delegate;}

    //! Set trace.
    void set_trace(const string& to);

private:
    //! Root namer for use with dot2 routines.
    string root_namer(ib_rule_phase_num_t phase, size_t index) const;

    //! List of rules.
    typedef list<const ib_rule_t*> rule_list_t;
    //! Map of root index to rule list.
    typedef map<size_t, rule_list_t> rules_by_index_t;
    //! Map of node to rule list.
    typedef map<P::node_p, rule_list_t> rules_by_node_t;
    //! Map of phase to map of node to rule list.
    typedef vector<rules_by_node_t> rules_by_phase_t;

    //! Const iterator through roots of @ref rules.  See roots().
    typedef boost::transform_iterator<
        boost::function<P::node_p(rules_by_node_t::const_reference)>,
        rules_by_node_t::const_iterator
    > roots_iterator;

    /**
     * Rules for each root by index.
     *
     * At configuration time, rules are accumulated here.  As the nodes of the
     * roots may change, they are indexed by root index rather than root node.
     * At the end of configuration, this datastructure will be converted to
     * the @ref rules member via convert_rules().
     **/
    rules_by_index_t m_rules_by_index;

    /**
     * Rules by phase and node.
     *
     * Generated from @ref m_rules_by_index by convert_rules() at the end of
     * configuration.  It can be iterated through directly to get roots and
     * rules or via roots() to iterate through just the roots.
     **/
    rules_by_phase_t m_rules;

    /**
     * Module delegate.
     *
     * Global (vs per context) data is stored in the delegate().  This member
     * allows access to that data when only the context is available.
     **/
    Delegate* m_delegate;

    //! Whether to output a trace.
    bool m_write_trace;
    //! Where to write a trace.
    string m_trace_to;
};

/**
 * Module delegate implementing the Predicate module.
 *
 * @sa ibmod_predicate.cpp
 * @sa IronBee::ModuleDelegate
 **/
class Delegate : public IB::ModuleDelegate
{
public:
    //! Constructor.
    explicit
    Delegate(IB::Module module);

    /**
     * Call factory accessor.
     *
     * @sa DelegateCallFactory
     * @returns Call factory.
     **/
    P::CallFactory& call_factory() { return m_call_factory; }

    /**
     * MergeGraph accessor.
     *
     * Only meaningful before end of configuration unless keep_data() is
     * called.
     **/
    P::MergeGraph& graph() const { return *m_graph; }

    /**
     * Keep extra data around.
     *
     * This instructs Delegate to keep the MergeGraph around past the end of
     * configuration.  This can be useful for introspective purposes and is
     * used by the trace code.
     **/
    void keep_data();

private:
    /**
     * Context close handler.
     *
     * For non-main context close, the context is recorded for later
     * processing.  Contexts cannot be fully processed until the MergeGraph
     * has been fully formed and completes its life cycle.
     *
     * At close of main context, it is no longer possible to add expressions
     * to the MergeGraph.  At this point, it goes through the MergeGraph
     * life cycle (pre-transform, transform, post-transform, pre-evaluate)
     * and then every context is converted via PerContext::convert_rules().
     **/
    void context_close(IB::Context context);

    /**
     * Ownership function: called on each rule at end of configuration.
     *
     * If rule has a predicate action, then will be claimed.  The action will
     * contain the parse tree as the instance data (see action_create()).
     * This parse tree will be added to the graph and associated with the
     * rule.
     *
     * @param[in] ib_engine Engine.
     * @param[in] rule      Rule to consider.
     * @returns
     * - IB_OK to claim rule.
     * - IB_DECLINED to decline claiming rule.
     **/
    ib_status_t ownership(
        const ib_engine_t* ib_engine,
        const ib_rule_t*   rule
    ) const;

    /**
     * Injection function: called at each phase in @ref c_phases.
     *
     * If first call for given transaction and phase, will reset portion of
     * DAG corresponding to the nodes to be evaluated in this phase.  Will
     * then evaluate all such nodes and inject the rules for those that are
     * true.
     *
     * @param[in] rule_exec    Rule execution environment.  Used to determine
     *                         current phase and transaction.
     * @param[in] ib_rule_list Rule list to write injected rules to.
     * @returns
     * - IB_OK on success.
     * - Other if any DAG node throws an error.
     **/
    ib_status_t injection(
        const ib_rule_exec_t* rule_exec,
        ib_list_t*            ib_rule_list
    ) const;

    /**
     * Predicate action creation.
     *
     * Parses expression into expression tree and stores result in
     * @c act_inst->data.  See ownership().
     *
     * @param[in] expr_c   Expression as C string.
     * @param[in] act_inst Action instance.
     * @returns
     * - IB_OK on success.
     * - IB_EINVAL on parse error.
     * - Other if call factory throws an error.
     **/
    ib_status_t action_create(
        const char*       expr_c,
        ib_action_inst_t* act_inst
    );

    /**
     * Transaction start handler.
     *
     * Resets DAG.
     *
     * @param[in] tx Current transaction.
     **/
    void request_started(IB::Transaction tx) const;

    /**
     * Handle @ref c_assert_valid_directive.
     *
     * See MergeGraph::write_validation_report().
     *
     * @param[in] cp Configuration parser.
     * @param[in] to Where to write report.  Empty string means cerr.
     * @throws IronBee::einval on failure.
     **/
    void assert_valid(IB::ConfigurationParser& cp, const char* to) const;

    /**
     * Handle @ref c_debug_report_directive.
     *
     * See MergeGraph::write_debug_report().
     *
     * @param[in] cp Configuration parser.
     * @param[in] to Where to write report.  Empty string means cerr.
     **/
    void debug_report(IB::ConfigurationParser& cp, const char* to);

    /**
     * Handle @ref c_define_directive.
     *
     * See Template section of reference.md.
     *
     * @param[in] cp     Configuration parser.
     * @param[in] params Parameters of directive.
     **/
    void define(IB::ConfigurationParser& cp, IB::List<const char*> params);

    /**
     * Handle @ref c_trace_directive.
     *
     * @param[in] cp     Configuration parser.
     * @param[in] params Parameters of directive.
     **/
    void trace(IB::ConfigurationParser& cp, const char* to);

    /**
     * Handle @ref reports for Predicate::Node life cycle routines.
     *
     * Warnings and errors are translated into log messages.  Errors also
     * increment @a num_errors.
     *
     * @sa Predicate::validate_graph()
     * @sa Predicate::pre_eval_graph()
     * @sa Predicate::transform_graph()
     *
     * @param[out] num_errors Counter to increment on error.
     * @param[in]  is_error   Error if true; warning if false.
     * @param[in]  message    Message.
     **/
    void report(
        size_t& num_errors,
        bool is_error,
        const string& message
    );

    /**
     * Register trampoline data for cleanup on destruction.
     *
     * @sa make_c_trampoline()
     * @param[in] cdata Trampoline data.
     **/
    void register_trampoline_data(void* cdata);

    /**
     * All trampolines used.  See make_c_trampoline()
     *
     * This vector is only written to.  Trampolines are added to it with
     * delete_c_trampoline() as the shared pointer deleter.  The result is
     * that all trampolines are deleted when this instance is destructed.
     **/
    vector<boost::shared_ptr<void> > m_trampolines;

    /**
     * Call factory to use for all node creation.
     *
     * @sa DelegateCallFactory()
     * @sa call_factory()
     **/
    P::CallFactory m_call_factory;

    /**
     * Every context.
     *
     * Contexts can not be meaningfully processed until the MergeGraph
     * (@ref m_graph) is fully constructed, i.e., the end of configuration.
     * So contexts are added to this member at context close and processed
     * on main context close.
     *
     * @sa context_close()
     **/
    list<IB::Context> m_contexts;

    /**
     * The MergeGraph.
     *
     * All predicate expressions use this MergeGraph, regardless of origin.
     * It will be reset, reclaiming memory, at the end of configuration.
     *
     * @sa context_close()
     **/
    scoped_ptr<P::MergeGraph> m_graph;

    /**
     * All roots.
     *
     * Used to reset DAG at request_started().
     *
     * @sa request_started()
     **/
    list<P::node_p> m_roots;

    //! Whether to output a debug report.
    bool m_write_debug_report;
    //! Where to write a debug report.
    string m_debug_report_to;
    //! Keep extra data past configuration?
    bool m_keep_data;
};

// Implementation

PerContext::PerContext() :
    m_delegate(NULL),
    m_write_trace(false)
{
    // nop
}

PerContext::PerContext(Delegate& delegate) :
    m_delegate(&delegate),
    m_write_trace(false)
{
    // nop
}

void PerContext::convert_rules()
{
    m_rules = rules_by_phase_t(IB_RULE_PHASE_COUNT);
    BOOST_FOREACH(rules_by_index_t::const_reference v, m_rules_by_index) {
        BOOST_FOREACH(const ib_rule_t* rule, v.second) {
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
            m_rules[phase][delegate()->graph().root(v.first)].push_back(rule);
        }
    }

    // Release memory.
    m_rules_by_index.clear();
}

void PerContext::add_rule(P::node_p root, const ib_rule_t* rule)
{
    assert(root);
    assert(rule);

    size_t index = delegate()->graph().add_root(root);
    m_rules_by_index[index].push_back(rule);
}

void PerContext::inject(
    IB::ConstContext           context,
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
    size_t num_considered = 0;
    size_t num_injected = 0;

    for (size_t i = 0; i < sizeof(phases)/sizeof(*phases); ++i) {
        ib_rule_phase_num_t phase = phases[i];

        BOOST_FOREACH(
            PerContext::rules_by_node_t::const_reference v,
            m_rules[phase]
        ) {
            // Only calculate if tracing as .size() might be O(n).
            size_t n = (m_write_trace ? v.second.size() : 0);
            num_considered += n;

            // Check user data.
            if (! v.first->get_user_data().empty()) {
                continue;
            }

            if (! v.first->eval(tx).empty()) {
                copy(
                    v.second.begin(), v.second.end(),
                    back_inserter(rule_list)
                );
                v.first->set_user_data(true);
                num_injected += n;
            }
        }
    }

    if (m_write_trace) {
        P::node_clist_t initial;
        for (size_t i = 0; i < sizeof(phases)/sizeof(*phases); ++i) {
            ib_rule_phase_num_t phase = phases[i];

            BOOST_FOREACH(
                PerContext::rules_by_node_t::const_reference v,
                m_rules[phase]
            ) {
                initial.push_back(v.first);
            }
        }
        if (! initial.empty()) {
            ostream* trace_out;
            scoped_ptr<ostream> trace_out_resource;

            if (m_write_trace && ! m_trace_to.empty()) {
                trace_out_resource.reset(
                    new ofstream(m_trace_to.c_str(), ofstream::app)
                );
                trace_out = trace_out_resource.get();
                if (! *trace_out) {
                    ib_log_error(delegate()->module().engine().ib(),
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
                       << " context=" << context.full_name()
                       << " consider=" << num_considered
                       << " inject=" << num_injected
                       << endl;

            to_dot2_value(*trace_out, delegate()->graph(), initial,
                boost::bind(
                    &PerContext::root_namer,
                    this,
                    rule_exec->phase,
                    _1
                )
            );

            *trace_out << "End PredicateTrace" << endl;
        }
    }
}

void PerContext::set_trace(const string& to)
{
    m_write_trace = true;
    m_trace_to = to;
    delegate()->keep_data();
}

string PerContext::root_namer(ib_rule_phase_num_t phase, size_t index) const
{
    const P::node_p& root = delegate()->graph().root(index);
    rules_by_node_t::const_iterator i = m_rules[phase].find(root);
    if (i == m_rules[phase].end()) {
        i = m_rules[IB_PHASE_NONE].find(root);
        if (i == m_rules[IB_PHASE_NONE].end()) {
            return "Out of phase";
        }
    }

    string result;
    bool first = true;
    BOOST_FOREACH(const ib_rule_t* rule, i->second) {
        if (! first) {
            result += "\\n";
        }
        first = false;
        result += rule->meta.full_id;
    }
    return result;
}

Delegate::Delegate(IB::Module module) :
    IB::ModuleDelegate(module),
    m_graph(new P::MergeGraph()),
    m_write_debug_report(false),
    m_keep_data(false)
{
    assert(module);

    // Call factory.
    P::Standard::load(m_call_factory);

    // Configuration data.
    PerContext base(*this);
    module.set_configuration_data<PerContext>(base);

    // Ownership Function.
    pair<ib_rule_ownership_fn_t, void*> owner =
        IB::make_c_trampoline<
            ib_status_t(
                const ib_engine_t*,
                const ib_rule_t*
            )
        >(bind(&Delegate::ownership, this, _1, _2));
    register_trampoline_data(owner.second);

    IB::throw_if_error(
        ib_rule_register_ownership_fn(
            module.engine().ib(),
            "predicate",
            owner.first, owner.second
        )
    );

    // Injection Functions.
    // Start at 1 to skip IB_PHASE_NONE.
    for (unsigned int i = 1; i < c_num_phases; ++i) {
        ib_rule_phase_num_t phase = c_phases[i];

        pair<ib_rule_injection_fn_t, void*> injection =
            IB::make_c_trampoline<
                ib_status_t(
                    const ib_engine_t*,
                    const ib_rule_exec_t*,
                    ib_list_t*
                )
            >(bind(&Delegate::injection, this, _2, _3));

        register_trampoline_data(injection.second);

        IB::throw_if_error(
            ib_rule_register_injection_fn(
                module.engine().ib(),
                c_module_name,
                phase,
                injection.first, injection.second
            )
        );
    }

    // 'predicate' action.
    pair<ib_action_create_fn_t, void*> action_create =
        IB::make_c_trampoline<
            ib_status_t(
                ib_engine_t*,
                const char*,
                ib_action_inst_t*
            )
        >(bind(&Delegate::action_create, this, _2, _3));

    register_trampoline_data(action_create.second);

    IB::throw_if_error(
        ib_action_register(
            module.engine().ib(),
            c_predicate_action,
            action_create.first, action_create.second,
            NULL, NULL,
            NULL, NULL
        )
    );

    // Hooks
    module.engine().register_hooks()
        .request_started(
            boost::bind(&Delegate::request_started, this, _2)
        )
        .context_close(
            boost::bind(&Delegate::context_close, this, _2)
        )
        ;

    // Directives.
    module.engine().register_configuration_directives()
        .param1(
            c_assert_valid_directive,
            bind(&Delegate::assert_valid, this, _1, _3)
        )
        .param1(
            c_debug_report_directive,
            bind(&Delegate::debug_report, this, _1, _3)
        )
        .list(
            c_define_directive,
            bind(&Delegate::define, this, _1, _3)
        )
        .param1(
            c_trace_directive,
            bind(&Delegate::trace, this, _1, _3)
        )
        ;
}

void Delegate::keep_data()
{
    m_keep_data = true;
}

void Delegate::context_close(IB::Context context)
{
    assert(context);

    // Register this context for processing at end of main context.
    m_contexts.push_back(context);

    if (context == context.engine().main_context()) {
        ostream* debug_out;
        scoped_ptr<ostream> debug_out_resource;

        if (m_write_debug_report && ! m_debug_report_to.empty()) {
            debug_out_resource.reset(new ofstream(m_debug_report_to.c_str()));
            debug_out = debug_out_resource.get();
            if (! *debug_out) {
                ib_log_error(module().engine().ib(),
                    "Could not open %s for writing.",
                    m_debug_report_to.c_str()
                );
                BOOST_THROW_EXCEPTION(IB::einval());
            }
        }
        else {
            debug_out = &cerr;
        }

        // Graph Lifecycle
        //
        // Below, we will...
        // 1. Pre-Transform: Validate graph before transformations.
        // 2. Transform: Transform graph until stable.
        // 3. Post-Transform: Validate graph after transformations.
        // 4. Pre-Evaluate: Provide the Engine to every node in the graph
        //    and instruct them to setup whatever data they need to evaluate.
        //
        // At each stage, any warnings and errors will be reported.  If errors
        // occur, the remaining stages are skipped and einval is thrown.
        // However, within each stage we gather as many errors and warnings
        // as possible.
        //
        // Once all has successfully completed, the MergeGraph will no longer
        // change.  So, on a context by context basis, we convert our root
        // index to rules map into a node to rules map more suitable for
        // evaluation.  Finally, once done with all contexts, the MergeGraph
        // itself is released.

        size_t num_errors = 0;
        P::reporter_t reporter = bind(
            &Delegate::report,
            this, boost::ref(num_errors), _1, _2
        );

        if (m_write_debug_report) {
            *debug_out << "Before Transform: " << endl;
            graph().write_debug_report(*debug_out);
        }

        // Pre-Transform
        {
            num_errors = 0;
            P::validate_graph(P::VALIDATE_PRE, reporter, graph());
            if (num_errors > 0) {
                BOOST_THROW_EXCEPTION(
                    IB::einval() << IB::errinfo_what(
                        "Errors occurred during pre-transform validation."
                        " See above."
                    )
                );
            }
        }

        // Transform
        {
            bool needs_transform = true;
            num_errors = 0;
            while (needs_transform) {
                needs_transform =
                    P::transform_graph(reporter, graph(), m_call_factory);
                if (num_errors > 0) {
                    BOOST_THROW_EXCEPTION(
                        IB::einval() << IB::errinfo_what(
                            "Errors occurred during DAG transformation."
                            " See above."
                        )
                    );
                }
            }
        }

        if (m_write_debug_report) {
            *debug_out << "After Transform: " << endl;
            graph().write_debug_report(*debug_out);
        }

        // Post-Transform
        {
            num_errors = 0;
            P::validate_graph(P::VALIDATE_POST, reporter, graph());
            if (num_errors > 0) {
                BOOST_THROW_EXCEPTION(
                    IB::einval() << IB::errinfo_what(
                        "Errors occurred during post-transform validation."
                        " See above."
                    )
                );
            }
        }

        // Pre-Evaluate
        {
            num_errors = 0;
            P::pre_eval_graph(reporter, graph(), context.engine());
            if (num_errors > 0) {
                BOOST_THROW_EXCEPTION(
                    IB::einval() << IB::errinfo_what(
                        "Errors occurred during pre-evaluation."
                        " See above."
                    )
                );
            }
        }

        // Per-Context Index Conversion
        BOOST_FOREACH(IB::Context other_context, m_contexts) {
            PerContext& per_context =
                module().configuration_data<PerContext>(other_context);
            per_context.convert_rules();
        }

        // Copy roots off.
        copy(
            graph().roots().first, graph().roots().second,
            back_inserter(m_roots)
        );

        // Release graph.
        if (! m_keep_data) {
            m_graph.reset();
        }
    }
}

ib_status_t Delegate::action_create(
    const char*       expr_c,
    ib_action_inst_t* act_inst
)
{
    assert(expr_c);
    assert(act_inst);

    try {
        IB::MemoryPool pool = module().engine().main_memory_pool();
        string expr(expr_c);

        size_t i = 0;
        P::node_p parse_tree = P::parse_call(expr, i, m_call_factory);
        if (i != expr.length() - 1) {
            // Parse failed.
            size_t pre_length  = max(i+1,               size_t(10));
            size_t post_length = max(expr.length() - i, size_t(10));
            ib_log_error(
                module().engine().ib(),
                "Predicate parser error: %s --ERROR-- %s",
                expr.substr(i-pre_length, pre_length).c_str(),
                expr.substr(i+1, post_length).c_str()
            );
            return IB_EINVAL;
        }

        act_inst->data = IB::value_to_data(parse_tree, pool.ib());
    }
    catch (...) {
        return IB::convert_exception(module().engine());
    }

    return IB_OK;
}

ib_status_t Delegate::ownership(
    const ib_engine_t* ib_engine,
    const ib_rule_t*   rule
) const
{
    assert(ib_engine);
    assert(rule);

    IB::ConstEngine engine(ib_engine);
    try {
        IB::ScopedMemoryPool pool;
        IB::List<ib_action_inst_t*> actions =
            IB::List<ib_action_inst_t*>::create(pool);
        IB::Context context(rule->ctx);

        IB::throw_if_error(
            ib_rule_search_action(
                engine.ib(),
                rule,
                IB_RULE_ACTION_TRUE,
                c_predicate_action,
                actions.ib(),
                NULL
            )
        );

        if (actions.empty()) {
            // Decline rule if no predicate action.
            return IB_DECLINED;
        }

        if (actions.size() != 1) {
            // Multiple actions!
            ib_log_error(
                engine.ib(),
                "Multiple predicate actions: %s",
                rule->meta.full_id
            );
            return IB_EINVAL;
        }

        ib_action_inst_t* action = actions.front();
        P::node_p parse_tree =
            IB::data_to_value<P::node_p>(action->data);
        assert(parse_tree);

        // Need to keep our own list of roots as it is a subset of all roots
        // in the graph.
        PerContext& per_context
            = module().configuration_data<PerContext>(context);
        per_context.add_rule(parse_tree, rule);
    }
    catch (...) {
        return IB::convert_exception(ib_engine);
    }

    return IB_OK;
}

ib_status_t Delegate::injection(
    const ib_rule_exec_t* rule_exec,
    ib_list_t*            ib_rule_list
) const
{
    assert(rule_exec);
    assert(ib_rule_list);

    try {
        IB::List<const ib_rule_t*> rule_list(ib_rule_list);
        IB::ConstTransaction tx(rule_exec->tx);
        PerContext& per_context =
            module().configuration_data<PerContext>(tx.context());
        per_context.inject(tx.context(), rule_exec, rule_list);
    }
    catch (...) {
        return IB::convert_exception(module().engine());
    }

    return IB_OK;
}

void Delegate::request_started(
    IB::Transaction tx
) const
{
    P::bfs_down(
        m_roots.begin(), m_roots.end(),
        boost::make_function_output_iterator(
            bind(&P::Node::reset, _1)
        )
    );
    BOOST_FOREACH(const P::node_p& root, m_roots) {
        root->set_user_data(boost::any());
    }
}

void Delegate::assert_valid(
    IB::ConfigurationParser& cp,
    const char*              to
) const
{
    assert(cp);
    assert(to);

    bool is_okay = false;

    if (to[0]) {
        ofstream out(to);
        if (! out) {
            ib_cfg_log_error(cp.ib(), "Could not open %s for writing.", to);
            BOOST_THROW_EXCEPTION(IB::einval());
        }
        is_okay = graph().write_validation_report(out);
    }
    else {
        is_okay = graph().write_validation_report(cerr);
    }

    if (! is_okay) {
        BOOST_THROW_EXCEPTION(
            IB::einval() << IB::errinfo_what(
                "Internal validation failed."
            )
        );
    }
}

void Delegate::debug_report(
    IB::ConfigurationParser& cp,
    const char*              to
)
{
    assert(cp);
    assert(to);

    if (m_write_debug_report) {
        ib_cfg_log_error(
            cp.ib(),
            "%s can only appear once.",
            c_debug_report_directive
        );
        BOOST_THROW_EXCEPTION(IB::einval());
    }
    m_write_debug_report = true;
    m_debug_report_to = to;
}

void Delegate::trace(
    IB::ConfigurationParser& cp,
    const char*              to
)
{
    assert(cp);
    assert(to);

    PerContext& per_context =
        module().configuration_data<PerContext>(cp.current_context());

    per_context.set_trace(to);
}

void Delegate::define(
    IB::ConfigurationParser& cp,
    IB::List<const char*>    params
)
{
    if (params.size() != 3) {
        ib_cfg_log_error(
            cp.ib(),
            "%s must have three arguments: name, args, and body.",
            c_define_directive
        );
        BOOST_THROW_EXCEPTION(IB::einval());
    }

    IB::List<const char*>::const_iterator i = params.begin();
    string name = *i;
    ++i;
    string args = *i;
    ++i;
    string body = *i;

    P::node_p body_node;
    try {
        size_t i = 0;
        if (body[0] == '(') {
            body_node = P::parse_call(body, i, m_call_factory);
        }
        else {
            body_node = P::parse_literal(body, i);
        }
    }
    catch (const IB::einval& e) {
        string message = "none reported";
        if (boost::get_error_info<IB::errinfo_what>(e)) {
            message = *boost::get_error_info<IB::errinfo_what>(e);
        }

        ib_cfg_log_error(
            cp.ib(),
            "%s: Error parsing body: %s",
            c_define_directive,
            message.c_str()
        );
        BOOST_THROW_EXCEPTION(IB::einval());
    }

    bool duplicate = true;
    try {
        m_call_factory(name);
    }
    catch (IB::enoent) {
        duplicate = false;
    }
    if (duplicate) {
        ib_cfg_log_error(
            cp.ib(),
            "%s: Already have function named %s",
            c_define_directive,
            name.c_str()
        );
    }

    P::Standard::template_arg_list_t arg_list;
    {
        size_t i = 0;
        while (i != string::npos) {
            size_t next_i = args.find_first_of(' ', i);
            arg_list.push_back(args.substr(i, next_i - i));
            i = args.find_first_not_of(' ', next_i);
        }
    }

    m_call_factory.add(
        name,
        P::Standard::define_template(arg_list, body_node)
    );
}

void Delegate::report(
    size_t& num_errors,
    bool is_error,
    const string& message
)
{
    ib_engine_t* ib = module().engine().ib();
    if (is_error) {
        ib_log_error(ib, "%s", message.c_str());
        ++num_errors;
    }
    else {
        ib_log_warning(ib, "%s", message.c_str());
    }
}

void Delegate::register_trampoline_data(void* cdata)
{
    assert(cdata);

    m_trampolines.push_back(
        boost::shared_ptr<void>(cdata, IB::delete_c_trampoline)
    );
}

}

// Outside anonymous namespace.

// Defined in ibmod_predicate.hpp.
P::CallFactory& IBModPredicateCallFactory(IB::Engine engine)
{
    IB::Module m = IB::Module::with_name(engine, c_module_name);
    PerContext& per_context = m.configuration_data<PerContext>(
        engine.main_context()
    );

    return per_context.delegate()->call_factory();
}

IBPP_BOOTSTRAP_MODULE_DELEGATE(c_module_name, Delegate);
