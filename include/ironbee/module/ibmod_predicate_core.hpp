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
 * @brief Predicate --- Predicate module core API.
 *
 * Defines public API for predicate core module.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__IBMOD_PREDICATE_CORE__
#define __PREDICATE__IBMOD_PREDICATE_CORE__

#include <ironbee/predicate/call_factory.hpp>
#include <ironbee/predicate/ironbee.hpp>

#include <ironbeepp/context.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/transaction.hpp>

#include <boost/shared_ptr.hpp>

#include <list>
#include <string>
#include <vector>

/**
 * Predicate Core Module
 *
 * @section Oracles
 *
 * Oracles are the central service of Predicate.  An oracle is acquired for a
 * given expression at configuration time and can then be used at runtime to
 * find the result of that expression.  Predicate coordinates all oracles,
 * sharing information as much as possible.
 *
 * Oracles are acquired for a specific context and are only valid for
 * transactions for that context or for a child context.
 *
 * Modules may acquire an oracle via acquire().  The oracle is itself a
 * function that can be called to query it.
 *
 * @section Defining Templates
 *
 * A module may define templates via define_template().  It is important to
 * note that template names share a namespace with all other templates and
 * predicate functions.
 *
 * @section Adding Functions
 *
 * A module may add additional predicate functions by adding them to call
 * factory provided by call_factory().
 *
 * @section Origin Information
 *
 * When acquiring an oracle or defining a template, an origin must be
 * specified.  An origin can be any string and should describe the origin of
 * the oracle/template from a user perspective, e.g., a filename and line
 * number.  Origins are reported with errors and are intended to aid users in
 * finding the source of the error.
 **/
namespace IBModPredicateCore {

/**
 * The result of querying an oracle.  Value and finished status.
 **/
typedef std::pair<IronBee::Predicate::Value, bool> result_t;

/**
 * An oracle.  Given a transaction, gives result.
 *
 * Note that the transaction must either have the same context as the oracle
 * was generated in, or a child context opened after the query was generated.
 **/
class Oracle
{
friend Oracle acquire(
    IronBee::Engine,
    IronBee::Context,
    const IronBee::Predicate::node_p&,
    const std::string&
);

friend std::vector<Oracle> acquire_from_root(
    IronBee::Engine,
    IronBee::ConstContext,
    const IronBee::Predicate::node_cp&
);

public:
    //! Evaluate.
    result_t operator()(IronBee::Transaction tx) const;

    //! Node accessor.  Only valid after context close.
    const IronBee::Predicate::node_cp& node() const;

    //! Index accessor.  Always valid.
    size_t index() const;

private:
    struct impl_t;

    //! Constructor.
    explicit
    Oracle(const boost::shared_ptr<impl_t>& impl);

    //! Implementation.
    boost::shared_ptr<impl_t> m_impl;
};

/**
 * Acquire an oracle; string version.
 *
 * This function can be called during configuration to register a future
 * query and receive an oracle for that query.  The query, represented as an
 * s-expression given by @a expr, is registered with the core module.  It will
 * thus share information with all other queries in the context.  The
 * resulting oracle is valid for transactions in @a context or any child
 * contexts opened after the oracle was acquired.
 *
 * @param[in] engine  IronBee engine.
 * @param[in] context Current context.
 * @param[in] expr    Expression to query.
 * @param[in] origin  Origin, e.g., file and line number.  Displayed with
 *                    error messages.
 * @return Oracle.
 **/
Oracle acquire(
    IronBee::Engine    engine,
    IronBee::Context   context,
    const std::string& expr,
    const std::string& origin
);

/**
 * Acquire an oracle; node version.
 *
 * This is an overload of the previous function that takes a node instead of
 * an expression.  It is useful for users who want to do the parsing
 * themselves.  The node must have been generated using the call factory
 * provided by call_factory().
 *
 * @param[in] engine  IronBee engine.
 * @param[in] context Current context.
 * @param[in] expr    Expression to query.
 * @param[in] origin  Origin, e.g., file and line number.  Displayed with
 *                    error messages.
 * @return Oracle.
 **/
Oracle acquire(
    IronBee::Engine                   engine,
    IronBee::Context                  context,
    const IronBee::Predicate::node_p& expr,
    const std::string&                origin
);

/**
 * Acquire an oracle from a known root.
 *
 * This method is primarily intended for use with introspective methods
 * such as those in dot2.hpp, to convert provided nodes into Oracles.
 *
 * @param[in] engine  IronBee engine.
 * @param[in] context Current context.
 * @param[in] root    Root node for current context DAG.  Must be an exact
 *                    match, not simply equivalent.  I.e., must have come
 *                    from the current context DAG via some means.
 * @return Oracle.
 * @throw IronBee::enoent if @a root is not a root in @a context DAG.
 **/
std::vector<Oracle> acquire_from_root(
    IronBee::Engine                    engine,
    IronBee::ConstContext              context,
    const IronBee::Predicate::node_cp& root
);

/**
 * Define a template; string version.
 *
 * The template will then be available via call_factory() and to any
 * expressions passed to acquire().
 *
 * @param[in] engine IronBee engine.
 * @param[in] name   Name of template.  Must be unique.
 * @param[in] args   Arguments to template.
 * @param[in] body   Body of template.
 * @param[in] origin Origin, e.g., file and line number.
 * @throw IronBee::einval if @a name is already taken.
 **/
void define_template(
    IronBee::Engine               engine,
    const std::string&            name,
    const std::list<std::string>& args,
    const std::string&            body,
    const std::string&            origin
);

/**
 * Define a template; node version.
 *
 * This is an overload of the previous function that takes a node instead of
 * an expression.
 *
 * @param[in] engine IronBee engine.
 * @param[in] name   Name of template.  Must be unique.
 * @param[in] args   Arguments to template.
 * @param[in] body   Body of template.
 * @param[in] origin Origin, e.g., file and line number.
 * @throw IronBee::einval if @a name is already taken.
 **/
void define_template(
    IronBee::Engine                    engine,
    const std::string&                 name,
    const std::list<std::string>&      args,
    const IronBee::Predicate::node_cp& body,
    const std::string&                 origin
);

/**
 * Call factory.
 *
 * This function can be used to add calls to the call factory used by the
 * predicate module.
 *
 * @param[in] engine IronBee engine.
 * @return Call factory used by Predicate module.
 **/
IronBee::Predicate::CallFactory& call_factory(
    IronBee::Engine engine
);

/**
 * Access graph eval state for a transaction.
 *
 * This functions provides access to the GraphEvalState.  It is probably only
 * need for introspection, e.g., via dot2.
 *
 * @param[in] tx Transaction.
 * @return Graph eval state.
 **/
const IronBee::Predicate::GraphEvalState& graph_eval_state(
    IronBee::ConstTransaction tx
);

} // IBModPredicateCore

#endif
