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
 * @brief Predicate --- Standard Math implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_math.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/functional.hpp>
#include <predicate/reporter.hpp>
#include <predicate/validate.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

//! Map: Addition.
class Add :
    public Functional::Map
{
public:
    //! Constructor.
    Add() : Functional::Map(0, 2) {}

    //! See Functional::Base::validate_argument()
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            if (v.type() != Value::NUMBER && v.type() != Value::FLOAT) {
                reporter.error("Value " + v.to_s() + " is not numeric.");
            }
        }
    }

protected:
    //! See Functional::Map::eval_map()
    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        Value lhs = secondary_args[0];
        switch (subvalue.type()) {
            case Value::NUMBER:
                switch (lhs.type()) {
                    case Value::NUMBER:
                        return Value::create_number(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_number() + subvalue.as_number()
                        );
                    case Value::FLOAT:
                        return Value::create_float(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_float() + subvalue.as_number()
                        );
                    default:
                        assert(! "Unreachable");
                }
            case Value::FLOAT:
                switch (lhs.type()) {
                    case Value::NUMBER:
                        return Value::create_float(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_number() + subvalue.as_float()
                        );
                    case Value::FLOAT:
                        return Value::create_float(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_float() + subvalue.as_float()
                        );
                    default:
                        assert(! "Unreachable");
                }
            default:
                return subvalue;
        }
    }
};

//! Map: Multiplication.
class Mult :
    public Functional::Map
{
public:
    //! Constructor.
    Mult() : Functional::Map(0, 2) {}

    //! See Functional::Base::validate_argument()
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            if (v.type() != Value::NUMBER && v.type() != Value::FLOAT) {
                reporter.error("Value " + v.to_s() + " is not numeric.");
            }
        }
    }

protected:
    //! See Functional::Map::eval_map()
    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        Value lhs = secondary_args[0];
        switch (subvalue.type()) {
            case Value::NUMBER:
                switch (lhs.type()) {
                    case Value::NUMBER:
                        return Value::create_number(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_number() * subvalue.as_number()
                        );
                    case Value::FLOAT:
                        return Value::create_float(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_float() * subvalue.as_number()
                        );
                    default:
                        assert(! "Unreachable");
                }
            case Value::FLOAT:
                switch (lhs.type()) {
                    case Value::NUMBER:
                        return Value::create_float(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_number() * subvalue.as_float()
                        );
                    case Value::FLOAT:
                        return Value::create_float(
                            mm,
                            subvalue.name(), subvalue.name_length(),
                            lhs.as_float() * subvalue.as_float()
                        );
                    default:
                        assert(! "Unreachable");
                }
            default:
                return subvalue;
        }
    }
};

//! Map: Reciprocal.
class Recip :
    public Functional::Map
{
public:
    //! Constructor.
    Recip() : Functional::Map(0, 1) {}

protected:
    //! See Functional::Map::eval_map()
    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        switch (subvalue.type()) {
            case Value::FLOAT:
                return Value::create_float(
                    mm,
                    subvalue.name(), subvalue.name_length(),
                    1 / subvalue.as_float()
                );
            case Value::NUMBER: {
                long double f = subvalue.as_number();
                return Value::create_float(
                    mm,
                    subvalue.name(), subvalue.name_length(),
                    1 / f
                );
            }
            default:
                return subvalue;
        }
    }
};

//! Map: Negate.
class Neg :
    public Functional::Map
{
public:
    //! Constructor.
    Neg() : Functional::Map(0, 1) {}

protected:
    //! See Functional::Map::eval_map()
    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        switch (subvalue.type()) {
            case Value::FLOAT:
                return Value::create_float(
                    mm,
                    subvalue.name(), subvalue.name_length(),
                    -subvalue.as_float()
                );
            case Value::NUMBER:
                return Value::create_number(
                    mm,
                    subvalue.name(), subvalue.name_length(),
                    -subvalue.as_number()
                );
            default:
                return subvalue;
        }
    }
};

//! Numeric value.
inline
long double numeric_value(Value v)
{
    if (v.type() == Value::NUMBER) {
        return v.as_number();
    }
    if (v.type() == Value::FLOAT) {
        return v.as_float();
    }
    assert(! "v is number or float.");
}

//! Maximum.
class Max :
    public Functional::Simple
{
public:
    //! Constructor.
    Max() : Functional::Simple(0, 1) {}

    //! See Functional::Base::validate_argument()
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::LIST, reporter);
        }
    }

protected:
    //! See Functional::Simple::eval_simple()
    Value eval_simple(
        MemoryManager                  mm,
        const Functional::value_vec_t& dynamic_args
    ) const
    {
        Value max;
        BOOST_FOREACH(Value v, dynamic_args[0].as_list()) {
            if (
                v &&
                (v.type() == Value::NUMBER || v.type() == Value::FLOAT)
            ) {
                if (! max || numeric_value(v) > numeric_value(max)) {
                    max = v;
                }
            }
        }
        return max;
    }
};

//! Minimum.
class Min :
    public Functional::Simple
{
public:
    //! Constructor.
    Min() : Functional::Simple(0, 1) {}

    //! See Functional::Base::validate_argument()
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::LIST, reporter);
        }
    }

protected:
    //! See Functional::Simple::eval_simple()
    Value eval_simple(
        MemoryManager                  mm,
        const Functional::value_vec_t& dynamic_args
    ) const
    {
        Value max;
        BOOST_FOREACH(Value v, dynamic_args[0].as_list()) {
            if (
                v &&
                (v.type() == Value::NUMBER || v.type() == Value::FLOAT)
            ) {
                if (! max || numeric_value(v) < numeric_value(max)) {
                    max = v;
                }
            }
        }
        return max;
    }
};

} // Anonymous

void load_math(CallFactory& to)
{
    to
        .add("add", Functional::generate<Add>)
        .add("mult", Functional::generate<Mult>)
        .add("neg", Functional::generate<Neg>)
        .add("recip", Functional::generate<Recip>)
        .add("max", Functional::generate<Max>)
        .add("min", Functional::generate<Min>)
        ;
}

} // Standard
} // Predicate
} // IronBee
