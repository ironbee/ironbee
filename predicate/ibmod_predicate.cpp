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
 *   initialization, call DelegateCallFactory(), passing in the IronBee
 *   Engine.  It will return a reference to the CallFactory used by this
 *   module.  Add your calls to that CallFactory.
 *
 * *To check internal validaty*
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

#include "bfs.hpp"
#include "dag.hpp"
#include "merge_graph.hpp"
#include "parse.hpp"
#include "pre_eval_graph.hpp"
#include "standard.hpp"
#include "transform_graph.hpp"
#include "validate_graph.hpp"

#include <ironbeepp/all.hpp>

#include <ironbee/rule_engine.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <algorithm>
#include <fstream>

using namespace std;

namespace IB = IronBee;
namespace P  = IB::Predicate;
using boost::bind;
using boost::scoped_ptr;
using boost::shared_ptr;

namespace {

class Delegate;

/* Configuration */

//! Name of module.
const char *c_module_name = "predicate";

//! Action to mark a rule as a predicate rule.
const char* c_predicate_action = "predicate";

//! Directive to assert internal validity.
const char* c_assert_valid_directive = "PredicateAssertValid";

//! Directive to write output a debug report.
const char* c_debug_report_directive = "PredicateDebugReport";

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
    PHASE_REQUEST_HEADER,
    PHASE_REQUEST_BODY,
    PHASE_RESPONSE_HEADER,
    PHASE_RESPONSE_BODY
};
//! Number of phases in c_phases.
const size_t c_num_phases = sizeof(c_phases) / sizeof(ib_rule_phase_num_t);

//! Per-context data.
class PerContext
{
private:
    //! List of rules.
    typedef list<const ib_rule_t*> rule_list_t;
    //! Map of root index to rule list.
    typedef map<size_t, rule_list_t> rules_by_index_t;
    //! Map of node to rule list.
    typedef map<P::node_p, rule_list_t> rules_by_node_t;
    //! Map of phase to map of node to rule list.
    typedef vector<rules_by_node_t> rules_by_phase_t;

public:
    // Public interface is what is used by Delegate.

    //! Constructor.
    PerContext();

    //! Constructor with delegate.
    explicit PerContext(Delegate& delegate);

    //! Const iterator through roots of @ref rules.
    typedef boost::transform_iterator<
        boost::function<P::node_p(rules_by_node_t::const_reference)>,
        rules_by_node_t::const_iterator
    > roots_iterator;

    /**
     * Iterate through all root nodes for a phase.
     *
     * @param[in] phase Which phase to iterate through.
     * @returns Pair of begin and end iterator for all roots in @ref rules for
     *          phase @a phase.
     **/
    pair<roots_iterator, roots_iterator> roots(
        ib_rule_phase_num_t phase
    ) const;

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
     * @param[in] rule_exec Rule execution environment.
     * @param[in] rule_list List to write injected rules to.
     **/
    void inject(
        const ib_rule_exec_t*      rule_exec,
        IB::List<const ib_rule_t*> rule_list
    );

    Delegate* delegate() const {return m_delegate;}
    void set_delegate(Delegate& delegate) {m_delegate = &delegate;}

private:
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
     * Last phase evaluated.
     *
     * This and @ref tx below are used to detect when the evaluation context
     * has changed and the DAG needs to be reset.
     **/
    ib_rule_phase_num_t m_phase;

    //! Last tx evaluated.  See @ref phase above.
    IB::ConstTransaction m_tx;

    /**
     * Module delegate.
     *
     * Global (vs per context) data is stored in the delegate().  This member
     * allows access to that data when only the context is available.
     **/
    Delegate* m_delegate;
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
     * Context close handler.
     *
     * For non-main context close, the context is recorded for later
     * processing.  Contexts can not be fully processed until the MergeGraph
     * has been fully formed and completes its life cycle.
     *
     * At close of main context, it is no longer possible to add expressions
     * to the MergeGraph.  At this point, it goes through the MergeGraph
     * life cycle (pre-transform, transform, post-transform, pre-evaluate)
     * and then every context is converted via PerContext::convert_rules().
     **/
    void context_close(IB::Context context);

    /**
     * Call factory accessor.
     *
     * @sa DelegateCallFactory
     * @returns Call factory.
     **/
    P::CallFactory& call_factory() { return m_call_factory; }

    //! MergeGraph accessor.  Only meaningful before end of configuration.
    P::MergeGraph& graph() const { return *m_graph; }

private:
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
     * Injection function: called at each phase in c_phases.
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
     * @param[in] pool     Memory pool to use.
     * @param[in] expr_c   Expression as C string.
     * @param[in] act_inst Action instance.
     * @returns
     * - IB_OK on success.
     * - IB_EINVAL on parse error.
     * - Other if call factory throws an error.
     **/
    ib_status_t action_create(
        ib_mpool_t*       pool,
        const char*       expr_c,
        ib_action_inst_t* act_inst
    );

    /**
     * Handle c_assert_valid_directive.
     *
     * See MergeGraph::write_validation_report().
     *
     * @param[in] cp Configuration parser.
     * @param[in] to Where to write report.  Empty string means cerr.
     * @throws IronBee::einval on failure.
     **/
    void assert_valid(IB::ConfigurationParser& cp, const char* to) const;

    /**
     * Handler c_debug_report_directive.
     *
     * See MergeGraph::write_debug_report().
     *
     * @param[in] cp Configuration parser.
     * @param[in] to Where to write report.  Empty string means cerr.
     **/
    void debug_report(IB::ConfigurationParser& cp, const char* to);

    /**
     * Handle reports for Predicate::Node life cycle routines.
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
    vector<shared_ptr<void> > m_trampolines;

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

    //! Whether to output a debug report.
    bool m_write_debug_report;
    //! Where to write a debug report.
    string m_debug_report_to;
};

}

// Defined in ibmod_predicate.hpp.
P::CallFactory& DelegateCallFactory(IB::Engine engine)
{
    IB::Module m = IB::Module::with_name(engine, c_module_name);
    PerContext& per_context = m.configuration_data<PerContext>(
        engine.main_context()
    );

    return per_context.delegate()->call_factory();
}

IBPP_BOOTSTRAP_MODULE_DELEGATE(c_module_name, Delegate);

// Implementation

PerContext::PerContext() :
    m_phase(PHASE_INVALID)
{
    // nop
}

PerContext::PerContext(Delegate& delegate) :
    m_phase(PHASE_INVALID),
    m_delegate(&delegate)
{
    // nop
}

pair<PerContext::roots_iterator, PerContext::roots_iterator>
PerContext::roots(
    ib_rule_phase_num_t phase
) const
{
    return make_pair(
        roots_iterator(
            m_rules[phase].begin(),
            bind(&rules_by_node_t::value_type::first, _1)
        ),
        roots_iterator(
            m_rules[phase].end(),
            bind(&rules_by_node_t::value_type::first, _1)
        )
    );
}

void PerContext::convert_rules()
{
    m_rules = rules_by_phase_t(IB_RULE_PHASE_COUNT);
    BOOST_FOREACH(rules_by_index_t::const_reference v, m_rules_by_index) {
        BOOST_FOREACH(const ib_rule_t* rule, v.second) {
            ib_rule_phase_num_t phase = rule->meta.phase;
            if (
                find(c_phases, c_phases+c_num_phases, phase) ==
                c_phases+c_num_phases
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
    size_t index = delegate()->graph().add_root(root);
    m_rules_by_index[index].push_back(rule);
}

void PerContext::inject(
    const ib_rule_exec_t*      rule_exec,
    IB::List<const ib_rule_t*> rule_list
)
{
    ib_rule_phase_num_t phase = rule_exec->phase;
    IB::Transaction tx(rule_exec->tx);

    if (m_phase != phase || tx != tx) {
        P::bfs_down(
            roots(phase).first,
            roots(phase).second,
            boost::make_function_output_iterator(
                bind(&P::Node::reset, _1)
            )
        );
    }

    m_phase = phase;
    m_tx    = tx;

    BOOST_FOREACH(
        PerContext::rules_by_node_t::const_reference v,
        m_rules[phase]
    ) {
        if (v.first->eval(tx)) {
            copy(
                v.second.begin(), v.second.end(),
                back_inserter(rule_list)
            );
        }
    }
}

Delegate::Delegate(IB::Module module) :
    IB::ModuleDelegate(module),
    m_graph(new P::MergeGraph()),
    m_write_debug_report(false)
{
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
    for (unsigned int i = 0; i < c_num_phases; ++i) {
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
                ib_context_t*,
                ib_mpool_t*,
                const char*,
                ib_action_inst_t*
            )
        >(bind(&Delegate::action_create, this, _3, _4, _5));

    register_trampoline_data(action_create.second);

    IB::throw_if_error(
        ib_action_register(
            module.engine().ib(),
            c_predicate_action,
            IB_ACT_FLAG_NONE,
            action_create.first, action_create.second,
            NULL, NULL,
            NULL, NULL
        )
    );

    // Introspection directives.
    module.engine().register_configuration_directives()
        .param1(
            c_assert_valid_directive,
            bind(&Delegate::assert_valid, this, _1, _3)
        )
        .param1(
            c_debug_report_directive,
            bind(&Delegate::debug_report, this, _1, _3)
        )
        ;
}

void Delegate::context_close(IB::Context context)
{
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
        P::reporter_t reporter =
            bind(
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
            P::validate_graph(P::PRE_TRANSFORM, reporter, graph());
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
            num_errors = 0;
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

        if (m_write_debug_report) {
            *debug_out << "After Transform: " << endl;
            graph().write_debug_report(*debug_out);
        }

        // Post-Transform
        {
            num_errors = 0;
            P::validate_graph(P::POST_TRANSFORM, reporter, graph());
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

        // Release graph.
        m_graph.reset();
    }
}

ib_status_t Delegate::action_create(
    ib_mpool_t*       pool,
    const char*       expr_c,
    ib_action_inst_t* act_inst
)
{
    try {
        string expr(expr_c);

        size_t i = 0;
        P::node_p parse_tree = P::parse_call(expr, i, m_call_factory);
        if (i != expr.length() - 1) {
            // Parse failed.
            size_t pre_length  = max(i+1, 10UL);
            size_t post_length = max(expr.length() - i, 10UL);
            ib_log_error(
                module().engine().ib(),
                "Predicate parser error: %s --ERROR-- %s",
                expr.substr(i-pre_length, pre_length).c_str(),
                expr.substr(i+1, post_length).c_str()
            );
            return IB_EINVAL;
        }

        act_inst->data = IB::value_to_data(parse_tree, pool);
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
                RULE_ACTION_TRUE,
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
    try {
        IB::List<const ib_rule_t*> rule_list(ib_rule_list);
        IB::ConstTransaction tx(rule_exec->tx);

        // The top, parent-less, context is the engine context and we do
        // not have per-context data for it.  So stop just before that.
        for (
            IB::Context ctx = tx.context();
            ctx.parent();
            ctx = ctx.parent()
        ) {
            PerContext& per_context =
                module().configuration_data<PerContext>(ctx);
            per_context.inject(rule_exec, rule_list);
        }
    }
    catch (const boost::exception& e) {
        assert(false);
    }
    catch (...) {
        return IB::convert_exception(module().engine());
    }

    return IB_OK;
}

void Delegate::assert_valid(
    IB::ConfigurationParser& cp,
    const char*              to
) const
{
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
    m_trampolines.push_back(
        shared_ptr<void>(cdata, IB::delete_c_trampoline)
    );
}

