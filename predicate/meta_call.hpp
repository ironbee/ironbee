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
 * @brief Predicate --- Helpful parents for Call nodes.
 *
 * XXX
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__META_CALL__
#define __PREDICATE__META_CALL__

#include <predicate/dag.hpp>

namespace IronBee {
namespace Predicate {

/**
 * Base class for calls that want children in canonical order.
 **/
class AbelianCall :
    virtual public Predicate::Call
{
public:
    //! Constructor.
    AbelianCall();

    // The three routines below simply mark this node as unordered.
    //! See Node::add_child().
    virtual void add_child(const node_p& child);
    //! See Node::replace_child().
    virtual void replace_child(const node_p& child, const node_p& with);

    /**
     * See Node::transform().
     *
     * Will order children canonically.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

private:
    bool m_ordered;
};

/**
 * Base class for calls that behave like maps.
 *
 * This class provides a protected method, map_calculate() to its children
 * which can be used in calculate() to easily apply a Value to Value function
 * to an input node to generate outputs.
 *
 * To use, subclass and implement value_calculate().  Then call
 * map_calculate() appropriately from calculate().  map_calculate() will
 * call value_calculate() on each value of the input and add the result if
 * non-NULL.
 **/
class MapCall :
    virtual public Predicate::Call
{
protected:
    //! Resets input location.
    virtual void reset();

    /**
     * Per-Value calculate function.  Must be implemented by child class.
     *
     * @param[in] v            Input value.
     * @param[in] eval_context Evaluation context.
     * @return Output value; may be NULL.
     **/
    virtual Value value_calculate(Value v, EvalContext eval_context) = 0;

    /**
     * Apply value_calculate() to every value of @a input.
     *
     * By default, will evaluate @a input at the beginning and will finish
     * this node if input is finished at the end.  These behaviors can be
     * overridden with @a eval_input and @a auto_finish.
     *
     * This routine does understand unfinished nodes.  If @a input is
     * unfinished, it will remember which values of @a input it has processed
     * and look for new ones the next time it is called.
     *
     * @param[in] input        Node whose values should be used as inputs.
     * @param[in] eval_context Evaluation context.
     * @param[in] eval_input   If true (the default), @a input will have
     *                         Node::eval() called on it first.  If you are
     *                         going to eval input yourself before calling
     *                         this function, set to false for a performance
     *                         gain.
     * @param[in] auto_finish  If true (the default), Node::finish() will be
     *                         called on this node if @a input is finished.
     *                         Set to false, if this is undesired, e.g., if
     *                         you are going to use map_calculate() on
     *                         multiple children.
     **/
    void map_calculate(
        const node_p& input,
        EvalContext   eval_context,
        bool          eval_input  = true,
        bool          auto_finish = true
    );

private:
    //! Type of @ref m_input_locations.
    typedef std::map<node_p, ValueList::const_iterator> input_locations_t;
    //! Map of node to location in its value list calculated so far.
    input_locations_t m_input_locations;
};

} // Predicate
} // IronBee

#endif
