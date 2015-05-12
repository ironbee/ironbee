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
 * @brief IronAutomata --- Eudoxus Compiler Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/eudoxus_compiler.hpp>

#include <ironautomata/bits.h>
#include <ironautomata/eudoxus_automata.h>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <queue>
#include <set>

using namespace std;

namespace IronAutomata {
namespace EudoxusCompiler {

#define CPP_EUDOXUS_VERSION 10
#if CPP_EUDOXUS_VERSION != IA_EUDOXUS_VERSION
#error "Mismatch between compiler version and automata version."
#endif
const int EUDOXUS_VERSION = CPP_EUDOXUS_VERSION;
#undef CPP_EUDOXUS_VERSION

namespace {

/**
 * Compiler for given @a id_width.
 *
 * This helper class implements compilation for a specific id width.
 *
 * @tparam id_width Width of all Eudoxus identifiers.
 */
template <size_t id_width>
class Compiler
{
public:
    /**
     * Constructor.
     *
     * @param[in] result        Where to store results.
     * @param[in] configuration Compiler configuration.
     */
    Compiler(
        result_t& result,
         configuration_t configuration
    );

    /**
     * Compile automata.
     *
     * @param[in]  automata Automata to compile.
     */
    void compile(
        const Intermediate::Automata& automata
    );

private:
    //! Subengine traits.
    typedef Eudoxus::subengine_traits<id_width> traits_t;
    //! Eudoxus Identifier.
    typedef typename traits_t::id_t          e_id_t;
    //! Eudoxus Low Edge.
    typedef typename traits_t::low_edge_t    e_low_edge_t;
    //! Eudoxus Low Node.
    typedef typename traits_t::low_node_t    e_low_node_t;
    //! Eudoxus High Node
    typedef typename traits_t::high_node_t   e_high_node_t;
    //! Eudoxus PC Node
    typedef typename traits_t::pc_node_t     e_pc_node_t;
    //! Eudoxus Output List.
    typedef typename traits_t::output_list_t e_output_list_t;

    friend class BFSVisitor;

    //! True iff @a edge does not advance input.
    static
    bool is_nonadvancing(const Intermediate::Edge& edge)
    {
        return ! edge.advance();
    }

    /**
     * Answer questions about nodes.
     */
    struct NodeOracle
    {
        //! use_ali will be set if num_consecutive > c_ali_threshold.
        static const size_t c_ali_threshold = 32;

        //! Constructor.
        explicit
        NodeOracle(const Intermediate::node_p& node)
        {
            has_nonadvancing = (
                find_if(node->edges().begin(), node->edges().end(), is_nonadvancing)
            ) != node->edges().end();

            targets_by_input = node->build_targets_by_input();
            deterministic = true;
            out_degree = 0;
            num_consecutive = 0;

            Intermediate::node_p previous_target;
            for (int c = 0; c < 256; ++c) {
                const Intermediate::Node::target_info_list_t& targets
                     = targets_by_input[c];
                if (targets.size() > 1) {
                    deterministic = false;
                }
                if (targets.empty()) {
                    continue;
                }
                const Intermediate::node_p& target = targets.front().first;
                if (target != node->default_target()) {
                    ++out_degree;
                    if (previous_target && target == previous_target) {
                        ++num_consecutive;
                    }
                    previous_target = target;
                }
            }

            use_ali = (num_consecutive > c_ali_threshold);

            low_node_cost = 0;

            low_node_cost += sizeof(e_low_node_t);
            if (node->first_output()) {
                low_node_cost += sizeof(e_id_t);
            }
            if (! node->edges().empty()) {
                low_node_cost += sizeof(uint8_t);
                low_node_cost += sizeof(typename traits_t::low_edge_t) * out_degree;
            }
            if (node->default_target()) {
                low_node_cost += sizeof(e_id_t);
            }
            if (has_nonadvancing) {
                low_node_cost += (out_degree + 7) / 8;
            }

            high_node_cost = 0;

            high_node_cost += sizeof(e_high_node_t);
            if (node->first_output()) {
                high_node_cost += sizeof(e_id_t);
            }
            if (node->default_target()) {
                high_node_cost += sizeof(e_id_t);
            }
            if (has_nonadvancing) {
                high_node_cost += sizeof(ia_bitmap256_t);
            }
            if (out_degree < 256) {
                high_node_cost += sizeof(ia_bitmap256_t);
            }
            if (use_ali) {
                high_node_cost += sizeof(ia_bitmap256_t);
            }
            if (use_ali) {
                high_node_cost += sizeof(e_id_t) * (out_degree - num_consecutive);
            }
            else {
                high_node_cost += sizeof(e_id_t) * out_degree;
            }
        }

        //! True if there are non-advancing edges (not including default).
        bool has_nonadvancing;
        //! True if every input has at most 1 target.
        bool deterministic;
        //! Number of inputs with non-default targets.
        size_t out_degree;
        /**
         * Number of inputs with same target as last input.
         *
         * Does not include default targets.
         */
        size_t num_consecutive;

        //! True if a high degree node should use an ALI.
        bool use_ali;

        //! Cost in bytes of representing with a low node.
        size_t low_node_cost;
        //! Cost in bytes of representing with a high node.
        size_t high_node_cost;

        //! Targets by input map.
        Intermediate::Node::targets_by_input_t targets_by_input;
    };

    //! Set of nodes.
    typedef set<Intermediate::node_p> node_set_t;
    //! Map of nodes to set of nodes: it's parents.
    typedef map<Intermediate::node_p, node_set_t> parent_map_t;

    //! Compile @a node to @a end_of_path into a PC node.
    void pc_node(
        const Intermediate::node_p& node,
        const Intermediate::node_p& end_of_path,
        size_t                      path_length
    )
    {
        size_t old_size = m_assembler.size();

        {
            e_pc_node_t* header =
                m_assembler.append_object(e_pc_node_t());

            header->header = IA_EUDOXUS_PC;
            if (node->first_output()) {
                header->header = ia_setbit8(header->header, 0 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (node->default_target()) {
                header->header = ia_setbit8(header->header, 1 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (node->advance_on_default()) {
                header->header = ia_setbit8(header->header, 2 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (end_of_path->edges().front().advance()) {
                header->header = ia_setbit8(header->header, 3 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (path_length >= 4) {
                header->header = ia_setbit8(header->header, 4 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (path_length > 4 || path_length == 3) {
                header->header = ia_setbit8(header->header, 5 + IA_EUDOXUS_TYPE_WIDTH);
            }

            register_node_ref(
                m_assembler.index(&(header->final_target)),
                end_of_path
            );
        }

        if (node->first_output()) {
            append_output_ref(node->first_output());
            m_outputs.insert(node->first_output());
        }

        if (node->default_target()) {
            append_node_ref(node->default_target());
        }

        assert(path_length >= 2);
        assert(path_length <= 255);

        if (path_length > 4) {
            m_assembler.append_object(uint8_t(path_length));
        }

        for (
            Intermediate::node_p cur = node;
            cur != end_of_path;
            cur = has_unique_child(cur)
        ) {
            assert(cur->edges().size() == 1);
            assert(cur->edges().front().size() == 1);
            m_assembler.append_object(uint8_t(
               *cur->edges().front().begin()
            ));
        }

        ++m_result.pc_nodes;
        m_result.pc_nodes_bytes += m_assembler.size() - old_size;
    }

    //! Compile node into a demux (high or low) node.
    void demux_node(const Intermediate::node_p& node)
    {
        NodeOracle oracle(node);

        if (! oracle.deterministic) {
            throw runtime_error(
                "Non-deterministic automata unsupported."
            );
        }

        size_t old_size = m_assembler.size();
        size_t* bytes_counter = NULL;
        size_t* nodes_counter = NULL;
        size_t cost_prediction = 0;

        if (
            oracle.high_node_cost * m_configuration.high_node_weight
             > oracle.low_node_cost
        ) {
            cost_prediction = oracle.low_node_cost;
            bytes_counter = &m_result.low_nodes_bytes;
            nodes_counter = &m_result.low_nodes;
            low_node(*node, oracle);
        }
        else {
            cost_prediction = oracle.high_node_cost;
            bytes_counter = &m_result.high_nodes_bytes;
            nodes_counter = &m_result.high_nodes;
            high_node(*node, oracle);
        }
        size_t bytes_added = m_assembler.size() - old_size;

        if (cost_prediction != bytes_added) {
            throw logic_error(
                "Insanity: Incorrect cost prediction."
                "  Please report as bug."
            );
        }
        ++(*nodes_counter);
        (*bytes_counter) += bytes_added;
    }

    /**
     * Compile @a node as a low_node.
     *
     * Appends a low node to the buffer representing @a node.
     *
     * @param[in] node Intermediate node to compile.
     */
    void low_node(
        const Intermediate::Node& node,
        const NodeOracle& oracle
    )
    {
        {
            e_low_node_t* header =
                m_assembler.append_object(e_low_node_t());

            header->header = IA_EUDOXUS_LOW;
            if (node.first_output()) {
                header->header = ia_setbit8(header->header, 0 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (oracle.has_nonadvancing) {
                header->header = ia_setbit8(header->header, 1 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (node.default_target()) {
                header->header = ia_setbit8(header->header, 2 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (node.advance_on_default()) {
                header->header = ia_setbit8(header->header, 3 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (oracle.out_degree > 0) {
                header->header = ia_setbit8(header->header, 4 + IA_EUDOXUS_TYPE_WIDTH);
            }
        }

        if (node.first_output()) {
            append_output_ref(node.first_output());
            m_outputs.insert(node.first_output());
        }

        if (oracle.out_degree > 0) {
            m_assembler.append_object(
                uint8_t(oracle.out_degree)
            );
        }

        if (node.default_target()) {
            append_node_ref(node.default_target());
        }

        size_t advance_index = 0;
        if (oracle.has_nonadvancing) {
            uint8_t* advance =
                m_assembler.template append_array<uint8_t>(
                    (oracle.out_degree + 7) / 8
                );
            advance_index = m_assembler.index(advance);
        }

        size_t edge_i = 0;
        BOOST_FOREACH(const Intermediate::Edge& edge, node.edges()) {
            if (edge.epsilon()) {
                throw runtime_error(
                    "Epsilon edges currently unsupported."
                );
            }
            BOOST_FOREACH(uint8_t value, edge) {
                if (oracle.has_nonadvancing && edge.advance()) {
                    ia_setbitv(
                        m_assembler.template ptr<uint8_t>(
                            advance_index
                        ),
                        edge_i
                    );
                }
                ++edge_i;

                e_low_edge_t* e_edge =
                    m_assembler.append_object(e_low_edge_t());
                e_edge->c = value;
                register_node_ref(
                    m_assembler.index(&(e_edge->next_node)),
                    edge.target()
                );
            }
        }
    }

    /**
     * Compile @a node as a high_node.
     *
     * Appends a high node to the buffer representing @a node.
     *
     * @param[in] node Intermediate node to compile.
     */
    void high_node(
        const Intermediate::Node& node,
        const NodeOracle& oracle
    )
    {
        {
            e_high_node_t* header =
                m_assembler.append_object(e_high_node_t());

            header->header = IA_EUDOXUS_HIGH;
            if (node.first_output()) {
                header->header = ia_setbit8(header->header, 0 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (oracle.has_nonadvancing) {
                header->header = ia_setbit8(header->header, 1 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (node.default_target()) {
                header->header = ia_setbit8(header->header, 2 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (node.advance_on_default()) {
                header->header = ia_setbit8(header->header, 3 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (oracle.out_degree < 256) {
                header->header = ia_setbit8(header->header, 4 + IA_EUDOXUS_TYPE_WIDTH);
            }
            if (oracle.use_ali) {
                header->header = ia_setbit8(header->header, 5 + IA_EUDOXUS_TYPE_WIDTH);
            }
        }

        if (node.first_output()) {
            append_output_ref(node.first_output());
            m_outputs.insert(node.first_output());
        }

        if (node.default_target()) {
            append_node_ref(node.default_target());
        }

        if (oracle.has_nonadvancing) {
            ia_bitmap256_t& advance_bm =
                *m_assembler.append_object(ia_bitmap256_t());
            for (int c = 0; c < 256; ++c) {
                if (
                    ! oracle.targets_by_input[c].empty() &&
                    oracle.targets_by_input[c].front().second
                ) {
                    ia_setbitv64(advance_bm.bits, c);
                }
            }
        }

        if (oracle.out_degree < 256) {
            ia_bitmap256_t& target_bm =
                *m_assembler.append_object(ia_bitmap256_t());
            for (int c = 0; c < 256; ++c) {
                if (
                    ! oracle.targets_by_input[c].empty()
                    && oracle.targets_by_input[c].front().first !=
                       node.default_target()
                ) {
                    ia_setbitv64(target_bm.bits, c);
                }
            }
        }

        if (oracle.use_ali) {
            Intermediate::node_p previous_target;
            ia_bitmap256_t& ali_bm =
                *m_assembler.append_object(ia_bitmap256_t());
            for (int c = 0; c < 256; ++c) {
                if (! oracle.targets_by_input[c].empty()) {
                    const Intermediate::node_p& target =
                        oracle.targets_by_input[c].front().first;
                    if (target == node.default_target()) {
                        continue;
                    }
                    if (
                        previous_target &&
                        target != previous_target
                    ) {
                        ia_setbitv64(ali_bm.bits, c);
                    }
                    previous_target = target;
                }
            }

            // Using second loop as ali_bm might be moved by append_node_ref.
            previous_target.reset();
            for (int c = 0; c < 256; ++c) {
                if (! oracle.targets_by_input[c].empty()) {
                    const Intermediate::node_p& target =
                        oracle.targets_by_input[c].front().first;
                    if (target == node.default_target()) {
                        continue;
                    }
                    if (
                        ! previous_target ||
                        target != previous_target
                    ) {
                        append_node_ref(target);
                    }
                    previous_target = target;
                }
            }
        }
        else {
            for (int c = 0; c < 256; ++c) {
                if (! oracle.targets_by_input[c].empty()) {
                    const Intermediate::node_p& target =
                        oracle.targets_by_input[c].front().first;
                    if (target != node.default_target()) {
                        append_node_ref(target);
                    }
                }
            }
        }
    }

    /**
     * Go back over buffer and fill in the identifiers.
     *
     * This method combines @a id_map and @a object_map to fill in the
     * identifiers listed in @a id_map with the proper indices of their
     * referants as specified by @a object_map.
     *
     * It is called twice, once for output identifiers and once for input
     * identifiers.  These can not be compiled as the intermediate format does
     * not guarantee separate ID spaces.
     *
     * @tparam IDMapType     Type of @a id_map.
     * @tparam ObjectMapType Type of @a object_map.
     * @param[in] id_map     Map of identifier index to object.
     * @param[in] object_map Map of object to object index.
     */
    template <typename IDMapType, typename ObjectMapType>
    void fill_in_ids(
        const IDMapType&     id_map,
        const ObjectMapType& object_map
    )
    {
        BOOST_FOREACH(
            const typename IDMapType::value_type x, id_map
        )
        {
            if (x.second) {
                typename ObjectMapType::const_iterator object_map_entry =
                    object_map.find(x.second);
                if (object_map_entry == object_map.end()) {
                    throw logic_error("Request ID fill but no such object.");
                }
                e_id_t* e_id = m_assembler.ptr<e_id_t>(x.first);
                *e_id = object_map_entry->second;
            }
        }
    }

    //! Record an output reference to @a output at location @a id_index.
    void register_output_ref(
        size_t                        id_index,
        const Intermediate::output_p& output
    )
    {
        m_output_id_map[id_index] = output;
    }

    //! Record a node reference to @a node at location @a id_index.
    void register_node_ref(
        size_t                      id_index,
        const Intermediate::node_p& node
    )
    {
        m_node_id_map[id_index] = node;
    }

    //! Append a reference to @a output.
    void append_output_ref(const Intermediate::output_p& output)
    {
        e_id_t* e_id = m_assembler.append_object(e_id_t());
        register_output_ref(m_assembler.index(e_id), output);
    }

    //! Append a reference to a @node.
    void append_node_ref(const Intermediate::node_p& node)
    {
        e_id_t* e_id = m_assembler.append_object(e_id_t());
        register_node_ref(m_assembler.index(e_id), node);
    }

    /**
     * Transitively close @c m_outputs.
     *
     * This method does a breadth first search starting with the contents
     * of @c m_outputs and adds any new outputs found to @c m_outputs.  I.e.,
     * finds outputs that are only referred to be other outputs and adds
     * them.
     */
    void complete_outputs()
    {
        list<Intermediate::output_p> todo(m_outputs.begin(), m_outputs.end());
        while (! todo.empty()) {
            const Intermediate::output_p output = todo.front();
            todo.pop_front();
            if (output->next_output()) {
                bool is_new = m_outputs.insert(output->next_output()).second;
                if (is_new) {
                    todo.push_back(output->next_output());
                }
            }
        }
    }

    //! Appends all output lists and outputs in @c m_outputs to the buffer.
    void append_outputs()
    {
        // Set first_output.
        m_assembler.ptr<ia_eudoxus_automata_t>(
            m_e_automata_index
        )->first_output = m_assembler.size();

        // Calculate all contents.
        typedef map<Intermediate::byte_vector_t, size_t> output_content_map_t;
        output_content_map_t output_contents;
        BOOST_FOREACH(const Intermediate::output_p& output, m_outputs)
        {
            output_contents.insert(make_pair(output->content(), 0));
        }

        // Append all contents.  Note non-const reference.
        BOOST_FOREACH(
            output_content_map_t::value_type& v,
            output_contents
        )
        {
            ia_eudoxus_output_t* e_output =
                m_assembler.append_object(ia_eudoxus_output_t());
            v.second = m_assembler.index(e_output);
            e_output->length = v.first.size();

            m_assembler.append_bytes(v.first.data(), v.first.size());
            if (m_assembler.size() >= m_max_index) {
                throw out_of_range("id_width too small");
            }
        }

        m_assembler.ptr<ia_eudoxus_automata_t>(
            m_e_automata_index
        )->num_outputs = output_contents.size();
        m_result.ids_used += output_contents.size();

        // Handle all outputs.
        m_assembler.ptr<ia_eudoxus_automata_t>(
            m_e_automata_index
        )->first_output_list = m_assembler.size();
        BOOST_FOREACH(const Intermediate::output_p& output, m_outputs)
        {
            if (! output->next_output()) {
                // Single outputs will just point directly to the content.
                m_output_map[output] = output_contents[output->content()];
            }
            else {
                // Multiple outputs need a list.
                e_output_list_t* e_output_list =
                    m_assembler.append_object(e_output_list_t());

                ++m_assembler.ptr<ia_eudoxus_automata_t>(
                    m_e_automata_index
                )->num_output_lists;

                m_output_map[output] = m_assembler.index(e_output_list);
                e_output_list->output = output_contents[output->content()];
                // Register even if NULL to get id count correct.
                register_output_ref(
                    m_assembler.index(&(e_output_list->next_output)),
                    output->next_output()
                );
            }
            if (m_assembler.size() >= m_max_index) {
                throw out_of_range("id_width too small");
            }
        }
    }

    //! Returns unique child of @a node or singular node_p if no unique child.
    static
    Intermediate::node_p has_unique_child(const Intermediate::node_p& node)
    {
        if (
            node->edges().size() == 1 &&
            node->edges().front().size() == 1
        ) {
            return node->edges().front().target();
        }
        return Intermediate::node_p();
    }

    //! Returns true iff @a a and @a b have the same default behavior.
    static
    bool same_defaults(
        const Intermediate::node_p& a,
        const Intermediate::node_p& b
    )
    {
        return
            a->default_target()     == b->default_target() &&
            a->advance_on_default() == b->advance_on_default();
    }

    //! Add all children of @a node to @a parents.
    static
    void calculate_parents(
        parent_map_t&               parents,
        const Intermediate::node_p& node
    )
    {
        BOOST_FOREACH(const Intermediate::Edge& edge, node->edges()) {
            parents[edge.target()].insert(node);
        }
        if (node->default_target()) {
            parents[node->default_target()].insert(node);
        }
    }

    //! Logger to use.
    logger_t m_logger;

    //! Where to store the result.
    result_t& m_result;

    //! Compiler Configuration.
    configuration_t m_configuration;

    //! Assembler of m_result.buffer.
    BufferAssembler m_assembler;

    //! Index of automata structure.
    size_t m_e_automata_index;

    //! Type of m_node_map.
    typedef map<Intermediate::node_p, size_t> node_map_t;
    //! Type of m_output_map.
    typedef map<Intermediate::output_p, size_t> output_map_t;

    //! Map of node to location in buffer.
    node_map_t m_node_map;
    //! Map of output to location in buffer.
    output_map_t m_output_map;

    //! Type of m_node_id_map.
    typedef map<size_t, Intermediate::node_p> node_id_map_t;
    //! Type of m_output_id_map.
    typedef map<size_t, Intermediate::output_p> output_id_map_t;

    //! Map of id location to node.
    node_id_map_t m_node_id_map;
    //! Map of id location to output.
    output_id_map_t m_output_id_map;

    //! Type of m_outputs.
    typedef set<Intermediate::output_p> output_set_t;
    //! Set of all known outputs.
    output_set_t m_outputs;

    //! Maximum index of buffer based on id_width.
    const uint64_t m_max_index;
};

template <size_t id_width>
Compiler<id_width>::Compiler(
    result_t&       result,
    configuration_t configuration
) :
    m_result(result),
    m_configuration(configuration),
    m_assembler(result.buffer),
    m_e_automata_index(0),
    m_max_index(numeric_limits<e_id_t>::max())
{
    // nop
}

template <size_t id_width>
void Compiler<id_width>::compile(
    const Intermediate::Automata& automata
)
{
    m_result.buffer.clear();
    m_result.ids_used = 0;
    m_result.padding = 0;
    m_result.low_nodes = 0;
    m_result.low_nodes_bytes = 0;
    m_result.high_nodes = 0;
    m_result.high_nodes_bytes = 0;
    m_result.pc_nodes = 0;
    m_result.pc_nodes_bytes = 0;

    // Header
    ia_eudoxus_automata_t* e_automata =
        m_assembler.append_object(ia_eudoxus_automata_t());
    e_automata->version              = EUDOXUS_VERSION;
    e_automata->id_width             = id_width;
    e_automata->is_big_endian        = ia_eudoxus_is_big_endian();
    e_automata->no_advance_no_output = automata.no_advance_no_output();
    e_automata->reserved             = 0;

    // Fill in later.
    e_automata->num_nodes        = 0;
    e_automata->num_outputs      = 0;
    e_automata->num_output_lists = 0;
    e_automata->data_length      = 0;

    // Store index as it will likely move.
    m_e_automata_index = m_assembler.index(e_automata);

    // Calculate Node Parents
    parent_map_t parents;
    breadth_first(
        automata,
        boost::bind(calculate_parents, boost::ref(parents), _1)
    );

    // Adapted BFS... Complicated by path compression nodes.
    queue<Intermediate::node_p> todo;
    set<Intermediate::node_p>   queued;

    todo.push(automata.start_node());
    queued.insert(automata.start_node());

    while (! todo.empty()) {
        Intermediate::node_p node = todo.front();
        todo.pop();

        // Padding
        size_t index = m_assembler.size();
        size_t alignment = index % m_configuration.align_to;
        size_t padding = (
            alignment == 0 ?
            0 :
            m_configuration.align_to - alignment
        );
        if (padding > 0) {
            m_result.padding += padding;
            for (size_t i = 0; i < padding; ++i) {
                m_assembler.append_object(uint8_t(0xaa));
            }
        }
        assert(m_assembler.size() % m_configuration.align_to == 0);

        // Record node location.
        m_node_map[node] = m_assembler.size();

        Intermediate::node_p end_of_path = node;
        Intermediate::node_p child = has_unique_child(end_of_path);
        size_t path_length = 0;
        while (
            path_length <= 255 &&
            child &&
            ! child->first_output() &&
            end_of_path->edges().front().advance() &&
            has_unique_child(child) &&
            same_defaults(end_of_path, child) &&
            parents[child].size() == 1
        ) {
            end_of_path = child;
            child = has_unique_child(child);
            ++path_length;
        }

        if (path_length >= 2) {
            // Path Compression
            pc_node(node, end_of_path, path_length);
            // Add end of path.
            bool need_to_queue = queued.insert(end_of_path).second;
            if (need_to_queue) {
                todo.push(end_of_path);
            }
        }
        else {
            // Demux: High or Low
            demux_node(node);

            // And add all children.
            BOOST_FOREACH(const Intermediate::Edge& edge, node->edges()) {
                const Intermediate::node_p& target = edge.target();
                bool need_to_queue = queued.insert(target).second;
                if (need_to_queue) {
                    todo.push(target);
                }
            }
        }
        if (node->default_target()) {
            const Intermediate::node_p& target =
                node->default_target();
            bool need_to_queue = queued.insert(target).second;
            if (need_to_queue) {
                todo.push(target);
            }
        }

        if (m_assembler.size() >= m_max_index) {
            throw out_of_range("id_width too small");
        }
    }

    complete_outputs();
    append_outputs();

    fill_in_ids(m_node_id_map, m_node_map);
    fill_in_ids(m_output_id_map, m_output_map);

    // Append metadata.
    typedef map<string, string> map_t;
    size_t metadata_index = m_assembler.size();
    BOOST_FOREACH(const map_t::value_type& v, automata.metadata()) {
        ia_eudoxus_output_t* e_output =
            m_assembler.append_object(ia_eudoxus_output_t());
        e_output->length = v.first.size();
        m_assembler.append_bytes(
            reinterpret_cast<const uint8_t*>(v.first.data()), v.first.size()
        );

        e_output = m_assembler.append_object(ia_eudoxus_output_t());
        e_output->length = v.second.size();
        m_assembler.append_bytes(
            reinterpret_cast<const uint8_t*>(v.second.data()), v.second.size()
        );
    }

    // Recover pointer.
    e_automata = m_assembler.ptr<ia_eudoxus_automata_t>(m_e_automata_index);
    e_automata->num_nodes      = m_node_map.size();
    e_automata->num_outputs    = m_output_map.size();
    e_automata->num_metadata   = automata.metadata().size();
    e_automata->metadata_index = metadata_index;
    e_automata->data_length    = m_result.buffer.size();
    assert(m_node_map[automata.start_node()] < 256);
    e_automata->start_index = m_node_map[automata.start_node()];

    m_result.ids_used += m_node_id_map.size() + m_output_id_map.size();
}

result_t compile_minimal(
    const Intermediate::Automata& automata,
    configuration_t               configuration
)
{
    static const size_t c_id_widths[] = {1, 2, 4, 8};
    static const size_t c_num_id_widths =
        sizeof(c_id_widths) / sizeof(*c_id_widths);

    if (configuration.id_width != 0) {
        throw logic_error(
            "compile_minimal called with non-0 id_width."
            "  Please report as bug."
        );
    }

    result_t result;
    size_t i;

    for (i = 0; i < c_num_id_widths; ++i) {
        bool success = true;
        try {
            configuration.id_width = c_id_widths[i];
            result = compile(automata, configuration);
        }
        catch (out_of_range) {
            // move on to next id_width.
            success = false;
        }
        if (success) {
            break;
        }
    }

    if (i == c_num_id_widths) {
        throw logic_error(
            "Insanity error.  "
            "Could not fit automata in 2**8 bytes?  "
            "Please report as bug."
        );
    }

    return result; // RVO
}

} // Anonymous

configuration_t::configuration_t() :
    id_width(0),
    align_to(1),
    high_node_weight(1.0)
{
    // nop
}

result_t compile(
    const Intermediate::Automata& automata,
    configuration_t               configuration
)
{
    if (configuration.id_width == 0) {
        return compile_minimal(automata, configuration);
    }

    result_t result;
    result.configuration = configuration;

    switch (configuration.id_width) {
    case 1:
        Compiler<1>(result, configuration).compile(automata);
        break;
    case 2:
        Compiler<2>(result, configuration).compile(automata);
        break;
    case 4:
        Compiler<4>(result, configuration).compile(automata);
        break;
    case 8:
        Compiler<8>(result, configuration).compile(automata);
        break;
    default:
        throw logic_error("Unsupported id_width.");
    }

    return result; // RVO
}

} // EudoxusCompiler
} // IronAutomata
