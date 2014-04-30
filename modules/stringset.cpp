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
 * @brief IronBee --- StringSet Module
 *
 * This module adds support for longest matching prefix string matches.
 *
 * Adds two operators, both of which take a set of strinngs as a space
 * separated list as argument.
 *
 * - `@strmatch` is true iff the input is in the set.  The capture field is
 *   set to the input.
 * - @strmatch_prefix` is true iff a prefix of the input is in the set.  The
 *   capture field is set to the longest matching prefix.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/stringset.h>

#include <ironbeepp/all.hpp>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>

using namespace std;
using namespace IronBee;

namespace {

//! strmatch operator name.
const char* c_strmatch = "strmatch";
//! strmatch_prefix operator name.
const char* c_strmatch_prefix = "strmatch_prefix";

//! Called on module load.
void module_load(IronBee::Module module);

} // Anonymous

IBPP_BOOTSTRAP_MODULE("constant", module_load)

// Implementation

// Reopen for doxygen; not needed by C++.
namespace {

/**
 * Construct a string set.
 *
 * @param[in] mm Memory manager determining lifetime.
 * @param[in] parameters Space separated list of items.
 * @return String set.
 **/
const ib_stringset_t* construct_set(
    MemoryManager mm,
    const char* parameters
)
{
    vector<string> items;
    boost::split(items, parameters, boost::is_any_of(" "));

    ib_stringset_t* set = mm.allocate<ib_stringset_t>();
    ib_stringset_entry_t* entries =
        mm.allocate<ib_stringset_entry_t>(items.size());

    for (size_t i = 0; i < items.size(); ++i) {
        entries[i].string =
            reinterpret_cast<const char*>(
                mm.memdup(items[i].data(), items[i].size())
            );
        entries[i].length = items[i].size();
        entries[i].data   = NULL;
    }

    throw_if_error(ib_stringset_init(set, entries, items.size()));

    return set;
}

/** Execute @strmatch. */
int strmatch_prefix_execute(
    const ib_stringset_t* set,
    Transaction           tx,
    ConstField            input,
    Field                 capture
)
{
    if (! input) {
        return 0;
    }

    if (input.type() != Field::BYTE_STRING) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                string(c_strmatch) + " requires bytestring input."
            )
        );
    }

    ConstByteString bs = input.value_as_byte_string();
    const ib_stringset_entry_t* result;

    ib_status_t rc =
        ib_stringset_query(set, bs.const_data(), bs.size(), &result);
    if (rc == IB_OK) {
        if (capture) {
            capture.mutable_value_as_list<Field>().push_back(
                Field::create_no_copy_byte_string(
                    tx.memory_manager(),
                    input.name(), input.name_length(),
                    ByteString::create(
                        tx.memory_manager(),
                        result->string, result->length
                    )
                )
            );
        }
        return 1;
    }
    return 0;
}

/** Execute @strmatch_prefix. */
int strmatch_execute(
    const ib_stringset_t* set,
    ConstField input
)
{
    if (! input) {
        return 0;
    }

    if (input.type() != Field::BYTE_STRING) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                string(c_strmatch) + " requires bytestring input."
            )
        );

    }

    ConstByteString bs = input.value_as_byte_string();
    const ib_stringset_entry_t* result;

    ib_status_t rc =
        ib_stringset_query(set, bs.const_data(), bs.size(), &result);

    if (rc == IB_OK && result->length == bs.size()) {
        return 1;
    }
    return 0;
}

/** Generate @strmatch instance. */
Operator::operator_instance_t strmatch_generator(
    Context,
    MemoryManager mm,
    const char* parameters
)
{
    const ib_stringset_t* set = construct_set(mm, parameters);

    return bind(strmatch_execute, set, _2);
}

/** Execute @strmatch_prefix instance. */
Operator::operator_instance_t strmatch_prefix_generator(
    Context,
    MemoryManager mm,
    const char* parameters
)
{
    const ib_stringset_t* set = construct_set(mm, parameters);

    return bind(strmatch_prefix_execute, set, _1, _2, _3);
}

void module_load(IronBee::Module module)
{
    MemoryManager mm = module.engine().main_memory_mm();

    Operator::create(
        mm,
        c_strmatch,
        IB_OP_CAPABILITY_ALLOW_NULL,
        strmatch_generator
    ).register_with(module.engine());

    Operator::create(
        mm,
        c_strmatch_prefix,
        IB_OP_CAPABILITY_CAPTURE | IB_OP_CAPABILITY_ALLOW_NULL,
        strmatch_prefix_generator
    ).register_with(module.engine());
}

} // Anonymous

