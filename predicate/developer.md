Title:  IronBee Predicate Developer Manual
Author: Christopher Alfeld <calfeld@qualys.com>

Predicates Rules User Manual
============================

Christopher Alfeld <calfeld@qualys.com><br>

**Warning:** This document describes a feature that is under ongoing development.  Only portions of what is described are currently available and all of it is subject to change.  This document is intended for IronBee developers, those wanting to play on the bleeding edge, and those interested in upcoming major features.

Introduction
------------

This document describes how the internals of the Predicate system work from the viewpoint of a Call node and how to write additional Calls.

All code is in C++ and in the IronBee::Predicate namespace.

The Predicate DAG is made up of instances of a class hierarchy.  The top of the class hierarchy is `Node`.  One branch below `Node` handles literals and ends with the concrete classes `String` and `Null`.  The other branch ends with a virtual class `Call`.  Below `Call` are classes for each function, generally, one class per function.

As a developer of new Call nodes, you will be writing new subclasses of `Call`.  To do so effectively, you need to understand the Call lifecycle and which methods to implement.

This document is not comprehensive.  It should be read along with the API documentation for Predicate, especially `dag.hpp`.

DAG Structure
-------------

The DAG is maintained via child and parent pointers in Node.  Children are stored as shared pointers and thus stay allocated so long as there is a parent that holds them.  The roots of the DAG are stored externally, e.g., by MergeGraph or `ibmod_predicate`.  Thus the nodes that exist are exactly those that can be reached from a root.  In order to avoid shared pointer cycles, parents are stored as weak pointers.  Thus, holding a child is insufficient to guarantee that its parents are not destroyed.

Children are available via `Node::children()` and parents via `Node::parents()`.  Remember to lock parent pointers in order to use them.

Call Lifecycle
--------------

Calls go through several phases.  At present, all of these phases take place in `ibmod_predicate`.  However, they are designed to be separable and may be so in the future.

1. Parsing, Construction, and Assembly: An expression tree is assembled from some stored form such as S-Expressions.
2. Merging: The expression tree is added to the MergeGraph, merging any common subexpressions with those of previously added expressions.
3. Validation and Transformation: The entire graph is validated, transformed, and validated again.
4. Pre-Evaluation: Every node in the graph is provided with its Environment, the IronBee Engine, and given an opportunity to set up any internal structures it needs to evaluate itself.
5. Reset and Evaluation: Portions of the graph are reset and evaluated as needed to determine which rules inject.
6. Destruction: Nodes are destroyed.

**Parsing, Construction, and Assembly**

Every Call node registers itself with a CallFactory.  For `ibmod_predicate`, this can be done by calling, e.g., via:

    IBModPredicateCallFactory(engine).add<MyCustomCall>();

`IBModPredicateCallFactory()` is defined in `ibmod_predicate.hpp`.  The predicate module must be loaded before any module that calls `IBModPredicateCallFactory()`.

On parsing, the CallFactory will be used to construct Nodes.  Normally, Nodes are constructed via their default constructors, but you can also provide an arbitrary functional to build a Call given its name.  Note that Nodes thus begin with no children.

As parsing continues, children will be constructed and added to the Nodes via `Node::add_child()`.

**Merging**

Once an expression is parsed into an expression tree, the root of that tree is added to a MergeGraph.  The MergeGraph searches for common subexpressions in existing nodes and replaces nodes in the incoming tree with those nodes as it can.

An important ramification of this merging is that the Node added may not be the Node stored in the tree.  To allow for this, `MergeGraph::add_root()` returns a *root index*.  The root index will refer to the root of the provided expression tree regardless of any future merging or transformations.  It can be turned back into a Node via `MergeGraph::root()`.

As an example, `ibmod_predicate`, during configuration, adds all expressions to the MergeGraph and keeps track of the root *indices* and which rules are connected to them.  At the end of configuration, once all merging and transformation is complete, it translates these back into Nodes, stores those and their associated rules in an appropriate datastructure, and then discards the MergeGraph.

**Validation and Transformation**

Every node is given a chance to validate itself both before and after transform via `Node::pre_transform()` and `Node::post_transform()`.  Nodes report any issues by calling `NodeReporter::error()` or `NodeReporter::warning()` on the provided NodeReporter.

At present, if any Node reports errors, it is a configuration error and IronBee will stop.  In the future, Nodes with errors may simply be removed along with any ancestors from the DAG.

Transformation is done via `Node::transform()`.  Nodes may transform themselves (or any other part of the DAG) by calling appropriate MergeGraph methods such as `MergeGraph::replace()`.  A typical transformation looks like:

    node_p me = shared_from_this();
    node_p replacement = call_factory(replacement_name);
    // Add children to replacement...
    merge_graph.replace(me, replacement);

Note the use of `shared_from_this()` to allow a Node to get a shared pointer to itself.

**Pre-Evaluation**

Every node is provided with the *Environment*, the IronBee engine, and given a chance to initialize any data it needs to calculate its value in the future via `Node::pre_eval()`.  E.g., operator instances.  Nodes may also report failures here if they are unable to construct the data they need.

**Reset and Evaluation**

Nodes provide a value for themselves via `Node::calculate()` which takes an `EvalContext`, an IronBee transaction, and returns a value.  Nodes can query the value of their children via `Node::eval()`.  Values are automatically cached and `Node::calculate()` will only be called the first time the value of that node is needed.  At each phase, Nodes are reset (via `Node::reset()`).

**Destruction**

All nodes are destroyed when Predicate is completely, i.e., at IronBee termination.  Nodes are also destroyed when no longer needed as part of merging and transformation.  Consistent use of RAII means that most Nodes do not need custom destructors, and Call writers are encouraged to use similar techniques.

Writing a new Call
------------------

To write a new Call node, you must do two things in an IronBee module:

- Subclass IronBee::Predicate::Call and implement at least `name()` and `calculate()` and possibly other methods, e.g., `transform()`.
- If every instance of your class is the same function (same name), use `IBModPredicateCallFactory(engine).add<CallType>()` to register your class.  It will now be available to all sexprs under the name returned by `name()`.  Your class will be constructed via the default constructor.
- If the above options is not sufficient, e.g., your are defining several Call nodes in terms of a single class parameterized by name, you can register an arbitrary functional to generate an instance given a name via `IBModPredicateCallFactory(engine).add(name, generator)`.

What follows is an informal discussion of the methods you must or may implement.  See `dag.hpp` API documentation for details.

Your class must override the following two methods:

- `name()`: Return the name of your Call.  This name determines what name in the sexpr matches the Call.  It must be unique across all Call generators.
- `calculate(tx)`: Calculate a value for yourself given your children and the IronBee transaction `tx`.  You must throw an exception (fatal error) or call `set_value(v)` to set a value for yourself.  You can access your children via `children()` and access their values via `child->eval(tx)`.

Your class may override the following methods:

- `add_child(child)`, `remove_child(child)`, `replace_child(child)`: You can implement these to hook into adding, removing, or replacing children.  If you do so, be sure to call `Call::X_child(child)` which will handle updating your children list, their parent list, and your and your ancestors sexprs.  However, consider using one of next methods instead:
- `pre_transform(reporter)`, `post_transformer(reporter)`: The pre and post routines are intended for validation. You can use `reporter` to report any warnings or errors.  There is an optional library, `validate.hpp`, that can be used to easily do a variety of validations such as number of children.
- `transform(reporter, merge_graph, call_factory)`.  The `transform()` routine can additional be used to transform yourself or even the DAG around you.  Be sure to return `true` if you change anything and `false` otherwise.  This method will be called repeatedly until no node in the DAG returns true.
- `pre_evaluation(engine reporter)`: This method will be called after the methods above but before any evaluation loop.  The purpose of this method is to construct any internal structures you need for evaluation.
- Constructor: You may want to write a custom constructor if you are using a custom generator or to initialize data members.  Note that construction occurs early in the life cycle and should not initialize any evaluation data; use `pre_eval()` for that.
- Destructor: It is strongly suggested to use RAII techniques.

Your class may also use any of the public methods of the class defined in `dag.hpp`.
