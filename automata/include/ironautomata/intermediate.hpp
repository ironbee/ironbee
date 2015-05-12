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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include <ironautomata/intermediate.pb.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ironautomata/logger.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/iterator/iterator_facade.hpp>
#include <boost/shared_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

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
 * format (Automata, node_t, edge_t, output_t), related types (node_p,
 * edge_p, output_p, string_p), and code related to this format (e.g.,
 * read from protobuf).
 */
namespace Intermediate {

class Edge;
class Node;
class Output;

//! Shared pointer to node.
typedef boost::shared_ptr<Node> node_p;

//! Shared pointer to edge.
typedef boost::shared_ptr<Edge> edge_p;

//! Shared pointer to output.
typedef boost::shared_ptr<Output> output_p;

//! Vector of bytes.
typedef std::vector<uint8_t> byte_vector_t;

/**
 * An edge in the automata.
 *
 * @note Default edges are represented directly in Node, not by an edge.
 *
 * Edges have two possible internal representations for their values.  They
 * may store values as a vector of values or as a 256-bit bitmap with 1s
 * representing values.  Usually, edges with less than 32 values use vectors
 * and those with 32 or more values use bitmaps.
 *
 * @note An edge with no values is called an epsilon edge and matches any
 *       input.
 */
class Edge
{
public:
    friend class const_iterator;

    /**
     * Value iterator.
     *
     * This class provides a forward non-mutable input iterator that iterates
     * through every value of an edge.
     *
     * @note The reference value of this iterator is a copy, not a reference.
     */
    class const_iterator :
        public boost::iterator_facade<
            const_iterator,
            uint8_t,
            boost::forward_traversal_tag,
            uint8_t
        >
    {
    public:
        //! Default/end constructor.
        const_iterator();

        //! Begin constructor.
        explicit
        const_iterator(const Edge& edge);

    private:
        friend class boost::iterator_core_access;

        void increment();
        bool equal(const const_iterator& other) const;
        uint8_t dereference() const;

        const Edge* m_edge;
        std::vector<uint8_t>::const_iterator m_string_i;
        size_t m_bitmap_i;
    };

    //! Iterators are the same as const iterators.
    typedef const_iterator iterator;

    //! Value type for iteration.
    typedef const_iterator::value_type value_type;

    //! Default Constructor.
    Edge();

    /**
     * Constructor.
     *
     * Constructs as epsilon (valueless) edge, but values can be added with
     * add().
     *
     * @param[in] target  Target.
     * @param[in] advance True if input should be advanced when followed.
     */
    explicit
    Edge(
        const node_p&  target,
        bool           advance = true
    );

    /**
     * Create an edge directly from a vector of values.
     *
     * Behavior is undefined if values contains duplicates.
     *
     * @param[in] target  Target.
     * @param[in] advance True if input should be advanced when followed.
     * @param[in] values  Vector of bytes to match.
     * @return Edge
     */
    static
    Edge make_from_vector(
        const node_p&        target,
        bool                 advance,
        const byte_vector_t& values
    );

    /**
     * Create an edge directly from a bitmap of values.
     *
     * @param[in] target  Target.
     * @param[in] advance True if input should be advanced when followed.
     * @param[in] bitmap  256 bit, 32 byte, vector treated as bitmap of which
     *                    values to match.
     * @return Edge
     * @throw logic_error if @a bitmap is not 32 bytes.
     */
    static
    Edge make_from_bitmap(
        const node_p&        target,
        bool                 advance,
        const byte_vector_t& bitmap
    );

    //! Target accessor.
    const node_p& target() const
    {
        return m_target;
    }
    //! Target accessor.
    node_p& target()
    {
        return m_target;
    }

    //! Advance accessor.
    bool advance() const
    {
        return m_advance;
    }
    //! Advance accessor.
    bool& advance()
    {
        return m_advance;
    }

    //! Beginning of values.  Vector: O(1) Bitmap: O(n)
    iterator begin() const;
    //! End of values. O(1)
    iterator end() const;

    //! Number of values. Vector: O(1) Bitmap: O(n)
    size_t size() const;
    //! Are there any values? Synonym for epsilon(). Vector: O(1) Bitmap: O(n)
    bool empty() const;

    /**
     * Is @a c in the values? Vector: O(n) Bitmap: O(1)
     *
     * This method returns false if epsilon().  In contrast, matches() returns
     * true if epsilon().  Otherwise, they are the same.
     *
     * @param[in] c Input to look for.
     * @return true iff @a c is a value of this edge.
     */
    bool has_value(uint8_t c) const;

    /**
     * Does this edge match @a c? Vector: O(n) Bitmap: O(1)
     *
     * @param[in] c Input to check.
     * @return epsilon() || has_value(c)
     */
    bool matches(uint8_t c) const;

    /**
     * Is this an epsilon edge? Synonym for empty(). Vector: O(1) Bitmap: O(n)
     *
     * Epsilon edges match any input.
     */
    bool epsilon() const;

    //! Add value @a c. Vector: O(n) Bitmap: O(1)
    void add(uint8_t c);
    //! Remove value @a c. Vector: O(n) Bitmap: O(1)
    void remove(uint8_t c);

    /**
     * Force internal representation to bitmap. O(n)
     *
     * Call if you want to use bitmap() later.  Once an edge uses bitmaps, it
     * will keep using bitmaps unless switch_to_vector() is called.
     */
    void switch_to_bitmap();

    /**
     * Force internal representation of vector. O(n)
     *
     * Call if you want to use vector() later.  Calling add() may change edge
     * back to a bitmap.
     */
    void switch_to_vector();

    //! Values as bitmap.  Will be empty if edge in vector representation.
    const byte_vector_t& bitmap() const
    {
        return m_bitmap;
    }

    //! Values as vector.  Will be empty if edge in bitmap representation.
    const byte_vector_t& vector() const
    {
        return m_vector;
    }

    //! Values as bitmap.  Will be empty if edge in vector representation.
    byte_vector_t& bitmap()
    {
        return m_bitmap;
    }

    //! Values as vector.  Will be empty if edge in bitmap representation.
    byte_vector_t& vector()
    {
        return m_vector;
    }

    //! Clear
    void clear();

    //! Swap
    void swap(Edge& other);

private:
    node_p m_target;
    bool m_advance;

    byte_vector_t m_vector;
    byte_vector_t m_bitmap;
};

/**
 * A node in the automata.
 *
 * Note: Implicit extra edge to @c default_target if non-singular.
 */
class Node
{
public:
    //! List of edges.
    typedef std::list<Edge> edge_list_t;

    //! Constructor.
    explicit
    Node(bool advance_on_default = true);

    //! First Output accessor.
    const output_p& first_output() const
    {
        return m_first_output;
    }
    //! First Output accessor.
    output_p& first_output()
    {
        return m_first_output;
    }

    //! Default Target accessor.
    const node_p& default_target() const
    {
        return m_default_target;
    }
    //! Default Target accessor.
    node_p& default_target()
    {
        return m_default_target;
    }

    //! Advance on default accessor.
    bool advance_on_default() const
    {
        return m_advance_on_default;
    }
    //! Advance on default accessor.
    bool& advance_on_default()
    {
        return m_advance_on_default;
    }

    //! Edges accessor.
    const edge_list_t& edges() const
    {
        return m_edges;
    }
    //! Edges accessor.
    edge_list_t& edges()
    {
        return m_edges;
    }

    /**
     * Find all edges for a given input.
     *
     * If result is empty, use default.
     *
     * @param[in] c Input.
     * @return List of edges to follow for @a c.
     */
    edge_list_t edges_for(uint8_t c) const;

    //! Next node/advance pair.
    typedef std::pair<node_p, bool> target_info_t;

    //! List of target infos.
    typedef std::list<target_info_t> target_info_list_t;

    /**
     * Find all targets for a given input.
     *
     * @param[in] c Input.
     * @return list of next nodes and inputs for @a c.
     */
    target_info_list_t targets_for(uint8_t c) const;

    //! Map of input to targets.
    typedef std::vector<target_info_list_t> targets_by_input_t;

    /**
     * Construct a map of input to targets.
     *
     * This method is equivalent to:
     * @code
     * targets_by_input_t result(256);
     * for (int c = 0; c < 256; ++c) {
     *     result[c] = targets_for(c);
     * }
     * return result;
     * @endcode
     *
     * However, it is significantly faster.
     */
    targets_by_input_t build_targets_by_input() const;

    //! Clear node.
    void clear();

    //! Swap.
    void swap(Node& other);

private:
    //! First output.
    output_p m_first_output;

    //! Default target.
    node_p m_default_target;

    //! Advance input on default.
    bool m_advance_on_default;

    //! Out edges.
    edge_list_t m_edges;
};

/**
 * An output.
 */
class Output
{
public:
    //! Default constructor.
    Output();

    //! Construct from byte vector.
    explicit
    Output(
        const byte_vector_t& content,
        const output_p&      next_output = output_p()
    );

    //! Construct from string.
    explicit
    Output(
        const std::string& content,
        const output_p&    next_output = output_p()
    );

    //! Content accessor.
    const byte_vector_t& content() const
    {
        return m_content;
    }
    //! Content accessor.
    byte_vector_t& content()
    {
        return m_content;
    }

    //! Next output accessor.
    const output_p& next_output() const
    {
        return m_next_output;
    }
    //! Next output accessor.
    output_p& next_output()
    {
        return m_next_output;
    }

private:
    //! Content.
    std::vector<uint8_t> m_content;

    //! Next output.
    output_p m_next_output;
};

//! Automata.
class Automata
{
public:
    //! Constructor.
    explicit
    Automata(bool no_advance_no_output = false);

    //! Start node accessor.
    const node_p& start_node() const
    {
        return m_start_node;
    }
    //! Start node accessor.
    node_p& start_node()
    {
        return m_start_node;
    }

    //! No advance no output accessor.
    bool no_advance_no_output() const
    {
        return m_no_advance_no_output;
    }
    //! No advance no output accessor.
    bool& no_advance_no_output()
    {
        return m_no_advance_no_output;
    }

    //! Metadata map accessor.
    const std::map<std::string, std::string>& metadata() const
    {
        return m_metadata;
    }
    //! Metadata map accessor.
    std::map<std::string, std::string>& metadata()
    {
        return m_metadata;
    }
private:
    //! Starting node.
    node_p m_start_node;

    //! If true, no output for targets of non-advancing edges.
    bool m_no_advance_no_output;

    //! Metadata map.
    std::map<std::string, std::string> m_metadata;
};

/**
 * Read a chunk from a stream.
 *
 * @param[in]  input Stream to read from.
 * @param[out] chunk Chunk to write to.
 * @return true if a chunk was read and false on EOF.
 * @throw runtime_error on error.
 */
bool read_chunk(std::istream& input, PB::Chunk& chunk);

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
     * Note: Returns const reference, but as an Automata is just a
     * node_p to the start node, can cheaply be copied if mutation is needed.
     */
    const Automata& automata() const;

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
    Automata&     destination,
    std::istream& input,
    logger_t      logger = nop_logger
);

/**
 * Write an automata.
 *
 * The write interface is significantly simpler than the read interface as it
 * does significantly less validation.
 *
 * @param[in] automata   Automata to write.
 * @param[in] output     Stream to write to.
 * @param[in] chunk_size If non-0, no chunk will contain more than
 *                       @a chunk_size nodes and outputs.
 * @throw runtime_error on write error.
 * @throw invalid_argument if @a automata is invalid.
 */
void write_automata(
    const Automata& automata,
    std::ostream&   output,
    size_t          chunk_size = 0
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
    const Automata&                      automata,
    boost::function<void(const node_p&)> callback
);

} // Intermediate
} // IronAutomata

#endif
