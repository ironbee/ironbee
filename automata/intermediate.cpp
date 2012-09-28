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
 * @brief IronAutomata --- Intermediate Format Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.hpp>
#include <ironautomata/bits.h>

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_array.hpp>

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <algorithm>
#include <list>
#include <map>
#include <queue>
#include <set>

#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

namespace IronAutomata {
namespace Intermediate {

bool read_chunk(istream& input, PB::Chunk& chunk)
{
    if (input.bad()) {
        throw runtime_error("Input in bad state.");
    }

    uint32_t raw_message_size = 0;
    input.read(
        reinterpret_cast<char*>(&raw_message_size),
        sizeof(uint32_t)
    );
    if (! input) {
        if (input.eof()) {
            // Hit EOF
            return false;
        }
        else {
            throw runtime_error("Input in bad state.");
        }
    }

    uint32_t message_size = ntohl(raw_message_size);
    boost::scoped_array<char> buffer(new char[message_size]);

    input.read(buffer.get(), message_size);
    if (input.fail()) {
        throw runtime_error("Failure reading chunk.");
    }

    google::protobuf::io::ArrayInputStream in(buffer.get(), message_size);
    google::protobuf::io::GzipInputStream unzipped_in(&in);

    if (! chunk.ParseFromZeroCopyStream(&unzipped_in)) {
        throw runtime_error("Failure parsing chunk.");
    }
    if (unzipped_in.ZlibErrorMessage()) {
        throw runtime_error((boost::format(
            "Failed to decompress: %s"
            ) % unzipped_in.ZlibErrorMessage()
        ).str());
    }

    return true;
}

void write_chunk(ostream& output, PB::Chunk& chunk)
{
    if (! output) {
        throw runtime_error("Bad output.");
    }
    string buffer;
    google::protobuf::io::StringOutputStream buffer_s(&buffer);
    google::protobuf::io::GzipOutputStream zipped_output(&buffer_s);

    chunk.SerializeToZeroCopyStream(&zipped_output);
    zipped_output.Close();

    uint32_t size = buffer.length();
    uint32_t nsize = htonl(size);

    output.write(
        reinterpret_cast<const char*>(&nsize), sizeof(uint32_t)
    );
    if (! output) {
        throw runtime_error("Error writing header.");
    }
    output.write(buffer.data(), size);
    if (! output) {
        throw runtime_error("Error writing chunk.");
    }
}

/**
 * Implementation of AutomataReader.
 */
struct AutomataReader::AutomataReaderImpl
{
    /**
     * Constructor.
     *
     * @param[in] logger Logger to use.
     */
    explicit
    AutomataReaderImpl(logger_t logger);

    /**
     * Load automata from istream.
     *
     * Decodes istream into series of Data messages and calls process_chunk()
     * on them and then calls finish().
     *
     * @param[in] input Istream to read from.
     */
    void read_from_istream(istream& input);

    /**
     * Process a data message.
     *
     * Calls process_graph(), process_node(), process_edge(), and
     * process_output() appropriately.
     *
     * @param[in] pb_chunk PB Data message.
     */
    void process_chunk(const PB::Chunk& pb_chunk);

    /**
     * Process a graph message.
     *
     * @param[in] pb_graph PB Graph message.
     */
    void process_graph(const PB::Graph& pb_graph);

    /**
     * Process a node message.
     *
     * @param[in] pb_node PB Node message.
     */
    void process_node(const PB::Node& pb_node);

    /**
     * Process an edge message.
     *
     * @param[in] source  Source node.
     * @param[in] pb_edge PB Edge message.
     */
    void process_edge(const node_p& source, const PB::Edge& pb_edge);

    /**
     * Process an output message.
     *
     * @param[in] pb_output PB Output message.
     */
    void process_output(const PB::Output& pb_output);

    /**
     * Finish the automata.  Called after all messages processed.
     *
     * Does a variety of checks, e.g., for references by unfilled ids, and
     * sets up the start node.
     */
    void finish();

    /**
     * Report an error.
     *
     * Calls the logger with an error, the current message number, and @a
     * what.  Also sets m_success and m_clean to false.
     *
     * @param[in] what Message.
     */
    void error(const string& what);

    /**
     * Report warning.
     *
     * Calls the logger with a warning, the current message number, and @a
     * what.  Also sets m_clean to false.
     *
     * @param[in] what Message.
     */
    void warn(const string& what);

    /**
     * A set of IDs.
     */
    typedef set<id_t> id_set_t;

    /**
     * Check for and set an ID as filled.
     *
     * @param[in] id_set Set of ids.
     * @param[in] id     ID to add to set.
     * @return true iff @a id was already in @a id_set.
     */
    static
    bool fill_id(id_set_t& id_set, id_t id);

    /**
     * Find or create an object in an id to object pointer map.
     *
     * @tparam T Type of object pointed to.
     * @param[in] map Map to find in/add to.
     * @param[in] id  ID of object to find or add.
     * @return Pointer to found/added object.
     */
    template <typename T>
    static
    boost::shared_ptr<T>& find_or_create_by_id(
        std::map<id_t, boost::shared_ptr<T> >& map,
        id_t                                   id
    );

    /**
     * Report issues for any ids in of @a a - @a b.
     *
     * For every id in the set @a a and not in the set @a b, will issue an
     * error or warning with a message of @a prefix + " " + id + " " +
     * @a suffix.
     *
     * @param[in] a          First set.
     * @param[in] b          Second set.
     * @param[in] prefix     Pre-id part of message.
     * @param[in] suffix     Post-id part of message.
     * @param[in] is_warning If true, issues warnings, else errors.
     */
    void check_id_list(
        const id_set_t &a,
        const id_set_t &b,
        const string& prefix,
        const string& suffix,
        bool is_warning
    );

    /**
     * Type of m_node_map.
     */
    typedef map<id_t, node_p>   node_map_t;

    /**
     * Type of m_output_map.
     */
    typedef map<id_t, output_p> output_map_t;

    //! Logger passed in to constructor.
    logger_t m_logger;

    //! Automata being constructor.
    automata_t m_automata;

    //! True iff error() never called.
    bool m_success;

    //! True iff error() and warn() never called.
    bool m_clean;

    //! Data message number, i.e., number of call to process_chunk().
    int m_chunk_number;

    //! ID of start node.
    id_t m_start_node_id;

    /**
     * @name Nodes and Outputs
     *
     * The following members record the nodes and outputs.  A node/output is
     * @e filled if it has appeared in a node or output message, respectively.
     * It is @e referenced it another node, output, or edge has referenced it
     * by id.  The members m_node_map and m_output_map hold the actual node_p
     * and output_p objects.  These objects are added to the maps the first
     * time they are filled or referenced.  The sets m_node_ids_filled,
     * m_node_ids_referenced, m_output_ids_filled, and m_output_ids_referenced
     * record which ids have been filled and which have been referenced and
     * are used to detect issues such as duplicates and dangling references.
     */
    ///@{

    //! Nodes
    node_map_t   m_node_map;

    //! Outputs
    output_map_t m_output_map;

    //! Node IDs filled.
    id_set_t     m_node_ids_filled;

    //! Node IDs referenced.
    id_set_t     m_node_ids_referenced;

    //! Output IDs filled.
    id_set_t     m_output_ids_filled;

    //! Output IDs referenced.
    id_set_t     m_output_ids_referenced;

    ///@}
};

bool AutomataReader::AutomataReaderImpl::fill_id(id_set_t& id_set, id_t id)
{
    return ! id_set.insert(id).second;
}

template <typename T>
boost::shared_ptr<T>& AutomataReader::AutomataReaderImpl::find_or_create_by_id(
    std::map<id_t, boost::shared_ptr<T> >& map,
    id_t id
)
{
    typedef std::map<id_t, boost::shared_ptr<T> > map_t;
    typename map_t::iterator i = map.find(id);
    if (i == map.end()) {
        i = map.insert(make_pair(id, boost::make_shared<T>())).first;
    }

    return i->second;
}

void AutomataReader::AutomataReaderImpl::read_from_istream(istream& input)
{
    PB::Chunk pb_chunk;
    while (input) {
        bool at_eof = false;
        try {
            at_eof = ! read_chunk(input, pb_chunk);
        }
        catch (const runtime_error& e) {
            error(e.what());
        }
        if (at_eof) {
            break;
        }

        ++m_chunk_number;

        process_chunk(pb_chunk);
    }

    finish();
}

AutomataReader::AutomataReaderImpl::AutomataReaderImpl(logger_t logger) :
    m_logger(logger),
    m_success(true),
    m_clean(true),
    m_chunk_number(0),
    m_start_node_id(0)
{
    m_automata.no_advance_no_output = false;
}

void AutomataReader::AutomataReaderImpl::error(const string& what)
{
    m_logger(
        IA_LOG_ERROR,
        (boost::format("Data #%d") % m_chunk_number).str(),
        what
    );
    m_clean   = false;
    m_success = false;
}

void AutomataReader::AutomataReaderImpl::warn(const string& what)
{
    m_logger(
        IA_LOG_WARN,
        (boost::format("Data #%d") % m_chunk_number).str(),
        what
    );
    m_clean = false;
}

void AutomataReader::AutomataReaderImpl::process_graph(
    const PB::Graph& pb_graph
)
{
    if (pb_graph.has_no_advance_no_output()) {
        m_automata.no_advance_no_output = pb_graph.no_advance_no_output();
    }
}

void AutomataReader::AutomataReaderImpl::process_chunk(
    const PB::Chunk& pb_chunk
)
{
    if (pb_chunk.has_graph()) {
        process_graph(pb_chunk.graph());
    }

    BOOST_FOREACH(const PB::Output& pb_output, pb_chunk.outputs()) {
        process_output(pb_output);
    }

    BOOST_FOREACH(const PB::Node& pb_node, pb_chunk.nodes()) {
        process_node(pb_node);
    }
}

void AutomataReader::AutomataReaderImpl::process_output(const PB::Output& pb_output)
{
    bool already_filled = fill_id(m_output_ids_filled, pb_output.id());
    if (already_filled) {
        warn((boost::format(
            "Duplicate output [id=%d].  Ignoring.")
            % pb_output.id()
        ).str());
        return;
    }

    output_p& output = find_or_create_by_id(m_output_map, pb_output.id());
    output->content.reserve(pb_output.content().size());
    output->content.insert(
        output->content.begin(),
        pb_output.content().begin(), pb_output.content().end()
    );
    if (pb_output.has_next() && pb_output.next() != 0) {
        output_p& next_output =
            find_or_create_by_id(m_output_map, pb_output.next());
        output->next_output = next_output;
        m_output_ids_referenced.insert(pb_output.next());
    }
}

void AutomataReader::AutomataReaderImpl::process_node(const PB::Node& pb_node)
{
    bool already_filled = fill_id(m_node_ids_filled, pb_node.id());
    if (already_filled) {
        warn((boost::format(
            "Duplicate node [id=%d]. Ignoring."
            ) % pb_node.id()
        ).str());
        return;
    }

    if (m_start_node_id == 0) {
        m_start_node_id = pb_node.id();
    }

    node_p& node = find_or_create_by_id(m_node_map, pb_node.id());

    if (pb_node.has_first_output() && pb_node.first_output() != 0) {
        output_p& output =
            find_or_create_by_id(m_output_map, pb_node.first_output());
        m_output_ids_referenced.insert(pb_node.first_output());
        node->output = output;
    }

    if (pb_node.has_default_target()) {
        node_p& target =
            find_or_create_by_id(m_node_map, pb_node.default_target());
        m_node_ids_referenced.insert(pb_node.default_target());
        node->default_target = target;
    }
    node->advance_on_default = (
        pb_node.has_advance_on_default() ?
        pb_node.advance_on_default() :
        true
    );

    BOOST_FOREACH(const PB::Edge& pb_edge, pb_node.edges()) {
        process_edge(node, pb_edge);
    }
}

void AutomataReader::AutomataReaderImpl::process_edge(
    const node_p&   source,
    const PB::Edge& pb_edge
)
{
    node_p& target =
        find_or_create_by_id(m_node_map, pb_edge.target());
    m_node_ids_referenced.insert(pb_edge.target());

    // Most validation of edges is handled once all data is loaded.
    source->edges.push_back(edge_t());
    edge_t& edge = source->edges.back();
    edge.target = target;
    edge.advance =
        (pb_edge.has_advance() ? pb_edge.advance() : true);
    if (pb_edge.has_values_bm()) {
        if (pb_edge.values_bm().size() != 32) {
            warn((boost::format(
                "Edge values bitmap is wrong size.  "
                "Expected 32, was %d."
            ) % pb_edge.values_bm().size()).str());
        }
        edge.values_bm.reserve(pb_edge.values_bm().size());
        edge.values_bm.insert(
            edge.values_bm.begin(),
            pb_edge.values_bm().begin(), pb_edge.values_bm().end()
        );
        if (pb_edge.has_values()) {
            warn((boost::format(
                "Edge to %d has both values bitmap and list"
                " list.  Ignoring list."
                ) % pb_edge.target()
            ).str());
        }
    }
    else {
        if (pb_edge.has_values()) {
            edge.values.reserve(pb_edge.values().size());
            edge.values.insert(
                edge.values.begin(),
                pb_edge.values().begin(), pb_edge.values().end()
            );
        }
        // else epsilon (follow on any value) edge.
    }
}

void AutomataReader::AutomataReaderImpl::check_id_list(
    const id_set_t &a,
    const id_set_t &b,
    const string& prefix,
    const string& suffix,
    bool is_warning
)
{
    list<id_t> ids;

    set_difference(
        a.begin(), a.end(),
        b.begin(), b.end(),
        back_inserter(ids)
    );
    BOOST_FOREACH(const id_t& id, ids) {
        const string message = (boost::format(
            "%s %d %s."
        ) % prefix % id % suffix).str();
        if (is_warning) {
            warn(message);
        }
        else {
            error(message);
        }
    }

}
void AutomataReader::AutomataReaderImpl::finish()
{
    if (m_start_node_id != 0) {
        m_node_ids_referenced.insert(m_start_node_id);
    }

    check_id_list(
        m_node_ids_referenced, m_node_ids_filled,
        "Node ID",
        "referenced but never defined.",
        false
    );
    check_id_list(
        m_output_ids_referenced, m_output_ids_filled,
        "Output ID",
        "referenced but never defined.",
        false
    );
    check_id_list(
        m_node_ids_filled, m_node_ids_referenced,
        "Node ID",
        "defined but never referenced.",
        true
    );
    check_id_list(
        m_output_ids_filled, m_output_ids_referenced,
        "Output ID",
        "defined but never referenced.",
        true
    );

    if (m_start_node_id != 0) {
        node_map_t::iterator start_nmi = m_node_map.find(m_start_node_id);
        if (start_nmi == m_node_map.end()) {
            error((boost::format(
                "Error: Start node id is %d but no such node."
                ) % m_start_node_id
            ).str());
        }
        else {
            m_automata.start_node = start_nmi->second;
        }
    }
}

AutomataReader::AutomataReader(logger_t logger) :
    m_impl(new AutomataReader::AutomataReaderImpl(logger))
{
    // nop
}

bool AutomataReader::read_from_istream(istream& input)
{
    m_impl->read_from_istream(input);
    return m_impl->m_success;
}

bool AutomataReader::success() const
{
    return m_impl->m_success;
}

bool AutomataReader::clean() const
{
    return m_impl->m_clean;
}

const automata_t& AutomataReader::automata() const
{
    return m_impl->m_automata;
}

bool read_automata(
    automata_t& destination,
    istream& input,
    logger_t logger
)
{
    AutomataReader reader(logger);
    reader.read_from_istream(input);
    destination = reader.automata();
    return reader.success();
}

void breadth_first(
    const automata_t&                    automata,
    boost::function<void(const node_p&)> callback
)
{
    typedef set<node_p> node_p_set_t;
    node_p_set_t queued;
    typedef queue<node_p> todo_t;
    todo_t todo;

    if (! automata.start_node) {
        return;
    }

    todo.push(automata.start_node);
    queued.insert(automata.start_node);

    while (! todo.empty()) {
        node_p node = todo.front();
        todo.pop();

        callback(node);

        BOOST_FOREACH(const edge_t& edge, node->edges) {
            const node_p& target = edge.target;
            bool need_to_queue = queued.insert(target).second;
            if (need_to_queue) {
                todo.push(target);
            }
        }
        if (node->default_target) {
            const node_p& target = node->default_target;
            bool need_to_queue = queued.insert(target).second;
            if (need_to_queue) {
                todo.push(target);
            }
        }
    }
}

namespace {

size_t find_one(const vector<uint8_t>& bm, size_t i)
{
    size_t j = i;
    while (j < bm.size() * 8 && ! ia_bitv(bm.data(), j)) {
        ++j;
    }

    return j;
}

} // Anonymous

edge_value_iterator::edge_value_iterator() :
    m_edge(NULL)
{
    // nop
}

edge_value_iterator::edge_value_iterator(const edge_t& edge) :
    m_edge(&edge)
{
    if (m_edge->values.empty()) {
        m_bitmap_i = find_one(m_edge->values_bm, 0);
    }
    else {
        m_string_i = m_edge->values.begin();
    }
}

void edge_value_iterator::increment()
{
    if (! m_edge) {
        return;
    }
    if (m_edge->values.empty()) {
        m_bitmap_i = find_one(m_edge->values_bm, m_bitmap_i + 1);
        if (m_bitmap_i > 255) {
            m_edge = NULL;
        }
    }
    else {
        ++m_string_i;
        if (m_string_i == m_edge->values.end()) {
            m_edge = NULL;
        }
    }
}

bool edge_value_iterator::equal(const edge_value_iterator& other) const
{
    if (! m_edge && ! other.m_edge) {
        return true;
    }
    if (m_edge != other.m_edge) {
        return false;
    }
    if (m_edge->values.empty()) {
        return m_bitmap_i == other.m_bitmap_i;
    }
    return m_string_i == other.m_string_i;
}

uint8_t edge_value_iterator::dereference() const
{
    if (! m_edge) {
        return 0;
    }
    if (m_edge->values.empty()) {
        return m_bitmap_i;
    }
    else {
        return *m_string_i;
    }
}

std::pair<edge_value_iterator, edge_value_iterator> edge_values(
     const edge_t& edge
)
{
    return make_pair(
        edge_value_iterator(edge),
        edge_value_iterator()
    );
}

} // Intermediate
} // IronAutomata
