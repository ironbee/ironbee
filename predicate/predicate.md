Title:  IronBee Predicate Rules Manual
Author: Christopher Alfeld <calfeld@qualys.com>

Predicates Rules User Manual
============================

Christopher Alfeld <calfeld@qualys.com><br>

**Warning:** This document describes a feature that is under ongoing development.  Only portions of what is described are currently available and all of it is subject to change.  This document is intended for IronBee developers, those wanting to play on the bleeding edge, and those interested in upcoming major features.

Introduction
------------

The Predicate Rule system provides an alternative rule injection system for IronBee.  That is, it provides an alternative way to determine which rules should fire in a phase and does not change the metadata or actions available to rules.  The system associates a predicate *expression* with a rule and fires the rule if and only if the predicate expression evaluates to true.  The major advantages over the traditional rule system are:

* Composition: The expressions allow easy composition of other expressions.  Composition allows logic to be shared among rules.  It also allows easy use of boolean operators such as 'and' and 'or' to compose logic.

* Performance: Common subexpressions are merged, including across rules in the same phase.  Such merging can have significant performance benefits, especially in rule sets with significant common logic.  In addition, the system provides a framework for expression transformation allowing further optimizations.

* Domain specific language: The expression syntax is oriented at easy machine generation and the use of domain specific languages for generating it.  While it is possible to write predicate expressions directly, support in higher level languages such as Lua can greatly facilitate expression rule logic.

The major disadvantage over the traditional rule system are:

* Order Independence: Within a phase, no order guarantees are provided for rule firings.  There is no way to specify that a rule should fire before another rule.

* Side Effects: Side effects of rules, such as setting variables, may not be visible to other predicate rules within the same phase.

* Feature Delay: The predicate system does provide mechanisms to call any operator defined for IronBee, however, as operators are written for the traditional rule system, they may have features that do not map into the predicate rule system.  As such, features introduced in IronBee or from other modules may not be initially available in the predicate system.

The predicate system is conceptually divided into three parts:

* Front End: The front end is a domain specific language for generating predicate expressions.  The front end is concerned with providing an easy and expressive syntax.
* Graph: The graph component gathers all predicate expressions and merges common subexpression, forming a directed acyclic graph (DAG).  It then executes a variety of validation and transformations on the DAG.  Each input predicate expression corresponds to a *root* node in the DAG.
* Evaluation: At each transaction, all nodes in the DAG for that phase are evaluated.  For each root node that is true, the corresponding rules are injected.

A reference guide to the available functions (Calls) is at `reference.txt`.

Expressions
-----------

An expression in the predicate system is a tree.  Each internal node represents a pure function call.  Its children plus the current IronBee transaction are the inputs to that function.  Leaf nodes are either literal values or function calls that have no inputs besides the current transaction.  Nodes representing function calls are known as *Call* nodes.  Nodes can have multiple values.  The values of a literal node is fixed.  The values of a Call node is the return value of the corresponding function on its inputs.    In addition, nodes can be "finished" or "unfinished".  A finished node will never add more values.  An unfinished node may add additional values if called again (e.g., at a later phase when additional information is available).  Nodes may never remove values or change already added values.  Values are IronBee fields.  IronBee fields provide a basic typing system including signed integers, floating point, strings and collections.  A node with no values is interpreted as false; a node with any values is interpreted as true.  The ultimate purpose of an expression is to express the semantics: if the root node is true, run a rule.

A node is said to be "simple" or "have a simple value" if it is finished and has 0 or 1 values.

All literals are simple.  Currently, the empty list (written 'null'), string, and numeric (integral and floating) literals are supported.

Many calls operate in a "map-compact" style, meaning that the call applies a per-value function to each value in the input which adds 0 or 1 values to the output.  For example, the 'rx' call applies a regular expression to each input and adds the capture collection to the output if a match is found.

Expressions are usually represented textually via S-Expressions (sexpr).  The grammar of a predicate sexpr is given below, but first an example:

    (and (gt (atoi (field 'Content-Length')) 0) (streq 'GET' (field 'Request-Method')))

The sexpr above represents the logic "the request is a GET and the Content-Length header is greater than 0".

The sexpr grammar is:

    root := call
    expr := call | literal
    call := '(' + name + *(' ' + expr) + ')'
    name := /^[A-Z0-9_]+$/
    literal := null | string | float | integer
    null := 'null'
    string := '\'' + *(/[^'\\]/ | '\\\\' | '\\'') + '\''
    integer := /^-?[0-9]+$/
    float := /^-?[0-9]+(\.[0-9]+)?$/

String literals are surrounded by single quotes and only support two escapes: backslash single quote for single quote and backslash backslash for backslash.

Note that the root must be a Call node.

The actual parser is not quite as strict as the grammar above: it allows for additional whitespace except in literals and names.

The most important performance consideration when using expressions is that *common subexpressions are merged*.  For example, if `(gt (atoi (field 'Content-Length')) 0)` appears in 100 predicate expression, you only pay the runtime cost of it once (per phase).  It is important to build expressions out of common building blocks and any Front End should support this.

The semantics of the expression language is defined by the types of Call node it supports.  However, rule writers will usually be writing in Front End languages that provide syntactic constructs and may define functions in terms of others.  For example, for boolean expressions, only `and`, 'or', and `not` are available.  However, the front end can provide an `nor` by converting `a nor b` to `(not (or a b))`.

The predicate system comes with a set of *Standard* Calls.  Modules may define additional Calls.  The current set of standard calls is documented in `reference.txt`.  Standard calls include boolean connectives, data manipulation, and IronBee operators.

The front end is documented in `frontend.md`.

Action and Operator
-------------------

The Predicate system may be used in rules in two ways: the `predicate` action or the `@predicate` operator.

The preferred way is via the `predicate` action.  The `predicate` action indicates that the rule should be claimed by Predicate and injected if appropriate.  The parameter to the action is the S-Expression that determines whether to inject the rule.

**Warning: `@predicate` is a future feature that is not yet implemented.**

The `@predicate` operator allows predicate expressions to be mixed with traditional rules.  The operator takes a single argument, an S-Expression, and is true if and only if that S-Expression is true.  Thus, the operator allows combining an predicate expression with normal rule logic.  However, because the rule is part of the normal rule system, much of the performance benefit of Predicate is lost.  It will still merge subexpressions with those of other Predicate expressions (whether from operator or action), but the rule will always be evaluated.

The `set_predicate_vars` Action
---------------------------

Rules using the `predicate` action can gain access to the name and value of each value in the top node by:

1. Add the `set_predicate_vars` action to the **beginning** of the actions.
2. Use the `VALUE` and `VALUE_NAME` vars.

The actions will be fired for every value on the valuelist of the top node.  The `set_predicate_vars` action will cause the `PREDICATE_VALUE` and `PREDICATE_VALUE_NAME` vars to be set for each value.

Warning: `set_predicate_vars` has mild performance cost and should only be used when needed.
