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
 * @brief Predicate --- C API
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee_config_auto_gen.h>

#include <assert.h>
#include <string>
#include <ironbee/predicate/predicate_c_api.h>
#include <ironbee/predicate/dag.hpp>
#include <ironbee/predicate/call_factory.hpp>
#include <ironbee/predicate/parse.hpp>
#include <ironbee/predicate/standard.hpp>
#include <ironbee/mm.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbeepp/catch.hpp>
#include <boost/foreach.hpp>
#include <iostream>

namespace P = IronBee::Predicate;
using namespace std;

ib_status_t ib_predicate_parse(
    ib_predicate_node_t **node,
    const char           *expression
)
{
    assert(node != NULL);
    assert(expression != NULL);

    string expr(expression);
    size_t s = 0;

    try {
        P::CallFactory call_factory;
        P::Standard::load(call_factory);
        *node = reinterpret_cast<ib_predicate_node_t *>(
            new P::node_cp(P::parse_call(expr, s, call_factory)));
        assert(*node != NULL);
    }
    catch (const IronBee::error& e) {
        string message;

        message += *boost::get_error_info<IronBee::errinfo_what>(e);
        std::cerr<<message<<std::endl;

        return IronBee::convert_exception();
    }

    return IB_OK;
}

void ib_predicate_node_destroy(
    ib_predicate_node_t *node
)
{
    assert(node != NULL);
    delete reinterpret_cast<P::node_cp *>(node);
}

const char * DLL_PUBLIC ib_predicate_node_to_s(
    ib_predicate_node_t *node
)
{
    assert(node != NULL);

    return reinterpret_cast<P::node_cp *>(node)->get()->to_s().c_str();
}

/**
 * Return the number of child nodes.
 */
size_t DLL_PUBLIC ib_predicate_node_child_count(
    ib_predicate_node_t *node
)
{
    assert(node != NULL);

    return reinterpret_cast<P::node_cp *>(node)->get()->children().size();
}


/**
 * Return the name of the node.
 */
const char *ib_predicate_node_name(
    ib_predicate_node_t *node
)
{
    assert(node != NULL);

    return reinterpret_cast<P::call_cp *>(node)->get()->name().c_str();
}

/**
 * Return child node i or NULL if none.
 *
 * @param[in] i The child index to retrieve.
 *
 * @returns The node or NULL if the index is out of bounds.
 */
void DLL_PUBLIC ib_predicate_node_children(
    ib_predicate_node_t  *node,
    ib_predicate_node_t **children
)
{
    assert(node != NULL);

    int i = 0;

    BOOST_FOREACH(
        P::node_cp p,
        (*reinterpret_cast<P::node_cp *>(node))->children()
    )
    {
        children[i] = const_cast<ib_predicate_node_t *>(
            reinterpret_cast<const ib_predicate_node_t *>(
                p.get()));
        ++i;
    }
}

/**
 * Return if the given node is a literal or not.
 */
bool DLL_PUBLIC ib_predicate_node_is_literal(
    ib_predicate_node_t *node
)
{
    assert(node != NULL);

    return (*reinterpret_cast<P::node_cp *>(node))->is_literal();
}


