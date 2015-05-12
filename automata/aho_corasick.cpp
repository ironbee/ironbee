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
 * @brief IronAutomata --- Aho-Corasick Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/generator/aho_corasick.hpp>
#include <ironautomata/buffer.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/assign.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <list>
#include <stdexcept>

using boost::assign::list_of;
using namespace std;

namespace IronAutomata {
namespace Generator {

namespace {

/**
 * Specialized subclass node type.
 *
 * Adds a pointer to the last output in the output chain to allow easy
 * merging of output sets.  All nodes will be created as ACNode's and
 * static pointer casts will be used to retrieve when needed.
 */
struct ACNode : public Intermediate::Node
{
    /**
     * Last output in output change.
     *
     * prepend_output() will maintain this and @c first_output.
     * append_outputs() will use it to append another nodes outputs to
     * this nodes output.  append_outputs() should be called at most once.
     */
    Intermediate::output_p last_output;

    /**
     * Prepend an output with content @a content.
     */
    void prepend_output(const Intermediate::byte_vector_t& content)
    {
        Intermediate::output_p output = boost::make_shared<Intermediate::Output>();
        output->content() = content;
        output->next_output() = first_output();
        if (! last_output) {
            last_output = output;
        }
        first_output() = output;
    }

    /**
     * Append outputs of node @a other to this node.
     *
     * This method should be called zero or one times for each node.  It is
     * is fine, however, to append this node to other nodes multiple times.
     */
    void append_outputs(const Intermediate::node_p& other)
    {
        if (! last_output) {
            assert(! first_output());
            first_output() = last_output = other->first_output();
        }
        else {
            last_output->next_output() = other->first_output();
            last_output.reset();
        }
    }
};

/**
 * Next node for an input of @a c at node @a node or Intermediate::node_p().
 */
Intermediate::node_p find_next(
    const Intermediate::node_p& node,
    uint8_t       c
);

/**
 * Do a deep tree copy of tree below @a src_head to be below @a dst_head.
 *
 * Behavior is undefined if subgraph reachable from @a src_head is not a tree.
 * Assumes @a dst_head is an empty node.  Ignores defaults.  Uses ACNodes, so
 * will not work properly in other contexts.
 *
 * @param[in] dst_head Where to copy tree to.
 * @param[in] src_head Top of tree to copy.
 */
void deep_copy(
    Intermediate::node_p&       dst_head,
    const Intermediate::node_p& src_head
);

/**
 * Split edge @a from using @a to_values.
 *
 * Creates a new edge, @c to, with values @a to_values.  Removes @a to_values
 * from edge @a from.  Does deep copy of target of @a from edge to target of
 * @c to edge.
 *
 * Uses deep_copy() and ACNodes so will not work in other contexts.
 *
 * Behavior is undefined if @a to_values is not a subset of values of @a from,
 * or if @a from is not vector based.
 *
 * @throw invalid_argument if @a to_values is empty or @a to_values is a
 * superset of values of from.
 *
 * @param[in] from      Edge to split.
 * @param[in] to_values Values to from @a from to a new edge.
 * @return New, @c to, edge.
 */
Intermediate::Edge split_edge(
    Intermediate::Edge&                from,
    const Intermediate::byte_vector_t& to_values
);

/**
 * Set default target of @a node to @a default_target and append outputs.
 */
void set_default_target(
    const Intermediate::node_p& node,
    const Intermediate::node_p& default_target
);

/**
 * Process all failure transitions of @a automata.
 *
 * See add_word() for discussion.
 */
void process_failures(Intermediate::Automata& automata);

/**
 * Convert a subpattern to a set of values.
 *
 * See aho_corasick_add_pattern() for details.
 */
const Intermediate::byte_vector_t subpat_to_set(char subpat[4]);

/**
 * Parse @a pattern starting at @a j, returning values and incrementing @a j.
 *
 * When finished @a j will point to the next subpattern.
 *
 * @param[in]     pattern  Pattern to parse.
 * @param[in,out] j        Current location in @a pattern.  Incremented to next
 *                         location.
 * @param[in]     in_union Set to true if extracting from inside a union.
 * @return Set of values.
 */
Intermediate::byte_vector_t extract_cs(
    const string&   pattern,
    size_t&         j,
    bool in_union = false
);

// Definitions

Intermediate::node_p find_next(
    const Intermediate::node_p& node,
    uint8_t       c
)
{
    Intermediate::Node::edge_list_t next_edges = node->edges_for(c);
    if (next_edges.empty()) {
        return Intermediate::node_p();
    }
    else {
        if (next_edges.size() != 1) {
            throw logic_error("Unexpected non-determinism.");
        }
        return next_edges.front().target();
    }
}

void deep_copy(
    Intermediate::node_p&       dst_head,
    const Intermediate::node_p& src_head
)
{
    typedef pair<Intermediate::node_p, Intermediate::node_p> dst_src_t;
    typedef list<dst_src_t> todo_t;

    todo_t todo;
    todo.push_back(make_pair(dst_head, src_head));

    Intermediate::node_p dst;
    Intermediate::node_p src;
    while (! todo.empty()) {
        boost::tie(dst, src) = todo.front();
        todo.pop_front();

        if (src->default_target()) {
            dst->default_target() = boost::make_shared<ACNode>();
            todo.push_back(
                make_pair(dst->default_target(), src->default_target())
            );
            dst->advance_on_default() = src->advance_on_default();
        }
        if (src->first_output()) {
            Intermediate::output_p current_src = src->first_output();
            Intermediate::output_p current_dst =
                boost::make_shared<Intermediate::Output>();
            boost::shared_ptr<ACNode> ac_dst =
                boost::static_pointer_cast<ACNode>(dst);
            ac_dst->first_output() = ac_dst->last_output = current_dst;
            while (current_src) {
                assert(current_dst);
                current_dst->content() = current_src->content();
                if (current_src->next_output()) {
                    current_dst->next_output() =
                        boost::make_shared<Intermediate::Output>();
                    ac_dst->last_output = current_dst->next_output();
                }
                current_dst = current_dst->next_output();
                current_src = current_src->next_output();
            }
        }

        BOOST_FOREACH(const Intermediate::Edge& src_edge, src->edges()) {
            dst->edges().push_back(Intermediate::Edge());
            Intermediate::Edge& dst_edge = dst->edges().back();
            dst_edge = src_edge;
            dst_edge.target() = boost::make_shared<ACNode>();
            todo.push_back(make_pair(dst_edge.target(), src_edge.target()));
        }
    }
}

Intermediate::Edge split_edge(
    Intermediate::Edge&                from,
    const Intermediate::byte_vector_t& to_values
)
{
    Intermediate::Edge to;

    if (to_values.empty()) {
        throw invalid_argument("Illegal split: to edge would be empty.");
    }

    // Fill in values.
    Intermediate::byte_vector_t from_values;
    set_difference(
        from.begin(), from.end(),
        to_values.begin(), to_values.end(),
        back_inserter(from_values)
    );

    if (from_values.empty()) {
        throw invalid_argument("Illegal split: old edge would be emptied.");
    }

    from.vector().swap(from_values);
    to.vector() = to_values;

    // This method assumes the context of Aho-Corasick.  If generalized,
    // ACNode should be changed to node, and advance() should be updated
    // instead of asserted.
    to.target() = boost::make_shared<ACNode>();
    deep_copy(to.target(), from.target());

    assert(to.advance());
    assert(from.advance());

    return to;
}

void set_default_target(
    const Intermediate::node_p& node,
    const Intermediate::node_p& default_target
)
{
    assert(! node->default_target());

    node->default_target() = default_target;
    node->advance_on_default() = false;

    if (default_target->first_output()) {
        boost::static_pointer_cast<ACNode>(node)->append_outputs(
            default_target
        );
    }
}

void process_failures(Intermediate::Automata& automata)
{
    assert(automata.start_node());

    typedef list<Intermediate::node_p> node_list_t;
    node_list_t todo;

    BOOST_FOREACH(
        const Intermediate::Edge& edge,
        automata.start_node()->edges()
    ) {
        const Intermediate::node_p& target = edge.target();
        target->default_target() = automata.start_node();
        target->advance_on_default() = false;
        todo.push_back(target);
    }

    Intermediate::byte_vector_t shared_cs;
    Intermediate::byte_vector_t remaining_cs;
    while (! todo.empty()) {
        Intermediate::node_p r = todo.front();
        todo.pop_front();

        BOOST_FOREACH(Intermediate::Edge& edge, r->edges()) {
            assert(! edge.vector().empty());
            Intermediate::byte_vector_t cs = edge.vector();

            Intermediate::node_p s = edge.target();

            assert(! s->default_target());

            todo.push_back(s);

            Intermediate::node_p current_node = r->default_target();
            while (! cs.empty()) {
                BOOST_FOREACH(const Intermediate::Edge& current_edge, current_node->edges()) {
                    shared_cs.clear();
                    set_intersection(
                        current_edge.begin(), current_edge.end(),
                        cs.begin(), cs.end(),
                        back_inserter(shared_cs)
                    );

                    if (shared_cs.empty()) {
                        // current_edge has no bearing on edge, skip.
                        continue;
                    }

                    if (shared_cs.size() == cs.size()) {
                        // current_edge absorbs everything.
                        cs.clear();
                        set_default_target(s, current_edge.target());
                        break;
                    }

                    // current_edge overlaps.  Split edge into overlap and
                    // remainder.  new_edge will be overlap, edge will
                    // become remainder.

                    // Should never reach this case in a non-pattern based
                    // AC run.
                    Intermediate::Edge new_edge = split_edge(edge, shared_cs);
                    r->edges().push_front(new_edge);
                    Intermediate::node_p s2 = new_edge.target();
                    todo.push_back(s2);
                    set_default_target(s2, current_edge.target());
                    // Reduce cs.
                    remaining_cs.clear();
                    set_difference(
                        cs.begin(), cs.end(),
                        shared_cs.begin(), shared_cs.end(),
                        back_inserter(remaining_cs)
                    );
                    cs.swap(remaining_cs);

                    // If we've handled everything, break early.  Note that while
                    // loop will immediately terminate.
                    if (cs.empty()) {
                        break;
                    }
                }

                // If values are left, have to adjust current node.
                if (! cs.empty()) {
                    // Still have values remaining, update current_node and
                    // keep going.
                    if (current_node == automata.start_node()) {
                        // Special case, default of start node is start node,
                        // so no reason to keep going.
                        set_default_target(s, automata.start_node());
                        cs.clear();
                    }
                    current_node = current_node->default_target();
                }
            }
        }
    }
}

// Support for extract_cs()

//! True if c =~ /[A-Za-z]/
inline
bool is_alpha(char c)
{
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z')
        ;
}

//! True if c =~ /[A-Za-z0-9]/
inline
bool is_hex(char c)
{
    return
        is_alpha(c) ||
        (c >= '0' && c <= '9')
        ;
}

//! True iff c c =~ /\?|[@-_]/
inline
bool is_control(char c)
{
    return
        (c >= '@' && c <= '_') ||
        (c == '?')
        ;
}

//! Translate hex character to value.
inline
int parse_hex(char c)
{
    assert(is_hex(c));
    if (c >= 'a') {
        return c - 'a' + 10;
    }
    else if (c >= 'A') {
        return c - 'A' + 10;
    }
    else {
        return c - '0';
    }
}

//! Translate X to lower case.
inline
int lowercase(char c)
{
    assert(is_alpha(c));
    return c >= 'a' ? c : ('a' + (c - 'A'));
}

//! Translate X to upper case.
inline
int uppercase(char c)
{
    assert(is_alpha(c));
    return c < 'a' ? c : ('A' + (c - 'a'));
}

//! Convert single subpattern to set.
// Single is a bit misleading.  This is subpatterns whose sets are not
// precomputed.
Intermediate::byte_vector_t single_subpat_to_set(char subpat[4])
{
    if (subpat[0] != '\\') {
        return list_of(subpat[0]);
    }
    switch (subpat[1]) {
    // Parameterized Single
    case '^':
        assert(is_control(subpat[2]));
        if (subpat[2] == '?') {
            return list_of(127);
        }
        else {
            return list_of(subpat[2] - '@');
        }
    case 'x':
        assert(is_hex(subpat[2]) && is_hex(subpat[3]));
        return list_of(parse_hex(subpat[2])*16 + parse_hex(subpat[3]));

    // Single
    case '\\': return list_of('\\');
    case '[': return list_of('[');
    case ']': return list_of(']');
    case 't': return list_of('\t');
    case 'v': return list_of('\v');
    case 'n': return list_of('\n');
    case 'r': return list_of('\r');
    case 'f': return list_of('\f');
    case '0': return list_of('\0');
    case 'e': return list_of('\e');
    case 'i':
        assert(is_alpha(subpat[2]));
        return list_of(uppercase(subpat[2]))(lowercase(subpat[2]));

    default:
        throw invalid_argument("Unknown pattern operator.");
    }
}

//! Add [@a a, @a b] to @a to.
void add_range(Intermediate::byte_vector_t& to, int a, int b)
{
    if (a > b) {
        throw invalid_argument("Can not add inverted range.");
    }
    while (a <= b) {
        to.push_back(a);
        ++a;
    }
}

//! Add @a from to @a to.
void add_set(
    Intermediate::byte_vector_t& to,
    const Intermediate::byte_vector_t& from
)
{
    Intermediate::byte_vector_t new_to;
    set_union(
        to.begin(), to.end(),
        from.begin(), from.end(),
        back_inserter(new_to)
    );
    new_to.swap(to);
}

//! Convert multiple subpattern to set.
const Intermediate::byte_vector_t& multiple_subpat_to_set(char subpat[4])
{
    // Very important to return *sorted* vectors.
    using boost::assign::list_of;

    static bool s_generated = false;
    static Intermediate::byte_vector_t s_any;
    static Intermediate::byte_vector_t s_digit;
    static Intermediate::byte_vector_t s_nondigit;
    static Intermediate::byte_vector_t s_hex;
    static Intermediate::byte_vector_t s_alpha;
    static Intermediate::byte_vector_t s_word;
    static Intermediate::byte_vector_t s_nonword;
    static Intermediate::byte_vector_t s_lower;
    static Intermediate::byte_vector_t s_upper;
    static Intermediate::byte_vector_t s_space;
    static Intermediate::byte_vector_t s_nonspace;
    static Intermediate::byte_vector_t s_eol;
    static Intermediate::byte_vector_t s_print;

    if (! s_generated) {
        add_range(s_any, 0, 255);
        add_range(s_digit, '0', '9');
        set_difference(
            s_any.begin(), s_any.end(),
            s_digit.begin(), s_digit.end(),
            back_inserter(s_nondigit)
        );
        s_hex = s_digit;
        add_range(s_hex, 'A', 'F');
        add_range(s_hex, 'a', 'f');
        add_range(s_lower, 'a', 'z');
        add_range(s_upper, 'A', 'Z');
        s_alpha = s_lower;
        add_set(s_alpha, s_upper);
        s_word = s_alpha;
        add_set(s_word, s_digit);
        set_difference(
            s_any.begin(), s_any.end(),
            s_word.begin(), s_word.end(),
            back_inserter(s_nonword)
        );
        s_space = list_of('\t')('\n')('\v')('\f')('\r')(' ')
			.convert_to_container<Intermediate::byte_vector_t>();
        set_difference(
            s_any.begin(), s_any.end(),
            s_space.begin(), s_space.end(),
            back_inserter(s_nonspace)
        );
        s_eol = list_of('\n')('\r')
			.convert_to_container<Intermediate::byte_vector_t>();
        add_range(s_print, 32, 127);

        s_generated = true;
    }

    switch (subpat[1]) {
    case '.': return s_any;
    case 'd': return s_digit;
    case 'D': return s_nondigit;
    case 'h': return s_hex;
    case 'w': return s_word;
    case 'W': return s_nonword;
    case 'a': return s_alpha;
    case 'l': return s_lower;
    case 'u': return s_upper;
    case 's': return s_space;
    case 'S': return s_nonspace;
    case '$': return s_eol;
    case 'p': return s_print;

    default:
        throw invalid_argument("Unknown pattern operator.");
    }
}

const Intermediate::byte_vector_t subpat_to_set(char subpat[4])
{
    static Intermediate::byte_vector_t temp;
    if (subpat[0] != '\\') {
        return list_of(subpat[0]);
    }
    if (
        subpat[1] == '\\' ||
        subpat[1] == 't' ||
        subpat[1] == 'v' ||
        subpat[1] == 'n' ||
        subpat[1] == 'r' ||
        subpat[1] == 'f' ||
        subpat[1] == '0' ||
        subpat[1] == 'e' ||
        subpat[1] == '^' ||
        subpat[1] == 'x' ||
        subpat[1] == '[' ||
        subpat[1] == ']' ||
        subpat[1] == 'i'
    )
    {
        temp = single_subpat_to_set(subpat);
        return temp;
    }
    return multiple_subpat_to_set(subpat);
}

Intermediate::byte_vector_t extract_cs(
    const string& pattern,
    size_t& j,
    bool in_union
)
{
    static bool s_generated = false;
    static Intermediate::byte_vector_t s_any;
    if (! s_generated) {
        add_range(s_any, 0, 255);
        s_generated = true;
    }

    if (! in_union && pattern[j] == '[') {
        ++j;
        Intermediate::byte_vector_t result;
        bool negate = false;
        if (j == pattern.length()) {
            throw invalid_argument("Union ends prematurely.");
        }
        if (pattern[j] == '^') {
            negate = true;
            ++j;
        }
        if (j == pattern.length()) {
            throw invalid_argument("Union ends prematurely.");
        }
        if (pattern[j] == '-') {
            result.push_back('-');
            ++j;
        }
        uint8_t range_begin = 0;
        bool in_range = false;
        bool valid_begin = false;
        for (;;) {
            if (j == pattern.length()) {
                throw invalid_argument("Union ends prematurely.");
            }
            if (pattern[j] == ']') {
                if (in_range) {
                    throw invalid_argument("Union ends before range does.");
                }
                ++j;
                break;
            }
            if (pattern[j] == '-') {
                if (! valid_begin) {
                    throw invalid_argument("Invalid range beginning.");
                }
                in_range = true;
                ++j;
                continue;
            }
            Intermediate::byte_vector_t subresult = extract_cs(pattern, j, true);
            if (in_range) {
                if (subresult.size() != 1) {
                    throw invalid_argument("Invalid range ending.");
                }
                uint8_t range_end = subresult.front();
                if (range_end <= range_begin) {
                    throw invalid_argument("Invalid range.");
                }
                subresult.clear();
                add_range(subresult, range_begin, range_end);
                in_range = false;
            }
            if (subresult.size() == 1) {
                range_begin = subresult.front();
                valid_begin = true;
            }
            else {
                valid_begin = false;
            }
            Intermediate::byte_vector_t newresult;
            set_union(
                subresult.begin(), subresult.end(),
                result.begin(), result.end(),
                back_inserter(newresult)
            );
            result.swap(newresult);
        }
        if (negate) {
            Intermediate::byte_vector_t newresult;
            set_difference(
                s_any.begin(), s_any.end(),
                result.begin(), result.end(),
                back_inserter(newresult)
            );
            result.swap(newresult);
        }
        return result;
    }
    else {
        char subpat[4] = {0, 0, 0, 0};
        subpat[0] = pattern[j++];
        if (subpat[0] == '\\') {
            if (j == pattern.length()) {
                throw invalid_argument("Pattern ends prematurely.");
            }
            subpat[1] = pattern[j++];
            if (subpat[1] == 'x') {
                if (j+1 == pattern.length()) {
                    throw invalid_argument("Pattern ends prematurely.");
                }
                subpat[2] = pattern[j++];
                subpat[3] = pattern[j++];
                if (! is_hex(subpat[2]) || ! is_hex(subpat[3])) {
                    throw invalid_argument("\\x was not expressed in hex.");
                }
            }
            else if (subpat[1] == '^' || subpat[1] == 'i') {
                if (j == pattern.length()) {
                    throw invalid_argument("Pattern ends prematurely.");
                }
                subpat[2] = pattern[j++];
                if (subpat[1] == '^' && ! is_control(subpat[2])) {
                    throw invalid_argument("\\^ did not specify valid control.");
                }
                else if (subpat[1] == 'i' && ! is_alpha(subpat[2])) {
                    throw invalid_argument("\\i did not specify valid alpha.");
                }
            }
        }
        return subpat_to_set(subpat);
    }
}

}

void aho_corasick_begin(
    Intermediate::Automata& automata
)
{
    if (automata.start_node()) {
        throw invalid_argument("Automata not empty.");
    }
    automata.start_node() = boost::make_shared<ACNode>();
}

void aho_corasick_add_length(
    Intermediate::Automata& automata,
    const string&           s
)
{
    IronAutomata::buffer_t data_buffer;
    IronAutomata::BufferAssembler assembler(data_buffer);
    assembler.append_object(uint32_t(s.length()));

    aho_corasick_add_data(automata, s, data_buffer);
}

void aho_corasick_add_data(
    Intermediate::Automata&            automata,
    const std::string&                 s,
    const Intermediate::byte_vector_t& data
)
{
    if (! automata.start_node()) {
        throw invalid_argument("Automata lacks start node.");
    }
    Intermediate::node_p current_node = automata.start_node();

    size_t j = 0;
    while (j < s.length()) {
        uint8_t c = s[j];

        Intermediate::node_p next_node = find_next(current_node, c);
        if (! next_node) {
            break;
        }
        ++j;

        current_node = next_node;
    }

    while (j < s.length()) {
        uint8_t c = s[j];
        ++j;

        current_node->edges().push_back(Intermediate::Edge());
        Intermediate::Edge& edge = current_node->edges().back();
        edge.target() = boost::make_shared<ACNode>();
        edge.add(c);
        current_node = edge.target();
    }

    boost::static_pointer_cast<ACNode>(current_node)->prepend_output(data);
}

void aho_corasick_add_pattern(
    Intermediate::Automata&            automata,
    const string&                      pattern,
    const Intermediate::byte_vector_t& data
)
{
    list<Intermediate::node_p> current_nodes;
    current_nodes.push_back(automata.start_node());

    size_t pattern_i = 0;

    list<Intermediate::node_p> next_current_nodes;
    Intermediate::byte_vector_t nodshared_cs;
    Intermediate::byte_vector_t shared_cs;
    Intermediate::byte_vector_t orig_cs;
    Intermediate::byte_vector_t cs;
    Intermediate::byte_vector_t new_cs;
    while (! current_nodes.empty() && pattern_i < pattern.length()) {
        // Increments pattern_i appropriately.
        orig_cs = extract_cs(pattern, pattern_i);
        next_current_nodes.clear();

        BOOST_FOREACH(const Intermediate::node_p& node, current_nodes) {
            cs = orig_cs;
            BOOST_FOREACH(Intermediate::Edge& e, node->edges()) {
                if (cs.empty()) {
                    break;
                }

                // shared_cs is the inputs that the pattern and the edge have
                // in common.
                shared_cs.clear();
                set_intersection(
                    cs.begin(), cs.end(),
                    e.begin(), e.end(),
                    back_inserter(shared_cs)
                );
                // If no common inputs, this edge doesn't matter.
                if (shared_cs.empty()) {
                    continue;
                }
                // If matches edge, follow.
                else {
                    new_cs.clear();
                    set_difference(
                        cs.begin(), cs.end(),
                        shared_cs.begin(), shared_cs.end(),
                        back_inserter(new_cs)
                    );
                    cs.swap(new_cs);


                    if (shared_cs.size() == e.size()) {
                        // Edge is subset.
                        next_current_nodes.push_back(e.target());
                        continue;
                    }

                    // Partial overlap.

                    // Add a new edge with shared inputs.  Add it to the
                    // front so we don't consider it later in the loop.
                    node->edges().push_front(split_edge(e, shared_cs));
                    next_current_nodes.push_back(node->edges().front().target());
                }
            }

            // If any inputs remain, make a new edge.
            // Note: This handles building all the way down.
            if (! cs.empty()) {
                node->edges().push_front(
                    Intermediate::Edge::make_from_vector(
                        boost::make_shared<ACNode>(),
                        true,
                        cs
                    )
                );
                next_current_nodes.push_back(node->edges().front().target());
            }
        }

        current_nodes = next_current_nodes;
        if (pattern_i == pattern.length()) {
            // handle outputs
            BOOST_FOREACH(const Intermediate::node_p& node, current_nodes) {
                boost::static_pointer_cast<ACNode>(node)->prepend_output(data);
            }
        }
    }
}


void aho_corasick_finish(
    Intermediate::Automata& automata
)
{
    assert(automata.start_node());
    automata.start_node()->default_target() = automata.start_node();
    automata.start_node()->advance_on_default() = true;

    automata.no_advance_no_output() = true;

    process_failures(automata);
}

} // Generator
} // IronAutomata
