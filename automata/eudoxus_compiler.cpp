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

#include <boost/foreach.hpp>

#include <queue>
#include <set>

using namespace std;

namespace IronAutomata {
namespace EudoxusCompiler {

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
     * @param[in] result   Where to store results.
     * @param[in] align_to Padding will be added as needed to align all node
     *                     indices to @a align_to.  I.e.,
     *                     index mod @a align_to will be 0.
     */
    explicit
    Compiler(result_t& result, size_t align_to = 1);

    /**
     * Compile automata.
     *
     * @param[in]  automata Automata to compile.
     */
    void compile(
        const Intermediate::automata_t& automata
    );

private:
    //! Subengine traits.
    typedef Eudoxus::subengine_traits<id_width> traits_t;
    //! Eudoxus Identifier.
    typedef typename traits_t::id_t       e_id_t;
    //! Eudoxus Low Edge.
    typedef typename traits_t::low_edge_t e_low_edge_t;
    //! Eudoxus Low Node.
    typedef typename traits_t::low_node_t e_low_node_t;
    //! Eudoxus Output.
    typedef typename traits_t::output_t   e_output_t;

    friend class BFSVisitor;

    /**
     * Functor to use with Intermediate::breadth_first().
     */
    class BFSVisitor
    {
    public:
        /**
         * Constructor.
         *
         * @param[in] parent The current Compiler instance.
         */
        explicit
        BFSVisitor(Compiler& parent) :
            m_parent(parent)
        {
            // nop
        }

        /**
         * Call operator; process anode.
         */
        void operator()(const Intermediate::node_p& node)
        {
            if (m_parent.m_assembler.size() >= m_parent.m_max_index) {
                throw out_of_range("id_width too small");
            }

            size_t index = m_parent.m_assembler.size();
            size_t alignment = index % m_parent.m_align_to;
            size_t padding = (
                alignment == 0 ?
                0 :
                m_parent.m_align_to - alignment
            );
            if (padding > 0) {
                m_parent.m_result.padding += padding;
                for (size_t i = 0; i < padding; ++i) {
                    m_parent.m_assembler.append_object(uint8_t(0xaa));
                }
            }
            assert(m_parent.m_assembler.size() % m_parent.m_align_to == 0);
            m_parent.m_node_map[node] = m_parent.m_assembler.size();
            // Eventually there will be other node types.
            low_node(*node);
        }

    private:
        //! True iff @a edge does not advance input.
        static
        bool is_nonadvancing(const Intermediate::edge_t& edge)
        {
            return ! edge.advance;
        }

        /**
         * Compile @a node as a low_node.
         *
         * Appends a low node to the buffer representing @a node.
         *
         * @param[in] node Intermediate node to compile.
         */
        void low_node(const Intermediate::node_t& node)
        {
            e_low_node_t* header =
                m_parent.m_assembler.append_object(e_low_node_t());

            bool has_nonadvancing = (
                find_if(node.edges.begin(), node.edges.end(), is_nonadvancing)
            ) != node.edges.end();

            header->header.type = IA_EUDOXUS_LOW;
            header->header.flags = 0;
            if (node.output) {
                header->header.flags = ia_setbit8(header->header.flags, 0);
            }
            if (has_nonadvancing) {
                header->header.flags = ia_setbit8(header->header.flags, 1);
            }
            if (node.default_target) {
                header->header.flags = ia_setbit8(header->header.flags, 2);
            }
            if (node.advance_on_default) {
                header->header.flags = ia_setbit8(header->header.flags, 3);
            }

            if (node.edges.size() > 0) {
                header->header.flags = ia_setbit8(header->header.flags, 4);
                m_parent.m_assembler.append_object(
                    uint8_t(node.edges.size())
                );
            }

            if (node.output) {
                m_parent.append_output_ref(node.output);
                m_parent.m_outputs.insert(node.output);
            }

            if (node.default_target) {
                m_parent.append_node_ref(node.default_target);
            }

            size_t advance_index = 0;
            if (has_nonadvancing) {
                uint8_t* advance =
                    m_parent.m_assembler.template append_array<uint8_t>(
                        (node.edges.size() + 7) / 8
                    );
                advance_index = m_parent.m_assembler.index(advance);
            }

            size_t edge_i = 0;
            BOOST_FOREACH(const Intermediate::edge_t& edge, node.edges) {
                if (edge.values.empty() && edge.values_bm.empty()) {
                    throw runtime_error(
                        "Epsilon edges currently unsupported."
                    );
                }
                set<uint8_t> values;
                BOOST_FOREACH(uint8_t value, edge_values(edge)) {
                    bool in_set = ! values.insert(value).second;
                    if (in_set) {
                        throw runtime_error(
                            "Non-deterministic automata unsupported."
                        );
                    }
                    if (has_nonadvancing && edge.advance) {
                        ia_setbitv(
                            m_parent.m_assembler.template ptr<uint8_t>(
                                advance_index
                            ),
                            edge_i
                        );
                    }
                    ++edge_i;

                    e_low_edge_t* e_edge =
                        m_parent.m_assembler.append_object(e_low_edge_t());
                    e_edge->c = value;
                    m_parent.register_node_ref(
                        m_parent.m_assembler.index(&(e_edge->next_node)),
                        edge.target
                    );
                }
            }
        }

        Compiler& m_parent;
    };

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
            if (output->next_output) {
                bool is_new = m_outputs.insert(output->next_output).second;
                if (is_new) {
                    todo.push_back(output->next_output);
                }
            }
        }
    }

    //! Appends all outputs in @c m_outputs to the buffer.
    void append_outputs()
    {
        BOOST_FOREACH(const Intermediate::output_p& output, m_outputs)
        {
            e_output_t* e_output = m_assembler.append_object(e_output_t());
            m_output_map[output] = m_assembler.index(e_output);
            e_output->output_length = output->content.size();
            // Register even if NULL to get id count correct.
            register_output_ref(
                m_assembler.index(&(e_output->next_output)),
                output->next_output
            );
            m_assembler.append_bytes(
                output->content.data(),
                output->content.size()
            );
        }
    }

    //! Logger to use.
    logger_t m_logger;

    //! Where to store the result.
    result_t& m_result;

    //! What to align node indices to.
    size_t m_align_to;

    //! Assembler of m_result.buffer.
    BufferAssembler m_assembler;

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
Compiler<id_width>::Compiler(result_t& result, size_t align_to) :
    m_result(result),
    m_align_to(align_to),
    m_assembler(result.buffer),
    m_max_index(numeric_limits<e_id_t>::max())
{
    // nop
}

template <size_t id_width>
void Compiler<id_width>::compile(
    const Intermediate::automata_t& automata
)
{
    m_result.buffer.clear();
    m_result.ids_used = 0;
    m_result.padding = 0;

    // Header
    ia_eudoxus_automata_t* e_automata =
        m_assembler.append_object(ia_eudoxus_automata_t());
    e_automata->version              = IA_EUDOXUS_VERSION;
    e_automata->id_width             = id_width;
    e_automata->is_big_endian        = ia_eudoxus_is_big_endian();
    e_automata->no_advance_no_output = automata.no_advance_no_output;
    e_automata->reserved             = 0;

    // Fill in at end.
    e_automata->num_nodes   = 0;
    e_automata->num_outputs = 0;
    e_automata->data_length = 0;

    // Store index as it will likely move.
    size_t e_automata_index = m_assembler.index(e_automata);

    Intermediate::breadth_first(automata, BFSVisitor(*this));

    complete_outputs();
    append_outputs();

    fill_in_ids(m_node_id_map, m_node_map);
    fill_in_ids(m_output_id_map, m_output_map);

    // Recover pointer.
    e_automata = m_assembler.ptr<ia_eudoxus_automata_t>(e_automata_index);
    e_automata->num_nodes   = m_node_map.size();
    e_automata->num_outputs = m_output_map.size();
    e_automata->data_length = m_result.buffer.size();
    assert(m_node_map[automata.start_node] < 256);
    e_automata->start_index = m_node_map[automata.start_node];

    m_result.ids_used = m_node_id_map.size() + m_output_id_map.size();
}

} // Anonymous

result_t compile(
    const Intermediate::automata_t& automata,
    size_t                          id_width,
    size_t                          align_to
)
{
    result_t result;

    result.id_width = id_width;
    result.align_to = align_to;

    switch (id_width) {
    case 1:
        Compiler<1>(result, align_to).compile(automata); break;
    case 2:
        Compiler<2>(result, align_to).compile(automata); break;
    case 4:
        Compiler<4>(result, align_to).compile(automata); break;
    case 8:
        Compiler<8>(result, align_to).compile(automata); break;
    default:
        throw logic_error("Unsupported id_width.");
    }

    return result; // RVO
}

result_t compile_minimal(
    const Intermediate::automata_t& automata,
    size_t                          align_to
)
{
    static const size_t c_id_widths[] = {1, 2, 4, 8};
    static const size_t c_num_id_widths =
        sizeof(c_id_widths) / sizeof(*c_id_widths);

    result_t result;
    size_t i;

    for (i = 0; i < c_num_id_widths; ++i) {
        bool success = true;
        try {
            result = compile(automata, c_id_widths[i], align_to);
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

} // EudoxusCompiler
} // IronAutomata
