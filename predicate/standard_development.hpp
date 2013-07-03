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
 * @brief Predicate --- Standard Development
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_DEVELOPMENT__
#define __PREDICATE__STANDARD_DEVELOPMENT__

#include <predicate/call_factory.hpp>
#include <predicate/dag.hpp>
#include <predicate/meta_call.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Output value of first child and message of second child.
 **/
class P :
    public MaplikeCall
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See MaplikeCall::value_calculate()
    virtual Value value_calculate(Value v, EvalContext context);

    //! See Node::calculate()
    virtual void calculate(EvalContext context);
};

/**
 * Take values of child; do not transform.
 **/
class Identity :
    public MaplikeCall
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See MaplikeCall::value_calculate()
    virtual Value value_calculate(Value v, EvalContext context);

    //! See Node::calculate()
    virtual void calculate(EvalContext context);
};

/**
 * Sequence of values; potentially infinite.
 **/
class Sequence :
    public Call
{
public:
    //! Constructor.
    Sequence();

    //! See Node::reset().
    virtual void reset();

    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See Node::calculate()
    virtual void calculate(EvalContext context);

private:
    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * Load all standard development calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_development(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
