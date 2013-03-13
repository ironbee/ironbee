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
 * @brief Predicate --- Standard Nodes
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "dag.hpp"

#include "call_factory.hpp"

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Falsy value, null.
 **/
class False :
    public DAG::Call
{
public:
    //! See DAG::Call::name()
    virtual std::string name() const;

protected:
    //! See DAG::Node::calculate()
    virtual DAG::Value calculate(DAG::Context);
};

/**
 * Truthy value, ''.
 **/
class True :
    public DAG::Call
{
public:
    //! See DAG::Call::name()
    virtual std::string name() const;

protected:
    //! See DAG::Node::calculate()
    virtual DAG::Value calculate(DAG::Context);
};

/**
 * True iff any children are truthy.
 **/
class Or :
    public DAG::Call
{
public:
    //! See DAG::Call::name()
    virtual std::string name() const;

protected:
    virtual DAG::Value calculate(DAG::Context context);
};

/**
 * True iff all children are truthy.
 **/
class And :
    public DAG::Call
{
public:
    //! See DAG::Call::name()
    virtual std::string name() const;

protected:
    virtual DAG::Value calculate(DAG::Context context);
};

/**
 * True iff child is falsy.
 **/
class Not :
    public DAG::Call
{
public:
    //! See DAG::Call::name()
    virtual std::string name() const;

protected:
    virtual DAG::Value calculate(DAG::Context context);
};

/**
 * Load all standard calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load(CallFactory& to);

} // Standard
} // Predicate
} // IronBee
