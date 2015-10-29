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
 * @brief IronBee --- CLIPP Burp Generator.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */


#include <burp_generator.hpp>
#include <clipp/input.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/throw_exception.hpp>

#include <stdexcept>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunknown-attributes")
#pragma clang diagnostic ignored "-Wunknown-attributes"
#endif
#endif
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif


#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/algorithm/string/predicate.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace IronBee {
namespace CLIPP {

namespace {

/**
 * This class handles processing Burp proxy XML files.
 *
 * It is responcible for allocating and destroying C objects
 * and breaking up the XML file processing into relevant parts.
 */
class BurpProcessor {

public:
    /**
     * Constructor for a BurpProcessor.
     *
     * @param[in] file XML input file.
     * @throws std::runtime_error on any failure.
     */
    BurpProcessor(const char *file);

    /**
     * Process the next <item> tag of the Burp proxy xml file.
     *
     * This opens a new connection for each <item>. Then records
     * the raw <request> and <response> contents.
     *
     * @param[in] input The input to have events recorded to.
     * @returns True if an <item> tag was processed. False otherwise.
     */
    bool operator()(Input::input_p& input);

    //! Called by operator() to record and open/close connection events.
    void initialize_input(Input::input_p& out_input, xmlNodePtr item);

    //! Called by operator() to process the tx in an <item>.
    void process_tx(Input::input_p& out_input, xmlNodePtr item);

    //! Cleanup libxml2 state.
    ~BurpProcessor();

private:
    //! Document.
    xmlDocPtr m_doc;
    //! Current node being operated on. This should probably be a local val.
    xmlNodePtr m_cur;
    //! Context for XPath operations.
    xmlXPathContextPtr m_xpath_ctx;
    //! XPath for /items/item which is the section of the file of interest.
    xmlXPathObjectPtr m_xpath_obj;
    //! Index into m_xpath_obj node table.
    size_t m_item_idx;


    /**
     * Store strings that must live the duration this object.
     *
     * Because Clipp tries to avoid copying data, the lifetime of
     * a string must go beyond a function call.
     * This is a list of strings we allocate and must clean up
     * when this object is eventually destroyed.
     */
    std::list<std::string> m_strings;

    /**
     * Compare @a name to the XML node name.
     *
     * @param[in] name The name to check for.
     * @param[in] node The node to extract and compare the name to.
     *
     * @return True of @a name matches @a node->name, false otherwise.
     */
    bool name_is(const char * name, xmlNodePtr node);

    /**
     * Recursively serialize the TEXT contents of @a node.
     *
     * The algorithm is that for every child node
     * that has no children, the content is appended to a string.
     * If a child has children, those children are recursively
     * processed in a depth-first traversal of the node tree.
     *
     * @param[in] node The node to serialize all the contents of.
     */
    std::string nodeContent(const xmlNodePtr node);

    /**
     * Using BurpGenerator::nodeContent() convert @a node to a Buffer.
     *
     * This will intern the string prodcued in the m_strings
     * collection so that it will not be destroyed and the Buffer
     * can reference that memory.
     *
     * @param[in] node The node to get the content of.
     */
    Input::Buffer nodeContentToBuffer(const xmlNodePtr node);
};

BurpProcessor::BurpProcessor(const char *file)
:
    m_doc(xmlParseFile(file)),
    m_cur(NULL),
    m_xpath_ctx(NULL),
    m_xpath_obj(NULL),
    m_item_idx(0)
{
    // Document failed to parse.
    if (m_doc == NULL ) {
        // We are in a constructor. Destroy m_doc manually.
        xmlFreeDoc(m_doc);
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Cannot parse XML file.")
        );
    }

    // Current root node.
    m_cur = xmlDocGetRootElement(m_doc);
    if (m_cur == NULL) {
        // We are in a constructor. Destroy m_doc manually.
        xmlFreeDoc(m_doc);
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Empty file.")
        );
    }

    m_xpath_ctx = xmlXPathNewContext(m_doc);
    if (m_xpath_ctx == NULL) {
        // We are in a constructor. Destroy m_doc manually.
        xmlFreeDoc(m_doc);
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Failed to allocate an XPath context.")
        );
    }

    // Grab all /items/item blocks.
    m_xpath_obj = xmlXPathEval(
        reinterpret_cast<const xmlChar*>("/items/item"),
        m_xpath_ctx
    );
    if (m_xpath_obj == NULL) {
        // We are in a constructor. Destroy m_doc and m_xpath_ctx manually.
        xmlFreeDoc(m_doc);
        xmlXPathFreeContext(m_xpath_ctx);
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Failed to allocate XPath /items/item.")
        );
    }
}

BurpProcessor::~BurpProcessor()
{
    xmlXPathFreeObject(m_xpath_obj);
    xmlXPathFreeContext(m_xpath_ctx);
    xmlFreeDoc(m_doc);
}

bool BurpProcessor::operator()(Input::input_p& input)
{
    // For each item node.
    if (m_item_idx < m_xpath_obj->nodesetval->nodeNr)
    {
        xmlNodePtr node = m_xpath_obj->nodesetval->nodeTab[m_item_idx];

        initialize_input(input, node);

        process_tx(input, node);

        ++m_item_idx;

        return true;
    }
    else {
       return false;
    }
}

void BurpProcessor::process_tx(Input::input_p& out_input, xmlNodePtr item)
{
    Input::Buffer request;
    Input::Buffer response;
    for (xmlNodePtr i = item->children; i != NULL; i = i->next) {
        if (name_is("request", i)) {
            request = nodeContentToBuffer(i);
        }
        else if (name_is("response", i)) {
            response = nodeContentToBuffer(i);
        }
    }

    out_input->connection.add_transaction(request, response);
}

void BurpProcessor::initialize_input(Input::input_p& out_input, xmlNodePtr item)
{

    Input::Buffer local_ip("1.2.3.4", 7);
    uint32_t      local_port  = 80;
    Input::Buffer remote_ip("5.6.7.8", 7);
    uint32_t      remote_port = 1234;

    for (xmlNodePtr i = item->children; i != NULL; i = i->next) {
        if (name_is("host", i)) {
            local_ip = nodeContentToBuffer(i);
        }
        else if (name_is("port", i)) {
            local_port = atoi(nodeContent(i).c_str());
        }
    }

    // Record 2 events - pre-tx connection open and a post-tx connection close.
    out_input->connection.connection_opened(
        local_ip,
        local_port,
        remote_ip,
        remote_port
    );
}

bool BurpProcessor::name_is(const char * name, xmlNodePtr node)
{
    // For the range of values it is OK to reinterpret_cast
    // what is an unsigned char* to a char*.
    return boost::iequals(
        reinterpret_cast<const char*>(node->name),
        name
    );
}

std::string BurpProcessor::nodeContent(const xmlNodePtr node)
{
    // If this is a text node, serialize it to a string.
    if (node->type == XML_TEXT_NODE) {
        size_t len = strlen(reinterpret_cast<const char *>(node->content));
        std::string content(node->content, node->content + len);
        return content;
    }
    // Assume any node with no children might have text.
    else if (node->children == NULL) {
        size_t len = strlen(reinterpret_cast<const char *>(node->content));
        std::string content(node->content, node->content + len);
        return content;
    }
    // In all other cases, try to descend the tree, depth first.
    else {
        std::string content;
        for (xmlNodePtr i = node->children; i != NULL; i = i->next) {
            content += nodeContent(i);
        }
        return content;
    }
}

Input::Buffer BurpProcessor::nodeContentToBuffer(const xmlNodePtr node)
{
    std::string s("");
    m_strings.push_back(s);
    m_strings.back() = nodeContent(node);
    return Input::Buffer(
        m_strings.back().data(),
        m_strings.back().length()
    );
}

} // anonymous namespace

struct BurpGenerator::State {
    State()
    :
        m_path(""),
        m_burp_processor(m_path.c_str()),
        m_output_generated(false),
        i(0)
    {
    }

    explicit
    State(const std::string& path)
    :
        m_path(path),
        m_burp_processor(m_path.c_str()),
        m_output_generated(false),
        i(0)
    {
    }

    std::string   m_path;
    BurpProcessor m_burp_processor;
    bool m_output_generated;
    int i;
};

BurpGenerator::BurpGenerator()
:
    m_state(new State("-"))
{
}

BurpGenerator::BurpGenerator(const std::string& path)
:
    m_state(new State(path))
{
}

bool BurpGenerator::operator()(Input::input_p& out_input)
{
    BurpProcessor& bp = m_state->m_burp_processor;

    // Reset Input
    out_input.reset(new Input::Input());
    out_input->id = m_state->m_path;

    m_state->m_output_generated = bp(out_input);

    return m_state->m_output_generated;
}


} // CLIPP
} // IronBee

