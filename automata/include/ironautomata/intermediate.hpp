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

#ifndef _IA_INTERMEDIATE_HPP_
#define _IA_INTERMEDIATE_HPP_

/**
 * @file
 * @brief IronAutomata --- Intermediate Format
 *
 * @sa IronAutomataIntermediate
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.pb.h>
#include <ironautomata/logger.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <list>
#include <string>

#include <sys/types.h>

namespace IronAutomata {

/**
 * @namespace IronAutomata::Intermediate
 * Code relating to intermediate format.
 *
 * This namespace defines an in memory representation of the intermediate
 * format (automata_t, node_t, edge_t, output_t), related types (node_p,
 * edge_p, output_p, string_p), and code related to this format (e.g.,
 * read from protobuf).
 */
namespace Intermediate {

//! Node or output identifier type.
typedef uint64_t id_t;

struct edge_t;
struct node_t;
struct output_t;

//! Shared pointer to node.
typedef boost::shared_ptr<node_t> node_p;

//! Shared pointer to edge.
typedef boost::shared_ptr<edge_t> edge_p;

//! Shared pointer to output.
typedef boost::shared_ptr<output_t> output_p;

/**
 * An edge in the automata.
 *
 * @note Default edges are represented directly in node_t, not by an edge.
 *
 * @note An edge with no values should be followed on all inputs, i.e., an
 *       epsilon edge.
 */
struct edge_t
{
    //! Target.
    node_p target;
    //! Advance input on following.
    bool advance;
    /**
     * Bitmap of which values edge represents.
     *
     * If non-empty, then must be exactly 32 bytes and @c values must be
     * empty.
     */
    std::vector<uint8_t> values_bm;

    /**
     * Sequence of values edge represents.
     *
     * If non-empty, then values_bm must be empty.
     */
    std::vector<uint8_t> values;
};

/**
 * A node in the automata.
 *
 * Note: Implicit extra edge to @c default_target if non-singular.
 */
struct node_t
{
    //! First output.
    output_p output;

    //! Default target.
    node_p default_target;

    //! Advance input on default.
    bool advance_on_default;

    //! List of edges.
    std::list<edge_t> edges;
};

/**
 * An output.
 */
struct output_t
{
    //! Content.
    std::vector<uint8_t> content;

    //! Next output.
    output_p next_output;
};

//! Automata.
struct automata_t
{
    //! Starting node.
    node_p start_node;

    //! If true, no output for targets of non-advancing edges.
    bool no_advance_no_output;
};

/**
 * Read a chunk from a stream.
 *
 * @param[in]  input Stream to read from.
 * @param[out] chunk Chunk to write to.
 * @return true if a chunk was read and false on EOF.
 * @throw runtime_error on error.
 */
bool read_chunk(std::istream& input, PB::Chunk& chunk)

/**
 * Write a chunk to a stream.
 *
 * @param[in] output Stream to write to.
 * @param[in] chunk  Chunk to write.
 * @throw runtime_error on error.
 */
void write_chunk(std::ostream& output, PB::Chunk& chunk);

/**
 * Read automata from protobuf.
 *
 * This class can be used to load an automata from an istream.  A simpler
 * interface is available via read_automata().  The advantages of this class
 * are the ability to query success() (no errors) and clean() (no errors or
 * warnings).
 *
 * @note This class is likely to evolve over time to provide better support
 * for streaming, loading from memory, etc.
 *
 * @sa read_automata()
 */
class AutomataReader
{
public:
    /**
     * Constructor.
     *
     * @param[in] logger Logger to use.
     */
    explicit
    AutomataReader(logger_t logger = nop_logger);

    /**
     * Load automata from istream.
     *
     * Istream is expected to be a sequence of size/data pairs where size is
     * a 32 bit network order unsigned int describing the number of bytes
     * in the following data message.  The data is expected to be a gzipped
     * Data protobuf message (see intermediate.proto
     ).
     *
     * At present, the result of calling this multiple times is undefined.
     *
     * @param[in] input Istream to read from.
     * @return success()
     */
    bool read_from_istream(std::istream& input);

    /**
     * True iff no error occurred in reading.
     *
     * True if read_from_istream() has not been called.
     *
     * @return True iff no error has occurred.
     */
    bool success() const;

    /**
     * True iff no warning or error occurred in reading.
     *
     * True if read_from_istream() has not been called.
     *
     * @return True iff no error or warning has occurred.
     */
    bool clean() const;

    /**
     * Read automata.
     *
     * Returns an empty automata if read_from_istream() has not been called.
     *
     * Note: Returns const reference, but as an automata_t is just a
     * node_p to the start node, can cheaply be copied if mutation is needed.
     */
    const automata_t& automata() const;

private:
    //! Opaque type of internal data.
    struct AutomataReaderImpl;
    //! Internal data.
    boost::shared_ptr<AutomataReaderImpl> m_impl;
};

/**
 * Simple wrapper of AutomataReader.
 *
 * Equivalent to:
 * @code
 * AutomataReader reader(logger);
 * reader.read_form_istream(input);
 * destination = reader.automata();
 * return reader.success()
 * @endcode
 *
 * That is, it reads an automata to @a destination from @a input, using @a
 * logger and returns true if no errors occurred.
 *
 * @param[out] destination Automata read.
 * @param[in]  input       Istream to read automata from.  See AutomataReader.
 * @param[in]  logger      Logger to use; defaults to nop_logger.
 * @return true iff no error occurred.
 *
 * @sa AutomataReader
 */
bool read_automata(
    automata_t&   destination,
    std::istream& input,
    logger_t      logger = nop_logger
);

/**
 * Breadth first traversal of an automata.
 *
 * Calls @a callback with each node in breadth first order.  Edge order is
 * defined by value with default edge being last.
 *
 * @param[in] automata Automata to traverse.
 * @param[in] callback Callback to call for each node.
 */
void breadth_first(
    const automata_t&                    automata,
    boost::function<void(const node_p&)> callback
);

/**
 * Iterator through all values of an edge.
 *
 * This class provides a forward non-mutable input iterator that iterates
 * through every value of an edge.
 *
 * @note The reference value of this iterator is a copy, not a reference.
 *
 * @sa edge_values()
 */
class edge_value_iterator :
    public boost::iterator_facade<
        edge_value_iterator,
        uint8_t,
        boost::forward_traversal_tag,
        uint8_t
    >
{
public:
    //! Default/end constructor.
    edge_value_iterator();

    //! Begin constructor.
    explicit
    edge_value_iterator(const edge_t& edge);

private:
    friend class boost::iterator_core_access;

    void increment();
    bool equal(const edge_value_iterator& other) const;
    uint8_t dereference() const;

    const edge_t* m_edge;
    std::vector<uint8_t>::const_iterator m_string_i;
    size_t m_bitmap_i;
};

/**
 * Construct begin and end edge value iterator for an edge.
 *
 * @param[in] edge Edge to iterator over.
 * @return Pair of begin and end iterators.
 */
std::pair<edge_value_iterator, edge_value_iterator> edge_values(
     const edge_t& edge
);

} // Intermediate
} // IronAutomata

#endif
