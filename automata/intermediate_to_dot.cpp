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
 * @brief IronBee --- Intermediate to dot implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate_to_dot.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <set>

using namespace std;

namespace IronAutomata {
namespace Intermediate {

namespace {

/**
 * Helper class for intermediate_to_dot().
 *
 * Should be used as functional for breadth_first() and, when that's done,
 * have flush_outputs() called.
 */
class intermediate_to_dot_helper
{
public:
    //! Constructor.  See intermediate_to_dot()
    intermediate_to_dot_helper(ostream& out, bool label_by_pointer) :
        m_out(out),
        m_label_by_pointer(label_by_pointer),
        m_first_node(true),
        m_next_id(1)
    {
        // nop
    }

    //! Call operator.  For use with breadth_first().
    void operator()(const node_p& node)
    {
        size_t id = m_next_id;
        ++m_next_id;
        m_out << "  \"" << node << "\" [label=\"";
        if (m_label_by_pointer) {
            m_out << node;
        }
        else {
            m_out << id;
        }
         m_out << "\"";

        if (m_first_node) {
            m_first_node = false;
            m_out << ", shape=diamond";
        }
        m_out << "];" << endl;

        BOOST_FOREACH(const Edge& edge, node->edges()) {
            m_out << "  \"" << node << "\" -> \"" << edge.target()
                 << "\" [weight=1000, label=\"";
            if (edge.epsilon()) {
                m_out << "&epsilon;";
            }
            else {
                BOOST_FOREACH(uint8_t c, edge) {
                    render(byte_vector_t(1, c));
                }
            }
            if (! edge.advance()) {
                m_out << "\", color=red";
            }
            m_out << "\"];" << endl;
        }

        if (node->default_target()) {
            m_out << "  \"" << node << "\" -> \"" << node->default_target()
                  << "\" [style=dashed, label=\"default\"";
            if (! node->advance_on_default()) {
                m_out << ", color=red";
            }
            m_out << "];" << endl;
        }

        if (node->first_output()) {
            m_out << "  \"" << node << "\" -> \"output"
                  << node->first_output() << "\" [style=dotted];" << endl;
            m_outputs.insert(node->first_output());
        }
    }

    //! Write all outputs.
    void flush_outputs()
    {
        list<output_p> todo;
        copy(m_outputs.begin(), m_outputs.end(), back_inserter(todo));
        while (! todo.empty()) {
            output_p output = todo.front();
            todo.pop_front();

            if (
                output->next_output() &&
                ! m_outputs.count(output->next_output())
            ) {
                todo.push_back(output->next_output());
                m_outputs.insert(output->next_output());
            }
        }

        BOOST_FOREACH(const output_p& output, m_outputs) {
            m_out << "  \"output" << output << "\" [shape=box, label=\"";
            render(output->content());
            m_out << "\"];" << endl;
            if (output->next_output()) {
                m_out << "  \"output" << output << "\" -> \"output"
                      << output->next_output() << "\" [style=dotted];" << endl;
            }
        }
    }

private:
    /**
     * Write the content of a string with escaping.
     *
     * Any non-printable character will be printed as decimal values in angle
     * brackets.
     *
     * @param[in] v Vector to write.
     */
    void render(const byte_vector_t& v)
    {
        BOOST_FOREACH(char c, v) {
            switch (c) {
            case '&': m_out << "&amp;"; break;
            case '"': m_out << "&quot;"; break;
            case '\'': m_out << "&apos;"; break;
            case '<': m_out << "&lt;"; break;
            case '>': m_out << "&gt;"; break;
            case '\\': m_out << "\\\\"; break;
            default:
                if (c < 32 || c > 126) {
                    m_out << boost::format("&lang;%d&rang;") % uint32_t(c);
                }
                else {
                    m_out << boost::format("%c") % c;
                }
            }
        }
    }

    ostream& m_out;
    bool m_label_by_pointer;
    bool m_first_node;
    size_t m_next_id;
    set<output_p> m_outputs;
};

}

void intermediate_to_dot(
    ostream&        out,
    const Automata& automata,
    bool            label_by_pointer
)
{
    out << "digraph A {" << endl;
    intermediate_to_dot_helper helper(out, label_by_pointer);
    breadth_first(automata, boost::ref(helper));
    helper.flush_outputs();
    out << "}" << endl;
}

} // Intermediate
} // IronAutomata
