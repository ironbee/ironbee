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
 * @brief Predicate --- Standard String Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/mm_ptr.hpp>

#include <ironbee/predicate/standard_string.hpp>
#include <ironbee/predicate/call_factory.hpp>
#include <ironbee/predicate/call_helpers.hpp>
#include <ironbee/predicate/functional.hpp>
#include <ironbee/predicate/validate.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/regex.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Regexp based replacement.
 *
 * First child is expression, second child is replacement, third child is
 * text.  Result is substitution applied to each string value of text child.
 **/
class StringReplaceRx :
    public Functional::Map
{
public:
    //! Constructor.
    StringReplaceRx() :
        Functional::Map(2, 1)
    {
        // nop
    }

protected:
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n < 2 && v.type() != Value::STRING) {
            reporter.error("Must be of type string: " + v.to_s());
        }
    }

    bool prepare(
        MemoryManager                  mm,
        const Functional::value_vec_t& static_args,
        Environment                    environment,
        NodeReporter                   reporter
    )
    {
        ConstByteString expression = static_args[0].as_string();
        try {
            m_expression.assign(
                expression.const_data(), expression.length(),
                boost::regex_constants::normal
            );
        }
        catch (const boost::bad_expression& e) {
            reporter.error(
                "Could not compile regexp: " +
                expression.to_s() +
                " (" + e.what() + ")"
            );
            return false;
        }

        m_replacement = static_args[1].as_string().to_s();
        return true;
    }

    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        /* If the subvalue we've been given is NULL, likewise return a NULL
         * subvalue. We can't change anything. */
        if (! subvalue) {
            return Value();
        }

        if (subvalue.type() != Value::STRING) {
            return Value();
        }

        ConstByteString text = subvalue.as_string();
        MMPtr<vector<char> > result(mm);
        result->reserve(1);

        boost::regex_replace(
            back_inserter(*result),
            text.const_data(), text.const_data() + text.length(),
            m_expression,
            m_replacement
        );

        return Value::create_string(
            mm,
            subvalue.name(), subvalue.name_length(),
            ByteString::create_alias(
                mm,
                result->data(), result->size()
            )
        );
    }

private:
    boost::regex m_expression;
    string       m_replacement;
};

/**
 * Length of string.
 **/
class Length :
    public Functional::Map
{
public:
    //! Constructor.
    Length() : Functional::Map(0, 1) {}

protected:
    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        if (subvalue.type() != Value::STRING) {
            return Value();
        }

        return Value::create_number(
            mm,
            subvalue.name(), subvalue.name_length(),
            subvalue.as_string().size()
        );
    }
};

void load_string(CallFactory& to)
{
    to
        .add("stringReplaceRx", Functional::generate<StringReplaceRx>)
        .add("length", Functional::generate<Length>)
        ;
}

} // Standard
} // Predicate
} // IronBee
