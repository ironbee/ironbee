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
 * @brief IronAutomata --- Deduplicate Outputs Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/deduplicate_outputs.hpp>

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

#include <algorithm>
#include <list>
#include <map>
#include <set>

using namespace std;

namespace IronAutomata {
namespace Intermediate {

namespace  {

//! List of outputs.
typedef list<output_p> outputs_t;

//! Set of outputs.
typedef set<output_p> output_set_t;

//! Map of output to its parents.
typedef map<output_p, outputs_t> parents_t;

//! List of references to outputs.
typedef list<output_p*> output_ref_list_t;

//! Map of output to all references to it.
typedef map<output_p, output_ref_list_t> output_refs_t;

/**
 * Append first output of node to list.
 *
 * @param[in] outputs List to append to.
 * @param[in] node    Node to add first output of.
 */
void append_first_output(outputs_t& outputs, const node_p& node)
{
    if (node->first_output()) {
        outputs.push_back(node->first_output());
    }
}

/**
 * Calculate parent relationship of all outputs.
 *
 * @param[in] parents  Map of output to parents of output to fill.
 * @param[in] automata Automata to calculate for.
 */
void calculate_parents(parents_t& parents, const Automata& automata)
{
    outputs_t todo;
    output_set_t done;

    breadth_first(
        automata,
        boost::bind(append_first_output, boost::ref(todo), _1)
    );

    while (! todo.empty()) {
        output_p output = todo.front();
        todo.pop_front();


        bool added = done.insert(output).second;
        if (! added) {
            continue;
        }

        const output_p& next_output = output->next_output();
        if (next_output) {
            parents[next_output].push_back(output);
            todo.push_back(next_output);
        }
    }
}

/**
 * Add any references to outputs of @a node to @a refs.
 *
 * @param[in] refs Reference map to fill.
 * @param[in] node Node to look for references n.
 */
void calculate_output_refs(output_refs_t& refs, const node_p& node)
{
    if (node->first_output()) {
        output_p* current = &node->first_output();
        while (current) {
            refs[*current].push_back(current);
            if ((*current)->next_output()) {
                current = &(*current)->next_output();
            }
            else {
                current = NULL;
            }
        }
    }
}

//! Lexicographical ordering of Output.
struct less_output :
    binary_function<const Output&, const Output&, bool>
{
    //! Call operator().
    bool operator()(const Output& a, const Output& b) const
    {
        if (a.content() < b.content()) {
            return true;
        }
        else if (a.content() == b.content()) {
            return a.next_output() < b.next_output();
        }

        return false;
    }
};

}

size_t deduplicate_outputs(Automata& automata)
{
    parents_t parents;

    calculate_parents(parents, automata);

    output_refs_t refs;

    breadth_first(
        automata,
        boost::bind(calculate_output_refs, boost::ref(refs), _1)
    );

    output_set_t todo;
    output_set_t next_todo;

    transform(
        refs.begin(), refs.end(),
        inserter(todo, todo.begin()),
        boost::bind(&output_refs_t::value_type::first, _1)
    );

    typedef map<Output, output_p, less_output> canonicals_t;
    canonicals_t canonicals;

    size_t removed = 0;
    while (! todo.empty()) {
        next_todo.clear();
        BOOST_FOREACH(const output_p& output, todo) {
            canonicals_t::iterator canonical_iter = canonicals.find(*output);
            if (canonical_iter == canonicals.end()) {
                canonicals[*output] = output;
            }
            else if (canonical_iter->second != output) {
                ++removed;
                // Update references.
                BOOST_FOREACH(output_p* ref, refs[output]) {
                    *ref = canonical_iter->second;
                }

                // Add parents to next_todo.
                const outputs_t& output_parents = parents[output];
                copy(
                    output_parents.begin(), output_parents.end(),
                    inserter(next_todo, next_todo.begin())
                );
            }
        }

        todo = next_todo;
    }

    return removed;
}

} // Intermediate
} // IronAutomata
