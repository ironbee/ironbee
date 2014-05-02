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
 * *To access the root value in a predicate rule*
 *
 * - Add the `set_predicate_vars` action with an empty parameter.  This action
 *   will cause the variables `PREDICATE_VALUE` and `PREDICATE_VALUE_NAME` to
 *   be set for all subsequent actions in this rule.  These variables hold the
 *   root value and name of that value, respectively.
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
 * *To trace evaluation*
 *
 * - Use the `PredicateTrace` configuration directive.  Pass in a path to
 *   write the trace to or "" for stderr.  See `ptrace.pdf`.
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
#include <predicate/eval.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/parse.hpp>
#include <predicate/pre_eval_graph.hpp>
#include <predicate/standard.hpp>
#include <predicate/standard_template.hpp>
#include <predicate/transform_graph.hpp>
#include <predicate/tree_copy.hpp>
#include <predicate/validate_graph.hpp>

#include <ironbeepp/all.hpp>

#include <ironbee/rule_engine.h>

#include <boost/bind.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
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

/* Configuration */

//! Name of module.
const char* c_module_name = "predicate";

//! Action to mark a rule as a predicate rule.
const char* c_predicate_action = "predicate";

//! Action to set predicate related vars.
const char* c_set_predicate_vars_action = "set_predicate_vars";

//! Var holding the current value.
const char* c_var_value_name = "PREDICATE_VALUE_NAME";

//! Var holding the current value name
const char* c_var_value = "PREDICATE_VALUE";

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
class PerTransaction;

//! Shared pointer to PerTransaction.
typedef boost::shared_ptr<PerTransaction> per_transaction_p;


//! Iterator through list of values.
typedef IB::ConstList<P::Value>::const_iterator value_iterator;

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

    /**
     * Copy Constructor.
     *
     * PerContext copies represent child contexts and have independent data.
     * As such, this copy constructor only copies the delegate and
     * trace/debug report settings.
     **/
    PerContext(const PerContext& other);

    //! Constructor with delegate.
    explicit
    PerContext(Delegate& delegate);

    /**
     * Add a parsed rule.
     *
     * This function simply records the root and rule.  It will be integrated
     * into a DAG at the close of this context (and at the close of every
     * child context) via process_rules().
     *
     * @param[in] root Root of expression of rule.
     * @param[in] rule Rule object.
     **/
    void add_rule(P::node_p root, const ib_rule_t* rule);

    /**
     * Process rules into form for evaluation.
     *
     * This function builds the merge graph, runs it through its lifecycle
     * and then generated indexes of the result suitable for evaluation.
     *
     * @param[in] context The context this PerContext belongs to.  Needed to
     *            look for rules in ancestor contexts.
     **/
    void process_rules(IB::Context context);

    /**
     * Inject rules.
     *
     * @param[in] context   Context this PerContext belongs to.
     * @param[in] rule_exec Rule execution environment.
     * @param[in] rule_list List to write injected rules to.
     **/
    void inject(
        IB::ConstContext           context,
        const ib_rule_exec_t*      rule_exec,
        IB::List<const ib_rule_t*> rule_list
    ) const;

    //! Delegate accessor.
    Delegate* delegate() const {return m_delegate;}

    //! Set trace.
    void set_trace(const string& to);

    //! Set debug report.
    void set_debug_report(const string& to);

    //! Set validation report.
    void set_validation_report(const string& to);

    /**
     * Eval state index for rule.
     *
     * @param[in] rule Rule to find index of.
     * @return Index of @a rule.
     * @throw enoent on failure.
     **/
    size_t index_for_rule(const ib_rule_t* rule) const;

private:
    /**
     * Fetch per-transaction data, creating if needed.
     *
     * @param[in] tx Transaction to fetch for.
     * @returns Per transaction data.  Guaranteed to not be singular.
     **/
    per_transaction_p get_transaction_data(IB::Transaction tx) const;

    //! Root namer for use with dot2 routines.
    string root_namer(ib_rule_phase_num_t phase, size_t index) const;

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
     * @param[in]  node       Node involved with message.
     **/
    void report(
        size_t&           num_errors,
        bool              is_error,
        const string&     message,
        const P::node_cp& node
    );

    //! Add a root to m_merge_graph, allowing for known roots.
    size_t add_root(P::node_p root_node);

    /**
     * Run m_merge_graph through its lifecycle.
     *
     * @param[in] context Context PerContext belongs to.
     **/
    void run_graph_lifecycle(IB::Context context);

    //! Write a validation report and throw error if fails.
    void assert_valid() const;

    //! Origin information.  File and line number.
    typedef pair<string, size_t> origin_info_t;

    /**
     * Set origin information for a node.
     *
     * @param[in] origin_infos Origin infos to set for node.
     * @param[in] node Node to set origin for.
     **/
    void set_origin(
        const list<origin_info_t>& origin_infos,
        const P::node_cp&         node
    );

    //! List of rules.
    typedef list<const ib_rule_t*> rule_list_t;
    //! Map of node to rule list.
    typedef map<P::node_p, rule_list_t> rules_by_node_t;
    //! Map of root index to rule list.
    typedef map<size_t, rule_list_t> rules_by_index_t;
    //! Map of phase to map of node to rule list.
    typedef vector<rules_by_node_t> rules_by_phase_t;
    //! Root of expression and associated rule.
    typedef pair<P::node_p, const ib_rule_t*> rule_pair_t;
    //! List of rule pairs.
    typedef list<rule_pair_t> rule_pair_list_t;
    //! Map of rule to root index.
    typedef map<const ib_rule_t*, size_t> index_by_rule_t;
    //! Map of sexpr to origin info
    typedef map<string, list<origin_info_t> > origin_info_by_sexpr_t;

    //! List of rules to process.
    rule_pair_list_t m_rules;

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
    rules_by_phase_t m_rules_by_phase;

    /**
     * The MergeGraph.
     *
     * This is only kept around if PerContext::m_keep_data is set, e.g., for
     * tracing.
     **/
    boost::scoped_ptr<P::MergeGraph> m_merge_graph;

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

    //! Whether to output a debug report.
    bool m_write_debug_report;
    //! Where to write a debug report.
    string m_debug_report_to;

    //! Where to output a validation report.
    bool m_write_validation_report;
    //! Where to write a validation report.
    string m_validation_report_to;

    /**
     * All roots.
     *
     * Used to reset DAG via get_transaction_data().
     **/
    list<P::node_cp> m_roots;

    //! One larger than highest index of any node.
    size_t m_index_limit;

    /**
     * One larger than highest index of any root node.
     *
     * The roots should be an initial segment of the index space and thus
     * this number should be the same as the number of *distinct* root nodes.
     **/
    size_t m_root_limit;

    /**
     * Index for each rule.
     *
     * Generates from each contexts rules by PerContext::process_rules() at
     * context close.  Used by the vars action to turn a rule into an index to
     * access the graph evaluation state with.
     **/
    index_by_rule_t m_index_by_rule;

    /**
     * Index of origin info by sexpr.
     **/
    origin_info_by_sexpr_t m_origin_info_by_sexpr;

    /**
     * Keep data past configuration.
     *
     * If set to true, m_merge_graph, m_rules_by_index,
     * m_origin_info_by_sexpr, and m_rules are kept.  Otherwise, they are
     * cleared after being converted into m_roots, m_rules_by_phase, and
     * m_index_by_rule.
     **/
    bool m_keep_data;
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
     * @param[in] ib_ctx    Context rule is enabled in.
     * @returns
     * - IB_OK to claim rule.
     * - IB_DECLINED to decline claiming rule.
     **/
    ib_status_t ownership(
        const ib_engine_t*  ib_engine,
        const ib_rule_t*    rule,
        const ib_context_t* ib_ctx
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
     * @param[in] expr_c        Expression as C string.
     * @param[in] instance_data Action instance data.
     * @returns
     * - IB_OK on success.
     * - IB_EINVAL on parse error.
     * - Other if call factory throws an error.
     **/
    ib_status_t action_create(
        const char* expr_c,
        void*       instance_data
    );

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
     * See Template section of reference.txt.
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
     * Register trampoline data for cleanup on destruction.
     *
     * @sa make_c_trampoline()
     * @param[in] cdata Trampoline data.
     **/
    void register_trampoline_data(void* cdata);

    /**
     * Vars action creation.
     *
     * @param[in] ib    IronBee engine.
     * @param[in] param Parameter.
     * @return IB_EINVAL if param is not NULL.
     **/
    ib_status_t vars_action_create(ib_engine_t* ib, const char* param) const;

    /**
     * Vars action execution.
     *
     * @param[in] rule_exec Rule execution environment.
     * @return
     * - IB_OK on success.
     * - Other on failure.
     **/
    ib_status_t vars_action_execute(const ib_rule_exec_t* rule_exec) const;

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

    //! Var source for value name.
    IB::VarSource m_value_name_source;
    //! Var source for value value.
    IB::VarSource m_value_source;
};

/**
 * Per-transaction data.
 *
 * An instance is created at the beginning of each transaction and destroyed
 * when the transaction memory pool is destroyed.  It holds the graph
 * evaluation state and which root nodes (and thus which rules) have fired
 * this transaction.  The latter is needed to prevent phaseless rules from
 * firing each phase after they become true.
 **/
class PerTransaction
{
public:
    /**
     * Constructor.
     *
     * The graph should index nodes such with N roots, those roots have the
     * first N indices with non-root nodes having higher indices.
     *
     * @param[in] index_limit Upper limit on node indices.  Should be one more
     *                        than largest index.
     * @param[in] root_limit  Upper limit on root indices.  Should be one more
     *                        than largest index of a root.
     **/
    PerTransaction(size_t index_limit, size_t root_limit);

    // These methods are intentionally inlined as they are both
    // simple and performance critical.

    //! Graph eval state.
    P::GraphEvalState& graph_eval_state()
    {
        return m_graph_eval_state;
    }
    //! Graph eval state.
    const P::GraphEvalState& graph_eval_state() const
    {
        return m_graph_eval_state;
    }

    //! How many times has root @a i fired.
    size_t root_fire_count(size_t i) const
    {
        return m_root_fire_counts[i];
    }

    //! Set how many times root @a i has fired.
    void set_root_fire_count(size_t i, size_t count)
    {
        m_root_fire_counts[i] = count;
    }

    //! Access value iterator for a rule.
    value_iterator& valuelist_iterator_for_rule(
        const ib_rule_t* rule
    )
    {
        return m_rule_to_valuelist_iterator[rule];
    }

private:
    //! Map of rule to iterator into values for rule.
    typedef map<const ib_rule_t*, value_iterator>
        rule_to_valuelist_iterator_t;
    //! Rule to iterator into values for rule.
    rule_to_valuelist_iterator_t m_rule_to_valuelist_iterator;

    //! Graph evaluation state.
    P::GraphEvalState m_graph_eval_state;

    /**
     * How many times each root has fired.
     *
     * A root needs to fire once for each value in its ValueList.  As that
     * ValueList may grow, we need to keep track of how many times we've fired
     * it.
     **/
    std::vector<size_t> m_root_fire_counts;
};

// Implementation

PerContext::PerContext() :
    m_delegate(NULL),
    m_write_trace(false),
    m_write_debug_report(false),
    m_write_validation_report(false),
    m_index_limit(0),
    m_root_limit(0),
    m_keep_data(false)
{
    // nop
}

PerContext::PerContext(const PerContext& other) :
    m_delegate(other.m_delegate),
    m_write_trace(other.m_write_trace),
    m_trace_to(other.m_trace_to),
    m_write_debug_report(other.m_write_debug_report),
    m_debug_report_to(other.m_debug_report_to),
    m_write_validation_report(other.m_write_validation_report),
    m_validation_report_to(other.m_validation_report_to),
    m_index_limit(0),
    m_root_limit(0),
    m_keep_data(other.m_keep_data)
{
    // nop
}

PerContext::PerContext(Delegate& delegate) :
    m_delegate(&delegate),
    m_write_trace(false),
    m_write_debug_report(false),
    m_write_validation_report(false),
    m_index_limit(0),
    m_root_limit(0),
    m_keep_data(false)
{
    // nop
}

void PerContext::add_rule(P::node_p root, const ib_rule_t* rule)
{
    assert(root);
    assert(rule);

    m_origin_info_by_sexpr[root->to_s()].push_back(
        origin_info_t(rule->meta.config_file, rule->meta.config_line)
    );
    m_rules.push_back(rule_pair_t(root, rule));
}

size_t PerContext::add_root(P::node_p root)
{
    assert(m_merge_graph);
    assert(root);

    size_t index;
    P::node_p known_root = m_merge_graph->known(root);
    if (
        known_root &&
        m_merge_graph->is_root(known_root)
    ) {
        // Already added to graph.
        index = *m_merge_graph->root_indices(known_root).begin();
    }
    else {
        index = m_merge_graph->add_root(root);
    }

    return index;
}

void PerContext::set_origin(
    const list<origin_info_t>& origin_infos,
    const P::node_cp&          node
)
{
    BOOST_FOREACH(const origin_info_t& origin_info, origin_infos) {
        m_merge_graph->add_origin(
            node,
            (
                boost::format("%s:%d %s") %
                origin_info.first %
                origin_info.second %
                node->to_s()
            ).str()
        );
    }
}

void PerContext::run_graph_lifecycle(IB::Context context)
{
    ostream* debug_out;
    scoped_ptr<ostream> debug_out_resource;

    if (m_write_debug_report && ! m_debug_report_to.empty()) {
        debug_out_resource.reset(new ofstream(m_debug_report_to.c_str(), ios_base::app));
        debug_out = debug_out_resource.get();
        if (! *debug_out) {
            ib_log_error(delegate()->module().engine().ib(),
                "Could not open %s for writing.",
                m_debug_report_to.c_str()
            );
            BOOST_THROW_EXCEPTION(IB::einval());
        }
    }
    else {
        debug_out = &cerr;
    }

    if (m_write_validation_report) {
        assert_valid();
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

    // Set origin information.
    BOOST_FOREACH(
        const P::node_cp& root,
        make_pair(m_merge_graph->roots().first, m_merge_graph->roots().second)
    ) {
        const list<origin_info_t>& origin_infos =
            m_origin_info_by_sexpr[root->to_s()];
        P::bfs_down(
            root,
            boost::make_function_output_iterator(
                boost::bind(
                    &PerContext::set_origin,
                    this,
                    boost::ref(origin_infos),
                    _1
                )
            )
        );
    }


    size_t num_errors = 0;
    P::reporter_t reporter = bind(
        &PerContext::report,
        this, boost::ref(num_errors), _1, _2, _3
    );

    if (m_write_debug_report) {
        *debug_out << "Before Transform: " << endl;
        m_merge_graph->write_debug_report(*debug_out);
    }

    // Pre-Transform
    {
        num_errors = 0;
        P::validate_graph(P::VALIDATE_PRE, reporter, *m_merge_graph);
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
            needs_transform = P::transform_graph(
                reporter,
                *m_merge_graph,
                delegate()->call_factory(),
                context
            );
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
        m_merge_graph->write_debug_report(*debug_out);
    }

    // Post-Transform
    {
        num_errors = 0;
        P::validate_graph(P::VALIDATE_POST, reporter, *m_merge_graph);
        if (num_errors > 0) {
            BOOST_THROW_EXCEPTION(
                IB::einval() << IB::errinfo_what(
                    "Errors occurred during post-transform validation."
                    " See above."
                )
            );
        }
    }

    if (m_write_validation_report) {
        assert_valid();
    }
}

void PerContext::assert_valid() const
{
    bool is_okay = false;

    if (m_validation_report_to[0]) {
        ofstream out(m_validation_report_to.c_str(), ios_base::app);
        if (! out) {
            BOOST_THROW_EXCEPTION(
                IB::einval() << IB::errinfo_what(
                    "Could not open " + m_validation_report_to +
                    " for writing."
                )
            );
        }
        is_okay = m_merge_graph->write_validation_report(out);
    }
    else {
        is_okay = m_merge_graph->write_validation_report(cerr);
    }

    if (! is_okay) {
        BOOST_THROW_EXCEPTION(
            IB::einval() << IB::errinfo_what(
                "Internal validation failed."
            )
        );
    }
}

void PerContext::process_rules(IB::Context context)
{
    m_merge_graph.reset(new P::MergeGraph());

    // Add rules to m_merge_graph and record in m_rules_by_index.
    BOOST_FOREACH(const rule_pair_t& rule_pair, m_rules) {
        size_t index = add_root(rule_pair.first);
        m_rules_by_index[index].push_back(rule_pair.second);
    }

    // Add copies of parent rules.
    {
        for (
            IB::Context ctx = context.parent();
            ctx.parent(); // stop before main
            ctx = ctx.parent()
        ) {
            PerContext& ctx_per_context =
                m_delegate->module().configuration_data<PerContext>(ctx);
            BOOST_FOREACH(
                const rule_pair_t& rule_pair,
                ctx_per_context.m_rules
            ) {
                size_t index = add_root(
                    tree_copy(rule_pair.first, delegate()->call_factory())
                );
                m_rules_by_index[index].push_back(rule_pair.second);
            }
        }
    }

    // Graph Life Cycle
    run_graph_lifecycle(context);

    // Index node and calculate index limits.
    {
        P::bfs_down(
            m_merge_graph->roots().first, m_merge_graph->roots().second,
            P::make_indexer(m_index_limit)
        );
        m_root_limit = 0;
        BOOST_FOREACH(const P::node_p& root, m_merge_graph->roots()) {
            if (root->index() >= m_root_limit) {
                m_root_limit = root->index() + 1;
            }
        }
    }

    // Pre-Evaluate
    {
        size_t num_errors = 0;
        P::reporter_t reporter = bind(
            &PerContext::report,
            this, boost::ref(num_errors), _1, _2, _3
        );
        P::pre_eval_graph(
            reporter,
            *m_merge_graph,
            context
        );
        if (num_errors > 0) {
            BOOST_THROW_EXCEPTION(
                IB::einval() << IB::errinfo_what(
                    "Errors occurred during pre-evaluation."
                    " See above."
                )
            );
        }
    }

    // Copy roots off.
    copy(
        m_merge_graph->roots().first, m_merge_graph->roots().second,
        back_inserter(m_roots)
    );

    // Fill in m_rules_by_phase and m_index_by_rule.
    m_rules_by_phase = rules_by_phase_t(IB_RULE_PHASE_COUNT);
    BOOST_FOREACH(rules_by_index_t::const_reference v, m_rules_by_index) {
        BOOST_FOREACH(const ib_rule_t* rule, v.second) {
            ib_rule_phase_num_t phase = rule->meta.phase;
            P::node_p root = m_merge_graph->root(v.first);
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
            m_rules_by_phase[phase][root].push_back(rule);
            m_index_by_rule[rule] = root->index();
        }
    }

    if (! m_keep_data) {
        m_rules.clear();
        m_rules_by_index.clear();
        m_origin_info_by_sexpr.clear();
        m_merge_graph.reset();
    }
}

per_transaction_p PerContext::get_transaction_data(IB::Transaction tx) const
{
    per_transaction_p per_tx;
    try {
        per_tx = tx.get_module_data<per_transaction_p>(delegate()->module());
    }
    catch (IB::enoent) {
        // nop
    }

    if (! per_tx) {
        per_tx.reset(new PerTransaction(
            m_index_limit, m_root_limit
        ));
        P::bfs_down(
            m_roots.begin(), m_roots.end(),
            P::make_initializer(per_tx->graph_eval_state(), tx)
        );
        tx.set_module_data(delegate()->module(), per_tx);
    }

    return per_tx;
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
    per_transaction_p per_tx = get_transaction_data(tx);
    assert(per_tx);
    size_t num_considered = 0;
    size_t num_injected = 0;

    for (size_t i = 0; i < sizeof(phases)/sizeof(*phases); ++i) {
        ib_rule_phase_num_t phase = phases[i];
        BOOST_FOREACH(
            PerContext::rules_by_node_t::const_reference v,
            m_rules_by_phase[phase]
        ) {
            size_t index = v.first->index();

            // Only calculate if tracing as .size() might be O(n).
            size_t n = (m_write_trace ? v.second.size() : 0);
            num_considered += n;

            per_tx->graph_eval_state().eval(v.first, tx);

            size_t copies;
            P::Value value = per_tx->graph_eval_state().value(index);
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
                size_t fire_count = per_tx->root_fire_count(index);

                assert(fire_count <= result_count);
                copies = result_count - fire_count;
            }
            else {
                copies = result_count;
            }

            if (copies > 0) {
                BOOST_FOREACH(const ib_rule_t* rule, v.second) {
                    for (size_t i = 0; i < copies; ++i) {
                        rule_list.push_back(rule);
                    }
                }
                num_injected += n;
            }

            if (phase == IB_PHASE_NONE) {
                per_tx->set_root_fire_count(index, result_count);
            }
        }
    }

    if (m_write_trace) {
        assert(m_keep_data);
        assert(m_merge_graph);

        P::node_clist_t initial;
        for (size_t i = 0; i < sizeof(phases)/sizeof(*phases); ++i) {
            ib_rule_phase_num_t phase = phases[i];

            BOOST_FOREACH(
                PerContext::rules_by_node_t::const_reference v,
                m_rules_by_phase[phase]
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

            to_dot2_value(
                *trace_out,
                *m_merge_graph,
                per_tx->graph_eval_state(),
                initial,
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
    m_keep_data = true;
}

void PerContext::set_debug_report(const string& to)
{
    m_write_debug_report = true;
    m_debug_report_to = to;
}

void PerContext::set_validation_report(const string& to)
{
    m_write_validation_report = true;
    m_validation_report_to = to;
}

size_t PerContext::index_for_rule(const ib_rule_t* rule) const
{
    index_by_rule_t::const_iterator i = m_index_by_rule.find(rule);
    if (i == m_index_by_rule.end()) {
        BOOST_THROW_EXCEPTION(
            IB::enoent() << IB::errinfo_what(
                "Could not find index for rule."
            )
        );
    }

    return i->second;
}

string PerContext::root_namer(ib_rule_phase_num_t phase, size_t index) const
{
    return m_rules_by_index.find(index)->second.front()->meta.full_id;
}

namespace {

void per_context_report_log(
    ib_engine_t* ib,
    bool is_error,
    const std::string& message
)
{
    if (is_error) {
        ib_log_error(ib, "%s", message.c_str());
    }
    else {
        ib_log_warning(ib, "%s", message.c_str());
    }
}

void per_context_report_find_roots_helper(
    list<P::node_cp>& result,
    const P::MergeGraph& merge_graph,
    const P::node_cp& node
)
{
    if (merge_graph.is_root(node)) {
        result.push_back(node);
    }
}

void per_context_report_find_roots(
    list<P::node_cp>& result,
    const P::MergeGraph& merge_graph,
    const P::node_cp& node
)
{
    P::bfs_up(
        node,
        boost::make_function_output_iterator(
            boost::bind(
                per_context_report_find_roots_helper,
                boost::ref(result), boost::ref(merge_graph), _1
            )
        )
    );
}

}

void PerContext::report(
    size_t& num_errors,
    bool is_error,
    const string& message,
    const P::node_cp& node
)
{
    ib_engine_t* ib = m_delegate->module().engine().ib();

    if (is_error) {
        ++num_errors;
    }

    if (node) {
        per_context_report_log(ib, is_error, node->to_s() + " : " + message);
        BOOST_FOREACH(const string& origin, m_merge_graph->origins(node)) {
            per_context_report_log(ib, is_error, "  origin " + origin);
        }

        list<P::node_cp> roots;
        per_context_report_find_roots(roots, *m_merge_graph, node);

        BOOST_FOREACH(const P::node_cp& root, roots) {
            per_context_report_log(ib, is_error, "  root " + root->to_s());
            BOOST_FOREACH(
                const string& origin, m_merge_graph->origins(root)
            ) {
                per_context_report_log(
                    ib, is_error,
                    "    origin " + origin
                );
            }
        }
    }
    else {
        per_context_report_log(ib, is_error, message);
    }
}

PerTransaction::PerTransaction(size_t index_limit, size_t root_limit) :
    m_graph_eval_state(index_limit),
    m_root_fire_counts(root_limit, 0)
{
    // nop
}

Delegate::Delegate(IB::Module module) :
    IB::ModuleDelegate(module)
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
                const ib_rule_t*,
                const ib_context_t*
            )
        >(bind(&Delegate::ownership, this, _1, _2, _3));
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
                ib_mm_t,
                const char*,
                void*
            )
        >(bind(&Delegate::action_create, this, _3, _4));

    register_trampoline_data(action_create.second);

    IB::throw_if_error(
        ib_action_create_and_register(
            NULL,
            module.engine().ib(),
            c_predicate_action,
            action_create.first, action_create.second,
            NULL, NULL,
            NULL, NULL
        )
    );

    // 'set_predicate_vars' action
    pair<ib_action_create_fn_t, void*> vars_action_create =
        IB::make_c_trampoline<
            ib_status_t(
                ib_engine_t*,
                ib_mm_t,
                const char*,
                void*
            )
        >(bind(&Delegate::vars_action_create, this, _1, _3));

    register_trampoline_data(vars_action_create.second);
    pair<ib_action_execute_fn_t, void*> vars_action_execute =
        IB::make_c_trampoline<
            ib_status_t(
                const ib_rule_exec_t*,
                void*
            )
        >(bind(&Delegate::vars_action_execute, this, _1));

    register_trampoline_data(vars_action_execute.second);

    IB::throw_if_error(
        ib_action_create_and_register(
            NULL,
            module.engine().ib(),
            c_set_predicate_vars_action,
            vars_action_create.first,  vars_action_create.second,
            NULL, NULL,
            vars_action_execute.first, vars_action_execute.second
        )
    );

    // Hooks
    module.engine().register_hooks()
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

    // Vars
    m_value_name_source = IB::VarSource::register_(
        module.engine().var_config(),
        c_var_value_name
    );
    m_value_source = IB::VarSource::register_(
        module.engine().var_config(),
        c_var_value
    );
}

void Delegate::context_close(IB::Context context)
{
    assert(context);

    module().configuration_data<PerContext>(context).process_rules(context);
}

ib_status_t Delegate::action_create(
    const char* expr_c,
    void*       instance_data
)
{
    assert(expr_c);
    assert(instance_data);

    try {
        IB::MemoryManager mm = module().engine().main_memory_mm();
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

        *reinterpret_cast<void**>(instance_data) =
             IB::value_to_data(parse_tree, mm.ib());
    }
    catch (...) {
        return IB::convert_exception(module().engine());
    }

    return IB_OK;
}

ib_status_t Delegate::ownership(
    const ib_engine_t*  ib_engine,
    const ib_rule_t*    rule,
    const ib_context_t* ib_ctx
) const
{
    assert(ib_engine);
    assert(rule);
    assert(ib_ctx);

    IB::ConstEngine engine(ib_engine);
    IB::ConstContext context(ib_ctx);
    try {
        IB::ScopedMemoryPool pool;
        IB::MemoryManager mm = IB::MemoryPool(pool);
        IB::List<ib_action_inst_t*> actions =
            IB::List<ib_action_inst_t*>::create(mm);

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
            IB::data_to_value<P::node_p>(ib_action_inst_data(action));
        assert(parse_tree);

        // Need to keep our own list of roots as it is a subset of all roots
        // in the graph.
        // Const cast is because of current ambiguity as to whether modules
        // can access their own per-context data given a const context.
        PerContext& per_context
            = module().configuration_data<PerContext>(
                IB::Context::remove_const(context)
              );
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

void Delegate::assert_valid(
    IB::ConfigurationParser& cp,
    const char*              to
) const
{
    assert(cp);
    assert(to);

    PerContext& per_context =
        module().configuration_data<PerContext>(cp.current_context());
    per_context.set_validation_report(to);
}

void Delegate::debug_report(
    IB::ConfigurationParser& cp,
    const char*              to
)
{
    assert(cp);
    assert(to);

    PerContext& per_context =
        module().configuration_data<PerContext>(cp.current_context());
    per_context.set_debug_report(to);
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

    {
        string origin_prefix = (
            boost::format("%s:%d ") %
            cp.current_file() %cp.ib()->curr->line
        ).str();
        m_call_factory.add(
            name,
            P::Standard::define_template(arg_list, body_node, origin_prefix)
        );
    }
}

void Delegate::register_trampoline_data(void* cdata)
{
    assert(cdata);

    m_trampolines.push_back(
        boost::shared_ptr<void>(cdata, IB::delete_c_trampoline)
    );
}

ib_status_t Delegate::vars_action_create(
    ib_engine_t* ib,
    const char*  param
) const
{
    try {
        if (param && param[0]) {
            BOOST_THROW_EXCEPTION(
                IB::einval() << IB::errinfo_what(
                    string(c_set_predicate_vars_action) +
                    " must have empty parameter."
                )
            );
        }
    }
    catch (...) {
        return IB::convert_exception(ib);
    }
    return IB_OK;
}

ib_status_t Delegate::vars_action_execute(
    const ib_rule_exec_t* rule_exec
) const
{
    try {
        IB::Transaction tx(rule_exec->tx);
        ib_rule_t* rule = rule_exec->rule;

        per_transaction_p per_tx =
            tx.get_module_data<per_transaction_p>(module());
        PerContext& per_context = module().configuration_data<PerContext>(
            tx.context()
        );

        size_t index = per_context.index_for_rule(rule);
        P::Value value = per_tx->graph_eval_state().value(index);
        assert(value);
        P::Value subvalue;

        if (value.type() == P::Value::LIST) {
            IB::ConstList<P::Value> values = value.as_list();
            value_iterator& i = per_tx->valuelist_iterator_for_rule(rule);
            // XXX this isn't legal although it often works.
            if (i == value_iterator()) {
                i = values.begin();
            }
            else {
                ++i;
                assert(i != values.end());
            }
            subvalue = *i;
        }
        else {
            subvalue = value;
        }

        m_value_name_source.set(
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
        m_value_source.set(
            tx.var_store(),
            // Have our own copy, so safe to pass the non-const version
            // var requires to allow for future mutation of subvalue.
            IB::Field::remove_const(
                subvalue.dup(tx.memory_manager()).to_field()
            )
        );
    }
    catch (...) {
        return IB::convert_exception(rule_exec->ib);
    }
    return IB_OK;
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
