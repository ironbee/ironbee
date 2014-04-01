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
 * @brief Predicate --- Standard Filter implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_filter.hpp>

#include <predicate/call_helpers.hpp>
#include <predicate/call_factory.hpp>
#include <predicate/functional.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>
#include <predicate/value.hpp>

#include <boost/regex.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

bool value_equal(Value a, Value b)
{
    if (! a && ! b) {
        return true;
    }
    if (a.to_field() == b.to_field()) {
        return true;
    }

    if (a.type() != b.type()) {
        return false;
    }

    switch (a.type()) {
        case Value::NUMBER:
            return a.as_number() == b.as_number();
        case Value::FLOAT:
            return a.as_float() == b.as_float();
        case Value::STRING:
        {
            ConstByteString a_s = a.as_string();
            ConstByteString b_s = b.as_string();
            if (a_s.length() != b_s.length()) {
                return false;
            }
            return equal(
                a_s.const_data(), a_s.const_data() + a_s.length(),
                b_s.const_data()
            );
        }
        case Value::LIST:
            return false;
        default:
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Unsupported value type for " +
                    a.to_s()
                )
            );
    }
}

bool value_less(Value a, Value b)
{   
    if (! a && ! b) {
        return false;
    }
    if (a.to_field() == b.to_field()) {
        return false;
    }
    
    if (b.type() != Value::NUMBER && b.type() != Value::FLOAT) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Unsupported value type for RHS " + b.to_s()
            )
        );
    }
    switch (a.type()) {
        case Value::NUMBER:
            return a.as_number() < b.as_number();
        case Value::FLOAT:
            return a.as_float() < b.as_float();
        default:
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    "Unsupported value type for LHS " + a.to_s()
                )
            );
    }
}

/**
 * Filter: Equal subvalues in type and value.
 **/
class Eq :
    public Functional::Filter
{
public:
    //! Constructor.
    Eq() : Functional::Filter(0, 2) {}

protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return value_equal(secondary_args[0], subvalue);
    }
};

/**
 * Filter: Opposite of Eq.
 **/
class Ne :
    public Functional::Filter
{
public:
    //! Constructor.
    Ne() : Functional::Filter(0, 2) {}

protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return ! value_equal(secondary_args[0], subvalue);
    }
};

/**
 * Base class for Lt, Le, Gt, and Ge.
 **/
class NumericBase :
    public Functional::Filter
{
public:
    NumericBase() : Functional::Filter(0, 2) {}
    
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            if (
                v.type() != Value::NUMBER && 
                v.type() != Value::FLOAT
            ) {
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        "Value must be float or number: " +
                        v.to_s()
                    )
                );
            }
        }
    }
};

/**
 * Filter: Subvalue less than secondary value.
 **/
class Lt :
    public NumericBase
{
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return value_less(subvalue, secondary_args[0]);
    }
};

/**
 * Filter: Subvalue less than or equal to secondary value.
 **/
class Le :
    public NumericBase
{
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return ! value_less(secondary_args[0], subvalue);
    }
};

/**
 * Filter: Subvalue greater than secondary value.
 **/
class Gt :
    public NumericBase
{
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return value_less(secondary_args[0], subvalue);
    }
};

/**
 * Filter: Subvalue greater than or equal to secondary value.
 **/
class Ge :
    public NumericBase
{
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return ! value_less(subvalue, secondary_args[0]);
    }
};

/**
 * Filter: Subvalues of specified type.
 **/
class Typed :
    public Functional::Filter
{
public:
    //! Constructor.
    Typed() : Functional::Filter(1, 1) {}
    
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            bool okay = Validate::value_is_type(v, Value::STRING, reporter);
            if (okay) {
                try {
                    typed_parse_type(v.as_string().to_s());
                }
                catch (einval) {
                    reporter.error("Invalid typed argument: " + v.to_s());
                }
            }
        }
    }
    
    //! See Functional::Base::prepare().
    void prepare(
        MemoryManager                  mm,
        const Functional::value_vec_t& static_args,
        NodeReporter                   reporter
    )
    {
        m_type = typed_parse_type(static_args[0].as_string().to_s());
    }
    
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return subvalue && subvalue.type() == m_type;
    }
    
private:
    static
    Value::type_e typed_parse_type(const string& type_s)
    {
        if (type_s == "list") {
            return Value::LIST;
        }
        if (type_s == "number") {
            return Value::NUMBER;
        }
        if (type_s == "float") {
            return Value::FLOAT;
        }
        if (type_s == "string") {
            return Value::STRING;
        }
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Invalid typed argument."
            )
        );
    }
    
    Value::type_e m_type;
};

/**
 * Filter: Subvalue has specified name.
 **/
class Named :
    public Functional::Filter
{
public:
    //! Constructor.
    Named() : Functional::Filter(0, 2) {}
    
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::STRING, reporter);
        }
    }
    
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        ConstByteString name = secondary_args[0].as_string();
    
        return
            subvalue &&
            subvalue.name_length() == name.length() &&
            equal(
                name.const_data(), name.const_data() + name.length(),
                subvalue.name()
            );
    }   
};  

/**
 * Filter: Subvalue has specified name; case insensitive.
 **/
class NamedI :
    public Functional::Filter
{
public:
    //! Constructor.
    NamedI() : Functional::Filter(0, 2) {}
    
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::STRING, reporter);
        }
    }
    
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        ConstByteString name = secondary_args[0].as_string();
    
        return
            subvalue &&
            subvalue.name_length() == name.length() &&
            equal(
                name.const_data(), name.const_data() + name.length(),
                subvalue.name(),
                namedi_caseless_compare
            );
    }   

private:
    static
    bool namedi_caseless_compare(char a, char b)
    {
        return (a == b || tolower(a) == tolower(b));
    }
};  

/**
 * Filter: Subvalue has specified name; regex.
 **/
class NamedRx :
    public Functional::Filter
{
public:
    //! Constructor.
    NamedRx() : Functional::Filter(1, 1) {}
    
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::STRING, reporter);
        }
    }
    
    //! See Functional::Base::prepare().
    void prepare(
        MemoryManager                  mm,
        const Functional::value_vec_t& static_args,
        NodeReporter                   reporter
    )
    {
        ConstByteString re = static_args[0].as_string();
        try {
            m_regex = boost::regex(
                re.const_data(), re.const_data() + re.length()
            );
        }
        catch (const boost::bad_expression& e) {
            reporter.error(
                "Error compiling regexp: " + re.to_s() + " (" + e.what() + ")"
            );
        }
    }
    
protected:
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return 
            subvalue && 
            regex_search(
                subvalue.name(), subvalue.name() + subvalue.name_length(),
                m_regex
            );
    }   

private:
    boost::regex m_regex;
};  

/**
 * Filter: List longer than specified length.
 **/
class Longer :
    public Functional::Filter
{
public:
    //! Constructor.
    Longer() : Functional::Filter(0, 2) {}
    
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::NUMBER, reporter);
        }
    }

protected:    
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        return 
            subvalue &&
            subvalue.type() == Value::LIST &&
            subvalue.as_list().size() > size_t(secondary_args[0].as_number());
    }
};

} // Anonymous

void load_filter(CallFactory& to)
{
    to
        .add("eq", Functional::generate<Eq>)
        .add("ne", Functional::generate<Ne>)
        .add("lt", Functional::generate<Lt>)
        .add("le", Functional::generate<Le>)
        .add("gt", Functional::generate<Gt>)
        .add("ge", Functional::generate<Ge>)
        .add("typed", Functional::generate<Typed>)
        .add("named", Functional::generate<Named>)
        .add("namedi", Functional::generate<NamedI>)
        .add("namedRx", Functional::generate<NamedRx>)
        .add("longer", Functional::generate<Longer>)
        ;
}

} // Standard
} // Predicate
} // IronBee
