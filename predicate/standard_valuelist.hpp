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
 * @brief Predicate --- Standard ValueList.
 *
 * See reference.md for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_VALUELIST__
#define __PREDICATE__STANDARD_VALUELIST__

#include <predicate/merge_graph.hpp>

#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>
#include <predicate/meta_call.hpp>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Construct a named value from a name (string) and value.
 **/
class SetName :
    public MapCall
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual Value value_calculate(Value v, EvalContext context);
    virtual void calculate(EvalContext context);
};

/**
 * Concatenate values of children.
 **/
class Cat :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

protected:
    virtual void calculate(EvalContext context);
};

/**
 * First value.
 **/
class First :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void calculate(EvalContext context);
};

/**
 * All but first value.
 **/
class Rest :
    public Call
{
public:
    //! Constructor.
    Rest();

    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void reset();
    virtual void calculate(EvalContext context);

private:
    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * Nth value.
 **/
class Nth :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void calculate(EvalContext context);
};

/**
 * Expand simple list value.
 **/
class Scatter :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void calculate(EvalContext context);
};

/**
 * Gathers values into simple list value.
 **/
class Gather :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void calculate(EvalContext context);
};

/**
 * Select named subfield of simple list value.
 **/
class Sub :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void calculate(EvalContext context);
};

/**
 * Load all standard valuelists calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_valuelist(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
