Introduction
============

**Warning:** This document represents a desired future not a realized present.  Implementation is currently under active development and is incomplete.

This document serves as the primary reference for Predicate.  It describes the fundamental types involved and provides a complete list of available functions.

Value
=====

A Value is a type, a value, and an optional name.[^fn-value-name]  The types supported by Predicate are:

- **Bytestring**: A string stored as a length and byte sequence.  May contain embedded NULs.  The type of string literals.
- **Number**: A signed integer.
- **Float**: A floating pointer number.
- **Collection**: A map of keys to values.
- **List**: A sequence of values.

Numeric literals are either numbers or floats depending on whether they contain a period.  Thus `1.0` is a float but `1` is a number.

Collections and lists are not clearly distinguished.  Any list may be treated as a collection (e.g., as an argument to `sub`), some collections, however, may not be treated as lists[^fn-value-collection].  Collections are only accessed via key lookup.  Lists are sequences.  Both collections and lists may contain multiple Values with the same name.  In C++ parlance, lists are like `std::list` collections are like `std::multimap`.

[^fn-value-name]: Value's correspond to IronBee fields.  However, many Value's in Predicate -- unlike in traditional IronBee -- do not make use of names.  There is a long term plan to separate the run-time type system and name-value pair facets of fields.  If this happens, Value will split into Value and NamedValue in Predicate.

[^fn-value-collection]: Collections that are not lists usually occur when modules provides collections via generator functions rather than a lists as values.

ValueList
=========

A Value list is a list of Values that begins empty and only has values added, never changed or removed.  ValueList's are the result type of every node type in Predicate (every literal and function call).

EvalContext
===========

The EvalContext (evaluation context) is provided to each function as part of its call and represents the external context predicate is evaluating.  In IronBee, this is the current transaction.  Functions that use the EvalContext are responsible for translating the knowledge it contains into Predicate terms.

Literals are not allowed to use EvalContext.

Environment
=======

The Environment is the larger environment Predicate runs in.  It does not provide varying knowledge to be evaluated by Predicate, but may provide a larger context as well as the resources functions need.  In IronBee, this is the engine.

Developer Types
===============

Predicate   | IronBee     | C Type              | C++ Type
----------- | ----------- | ------------------- | ----------------------------------
Value       | Field       | `const ib_field_t*` | `IronBee::ConstField`
ValueList   | List        | `ib_list_t*`        | `IronBee::List<IronBee::ConstField>`
Environment | Engine      | `ib_engine_t*`      | `IronBee::Engine`
EvalContext | Transaction | `const ib_tx_t*`    | `IronBee::ConstTransaction`

Nodes
=====

Every literal and function call in Predicate is represented as a node in a direct acyclic graph.  Literals are leaves in the graph, while function calls have out edges to the Nodes representing their arguments.  Nodes may have multiple in edges due to subexpression merging.  I.e., if two different Predicate rules involve the expression `(var 'ARGS')`, there will be a single node representing `(var 'ARGS')` with in edges for the expressions of both of those rules.

Every Predicate rule has a "top node" that determines whether the rule should fire.  The ultimate purpose of Predicate is to termine which top Nodes are truthy.

Nodes have two items of state: a ValueList and a finished flag.  A node is said to be "unfinished" if the finished flag is false, and "finished" if it is true.

A Node is falsy if its ValueList is empty and truthy otherwise.  As ValueLists may only add items, a Node may change from falsy to truthy but not the other way around.  As such, falsy Nodes are often treated as undetermined until they are finished.

At the beginning of each transaction, Nodes begin with an empty ValueList and unfinished.  At each phase, Predicate tries to determine the truthiness of all the top Nodes.  It may evaluate Nodes to do so.  If a Node is unfinished, it may execute arbitrary code to add values and/or change to finished.

Predicate tries to minimize the amount of work done.  As such, there is no guarantee that a Node will be evaluated at every phase, or even at all.

Notable Limitations
===================

Functional
----------

Predicate is purely functional and does badly when interacting with parts of the system that change.  For example, a data field that changes value within a transaction (perhaps due to some module or via rule actions) is difficult or impossible to use in Predicate.

No Unbound Variables
--------------------

Predicate does not provide for expressions with unbound variables and, as such has only limited operations such as map, reduce, and select (see, e.g., filters).  This limitation significantly simplifies Predicate, especiall pre-evaluation optimization, but is also a major reduction in expressiveness.

Unbound variables is an area of ongoing discussion and research.

Terminology
===========

Simple
: A Node is simple if its ValueList is empty or contains exactly one value.  Functions are simple if their Node is always simple.

Literal
: A Node is literal if it is a literal.  Literals are always simple and always have the same value.

Lua Front End
=============

The primary front end for Predicate is written in Lua and intended to be used with Waggle.  This document includes information on how functions are exposed in the front end.

All functions are exposed directly via `P.Function(...)`, e.g., `P.Eq("foo", P.Cat("foo", "bar", "baz"))`.

Functions marked **String Method** are also available as string members, e.g., `P.Var("REQUEST_URI"):length()", with the string passed in as the last argument.

Functions marked **Call Method** are also available as members of any function call node, e.g., `P.Var("ARGS"):scatter():eq("foo")`, with the call passed in as the last argument.

Literals can be represented via `P.String(s)` and `P.Number(n)` and the `P.Null` constant.  Arbitrary call functions can be represented via `P.Call(name, ...)`.  Raw sexpr text can be directly inserted via `P.Raw(text)`.  Shortcuts for the above are available as `P.S`, `P.N`, `P.C`, and `P.R`, respectively.

In many cases, Lua numbers and strings will be automatically converted to Predicate number and string nodes.

Functions
=========

**Conventions**

Arguments
: Arguments that must be literal are capitalized.
: Arguments that may be dynamic are not.

Boolean
-------

All of these functions will evaluate at transformation time if possible.  E.g., `(and (false) v)` will transform into `(false)`; `(if (true) t f)` will transform into `t`.

Furthermore, some amount of simplification will occur.  `true` and `false` will transform into `['']` and `[]`, respectively.  `or` and `and` will also reorder their arguments into a canonical order to allow further subexpression merging, e.g., `(or a b)` will merge with `(or b a)`.

In contrast, the short circuited versions, `andSC` and `orSC` do not reorder.  They will incorporate arguments of the same function as them, but not those of the non-short-circuited versions.

The front end provides some synthetic functions and operator overloads:

- `a + b` is equivalent to `P.And(a, b)`.
- `a / b` is equivalent to `P.Or(a, b)`.
- `-a` is equivalent to `P.Not(a)`.
- `a - b` is equivalent to `a + (-b)`
- `P.Xor(a, b)` is equivalent to `(a - b) + (b - a)`.
- `a ^ b` is equivalent to `P.Xor(a, b)`.
- `P.Nand(a, b)` is equivalent to `-(a + b)`.
- `P.Nor(a, b)` is equivalent to `-(a / b)`.
- `P.Nxor(a, b)` is equivalent to `-(a ^ b)`.

**`(true)`**

Result
: None (see below)

Finished
: Always

Transformations
: Always into the empty string literal.

Front End
: Available as constant `P.True`.

**`(false)`**

Result
: None (see below)

Finished
: Always

Transformations
: Always into `null`.

Front End
: Available as constant `P.False`.

**`(and ...)`**

Result
: truthy if all arguments are truthy.
: falsy if any child is falsy.

Finished
: All arguments are truthy or all arguments are finished.

See Also
: `andSC`

Transformations
: Will reorder children into a canonical order to aid in subexpression merging.
: If any child is a the null literal, will replace itself with `null`.
: Will remove any truthy literal children.

Front End
: Available via `+`.

**`(or ...)`**

Result
: truthy if any child is truthy.
: falsy if all arguments are falsy.

Finished
: Any child is truthy or all arguments are finished.

Transformations
: Will reorder children into a canonical order to aid in subexpression merging.
: If any child is a truthy literal, will replace itself with a truthy value.
: Will remove any null literal children.

Front End
: Available via `/`.

**`(not a)`**

Result
: truthy if `a` is falsy.
: falsy if `a` is truthy.

Finished
: `a` is truthy or `a` is finished.

Transformations
: If `a` is literal, will replace itself with a truthy or falsy literal value as appropriate.

**`(if p t f)`**

Result
: `t` if `p` is truthy.
: `f` if `p` is falsy and finished.
: `[]` if `p` is falsy and unfinished.

Finished
: `p` is truthy and `t` is finished.
: `p` is falsy and finished and `f` is finished.

Transformations
: If `p` is literal, will replace itself with `t` or `f` as appropriate.

Front End
: If `p` is a literal, the front end will evaluate the if expression.

**`(andSC ...)`**

Result
: `(true)` if all arguments are truthy.
: `(false)` if any child is falsy.

Finished
: All arguments are truthy or all arguments are finished.

Notes
: Will only evaluate first argument until that argument is truthy or finished.  If truthy, will then move on to second argument, and so forth.
: Prefer `and` except in cases when short circuiting is highly desirable, e.g., with `fast`.

See Also
: `and`

**`(orSC ...)`**

Result
: `(true)` if any child is truthy.
: `(false)` if all arguments are falsy.

Finished
: Any child is truthy or all arguments are finished.

Notes
: Will only evaluate first argument until that argument is falsy and finished.  Will then move on to second argument, and so forth.
: Prefer `or` except in cases when short circuiting is highly desirable.

See Also
: `or`

ValueList
---------

These functions are for manipulating ValueLists.

**`(setName N v)`**

Result
: `v` with name of every value of `v` set to `N`.

Finished
: `v` is finished.

Front End
: **Call Method**

**`(pushName v)`**

Result
: `v` except that for each list value of `v`, the subvalues of that list will now have the same name as the list.  I.e., it pushes the name of a parent list to its children.

Finished
: `v` is finished.

Notes
: Useful with `flatten` to flatten a valuelist but preserve the names of the lists, e.g., `(flatten (pushName v))`.

**`(cat ...)`**

Result
: Values of leftmost unfinished argument preceded by values of all finished arguments before it.  That is, Cat will work left to right, adding values from finished argument along with values from the leftmost unfinished argument, but no values after the leftmost unfinished argument.

Finished
: All arguments are finished.

Notes
: `cat` waits until all arguments are finished as it is not allowed to add values in the middle of its value list.
: A future version of `cat` may begin adding values when possible.  E.g., `(cat a b)` could begin adding values of `a` as soon as they appear, although it would have to wait for `a` to finish before adding values of `b`.
: `cat` could add values from any argument immediately if it were not concerned with preserving order.  An additional, differently named, function to do just that may be added in the future.

Transformations
: If a child is a null literal, it will be removed.
: If `cat` has a single argument, it will replace itself with its argument.
: If `cat` has no arguments, it will replace itself with false.

**`(scatter a)`**

Result
: `[]` if `a` is not finished.
: Else `[]` if `a` is not simple or (single) value of `a` is not a list.
: Else values of (single) value of `a`.

Finished
: `a` is finished or `a` has is not simple or `a` is simple and value if not a list.

Transformations
: If `a` is a literal, will replace itself with a falsy value.

Front End
: **Call Method**

**`(gather a)`**

Result
: `[]` if `a` is not finished.
: Else `[v]` where `v` is a list value containing the values of `a`.

Finished
: `a` is finished.

**`(first v)`**

Result
: `[]` if `v` is empty.
: Else `[x]` where `x` is the first value of `v`.

Finished
: `v` is finished.
: `v` has one or more elements.

Transformations
: If `v` is literal, will replace itself with `v`.

Front End
: **Call Method**

**`(rest v)`**

Result
: `[]` if `v` has one or fewer values.
: `[...]` where `...` is all but the first value of `v`.

Finished
: `v` is finished.

Transformations
: If `v` is a literal, will replace itself with `null`.

Front End
: **Call Method**

**`(nth N v)`**

Result
: `[]` if `v` has less than `N` elements.
: `[x]` where `x` is the `N`th element of `v` (first element is index 1)`.

Finished
: `v` is finished.
: `v` has `N` or more elements.

Transformations
: If `v` is a literal, will replace itself with null (`N > 1`) or `v` (`N == 1`).

Front End
: **Call Method**

**`(flatten v)`**

Result
: The concatenation of all values of values of `v` where non-list values of `v` are treated as lists of size 1.

Finished
: When `v` is finished.

Transformations
: If `v` is a (non-list) literal, will replace itself with `v`.

**`(focus N v)`**

Result
: For each list value of `v` that contains a subvalue named `N`, that subvalue with its named changed to `N`.

Finished
: When `v` is finished.

String
------

These functions manipulate strings.  All of them operate on every string value of their dynamic input and discard any non-string values.

**`(stringReplaceRx E R v)`**

Result
: Every string value of `v` with any substrings matching `E` replaced with `R`.  See `boost::regex` for details on expression and replacement string syntax.

Finished
: When `v` is finished.

Predicate Predicates
--------------------

These functions have `(true)` or `(false)` (aka `[]`) as their result.

**`(isLonger N v)`**

Result
: `[]` if `v` has `N` or fewer values.
: Else `(true)`.

Finished
: `v` is finished.
: `v` has more than `N` values.

Transformations
: If `v` is literal, will replace itself with a truthy or falsy literal depending on `N`.

Front End
: **Call Method**

**`(isLiteral v)`**

Result
: None (see below).

Finished
: Always

Transformations
: Will replace itself with a truthy value if `v` is a literal and a falsy value otherwise.

Front End
: **Call Method**

**`(isSimple v)`**

Result
: Truthy if `v` has at most one value.
: Else falsy.

Finished
: `v` is finished.
: `v` is not simple.

Transformations
: If `v` is a literal, will replace itself with a truthy value.

Front End
: **Call Method**

**`(isFinished v)`**

Result
: `[]` if `v` is not finished.
: Else `(true)`.

Finished
: `v` is finished.

Transformations
: If `v` is a literal, will replace itself with a truthy value.

Front End
: **Call Method**

**`(isHomogeneous v)`**

Result
: `[]` if `v` is not finished or `v` has values of different types.
: Else `(true)`.

Finished
: `v` is finished.
: `v` has values of different types.

Transformations
: If `v` is a literal, will replace itself with a truthy value.

Front End
: **Call Method**

**`(isHomogeneous ...)` [Future]**

Synonym
: `(isHomogeneous (cat ...))`

**`(isComplete a b)` [Future]**

Result
: `[]` if `a` or `b` is unfinished.
: Else `(true)` if length `a` is equal to the length of `b`.
: Else `(false)`.

Finished
: `a` and `b` are finished.
: `a` is finished and `b` has more values than `a`.

Notes
: Intended use is with maplike functions to see if the per-value function succeeded on every value.  For example: `(isComplete v (rx 'foo' v))` is true iff every value of `v` matches the regular expression 'foo'.

Front End
: **Call Method**
: E.g., `P.Rx('foo', v):isComplete(v)`.

Filters
-------

All of these functions are of the following form:

**`(X F v)`**

Result
: `[...]` where `...` is the values of `v` that match the filter described by `F`.

Finished
: `v` is finished.

Front End
: **Call Method**

**`(eq F v)`**

Filter
: Equal in type and value to filter.

Notes
: Floating point equality is tricky and unlike to be what you want.  A `near` filter may be added in the future.
: Lists are currently never equal.

**`(ne F v)`**

Filter
: Not equal in type or value to filter.

Notes
: `ne` is the exact opposite of `eq`.  E.g., lists are always `ne`.

**Warning on `lt`, `le`, `gt`, `ge`**

Filters based on asymmetric operators are tricky.  Should `(lt F v)` be elements of `v` that are less than `F` or greater than `F` (as `F` appears on the left).  After much debate, the former was chosen to facilitate more natural any-of-the-following expressions.  For example, `(gt 100 (length (sub 'Host' (var 'REQUEST_HEADERS'))))` should express the notion of a long 'Host' header, not a small one, and should be true if any 'Host' header is long in the case of multiples.

**`(lt F v)`**

Filter
: Number or float and less than filter.

**`(le F v)`**

Filter
: Number or float and less than or equal to filter.

**`(gt F v)`**

Filter
: Number or float and greater than filter.

**`(ge F v)`**

Filter
: Number or float and greater than or equal to filter.

**`(typed F v)`**

Filter
: Type as described by `F`.

Notes
: `F` must be one of 'number', 'float', 'list', 'string'.

**`(named F v)`**

Filter
: Has name equal to value of filter.

**`(namedi F v)`**

Filter
: Has name equal to value of filter, case insensitive.

** `(sub F v)`**

Synonym
: `(namedi F v)`

**`(namedRx F v)`**

Filter
: Has name matching regexp `F`.

Phase
-----

In these function, `P` is a string literal describing an IronBee phase.  Valid values are:

- `REQUEST_HEADER`
- `REQUEST`
- `RESPONSE_HEADER`
- `RESPONSE`
- `POSTPROCESS`
- `REQUEST_HEADER_STREAM`
- `REQUEST_BODY_STREAM`
- `RESPONSE_HEADER_STREAM`
- `RESPONSE_BODY_STREAM`

**`(waitPhase P v)`**

Result
: `[]` if has not been evaluated during phase `P` yet.
: Else `v`.

Finished
: Has been evaluated in phase `P` and `v` is finished.

Notes
: `v` is not evaluated via this path until phase `P`.  I.e., `v` will not be evaluated before phase `P` because of this expression, although it may be if used outside a `waitPhase` somewhere else.
: This function is primarily intended for performance tweaking although it may also be used to delay values to a later phase.

Front End
: **Call Method**

**`(finishPhase P v)`**

Result
: `v` until evaluated at phase `P`.
: Else `w` where `w` is the value of `v` when evaluated at phase `P`.

Finished
: `v` is finished.
: Has been evaluated at phase `P`.

Notes
: This function is primarily intended for performance although it may also be used to ignore later values.

Front End
: **Call Method**

IronBee
-------

**`(ask P v)`**

Result
: `[...]` where `...` is the result of asking each value of `v` for its values given parameter `P`.

Finished
: `v` is finished.

Notes
: Values created by other modules are allowed to define their value dynamically.  Such dynamic values are automatically used correctly by `var`, `sub`, `scatter`, etc.  However, these pass no argument in.  `ask` may be used to pass an argument in.

Front End
: **Call Method**

**`(operator N S d)`**

Result
: Executes operator named `N` with static argument `S` on each value of `d`.  If result is true, then adds the capture field as a value if possible and an empty string if not.

Finished
: `d` is finished.

Notes
: You will often not need to use `operator` directly.  The front end or templates will provide functions that directly call a specific IronBee operator.

Front End
: Provides most specific operators as `P.N(S, d)`.  These are also **Call Methods**.

**`(foperator N S d)`**

Result
: As `operator`, but instead of adding capture, it adds the input.

Finished
: `d` is finished.

Notes
: You will often not need to use `foperator` directly.  The front end or templates will provide functions that directly call a specific IronBee operator.

Front End
: Provides most specific operators as `P.FN(S, d)`.  These are also **Call Methods**.

**`(transformation N d)`**

Result
: `[...]` where `...` is each value of `d` transformed by the transformation named `N`.

Finished
: `d` is finished.

Notes
: You will often not need to use `transformation` directly.  The front end or templates will provide functions that directly call a specific IronBee operator.

Front End
: Provides most specific transformations as `P.N(d)`.  These are also **Call Methods**.

**`(var N)`**

Result
: `[]` if no var named `N`.
: `[v]` where `v` is the value of the var named `N` and 'v' is either a dynamic list or not a list.
: `[...]` where '...' is the values of the list var named `N`.

Finished
: Except for non-dynamic lists, as soon as var has a value.  For dynamic lists, it will be finished if the var has a defined final phase that is at or before the current phase.

**`(var N W F)`**

Result
: `[]` until phase `W`.
: Else `[v]` where `v` is the value of the data field named `N`.

Finished
: At phase `E`.

Notes
: See Phase section for acceptable values of `W` and `F`.
: This function should **not** be confused with `(waitPhase W (finishPhase E (field N)))`.  The latter enforces that the value does not change after phase `E`, whereas this function indicates that it can *assume* that the field will not change after phase `E` and can thus finish.

**`(field N)`**

Synonym
: `(var N)`

**`(field N W F)` **

Synonym
: `(var N W F)`

Fast \[Future]
--------------

TODO: Documentation.

**`(fast P)`**

Result
: `[]` until pattern `P` appears in traffic.
: Then `(true)`.

Finished
: When `(true)`.

Notes
: At present, requires that a Fast automata is loaded via the `PredicateFastAutomata` directive.  In the future, a version that generates the automata at configuration time may be present.
: The advantage of this function is that, properly used, all occurrences of  it will have their patterns merged into a single patterned Aho-Corasick automata which will efficiently determine which occurrences are true.

Development
-----------

**`(p ...)`**

Result
: Value of last argument.

Finished
: Last argument is finished.

Notes
: Will log each argument to stderr each time evaluated.

Front End
: **Call Method**

**`(sequence S E D)`**

Result
: `[...]` where ... is the numbers `S`, `S+D`, `S+2D`, ..., `E`, inclusive.

Finished
: When the entire range is produced.

Notes
: Will add one value each time evaluated.
: `D` can be negative.
: Infinite ranges can be done by having E be on the wrong side of `S`.  E.g., `E` < `S` and `D` > 0.
: Infinite constant ranges are also possible by having `D` == 0.

**`(sequence S E)`**

Synonym
: `(sequence S E 1)`

**`(sequence S)`**

Synonym
: `(sequence S E 1)` where `E` is some value less than `S`.

**`(identity v)`**

Result
: `[...]` where ... is the result of `v`.

Finished
: When `v` is finished.

Notes
: Does not transform.  In contrast, `(cat v)` is semantically identical, but transforms itself into `v`.

Templates
---------

Templates provide basic user defined substitution transformations.  They permit complex expressions to be expressed as simple expressions, allowing for improved error messages and fewer bytes in certain forms.  Ultimately, they expand out to full expressions tree, have subexpressions merged with all other expressions, and are treated no differently that had they been written out fully to begin with.

Templates are defined externally, e.g., via an IronBee directive.  As an example:

    PredicateDefine "requestHeader" "name" "(sub (ref 'name') (field 'REQUEST_HEADERS'))"

The `requestHeader` template then be used as a function, e.g., `(requestHeader 'Host')` which would be expanded to `(sub 'Host' (field 'REQUEST_HEADERS'))`.

**`(T ...)`**

Where `T` is some user defined template.

Result
: N/A (see below)

Finished
: N/A (see below)

Transformations
: Will replace itself with a deep copy of its associated expression tree.  Will then go through that copy and replace any `ref` nodes with values taken from its arguments.

**`(ref S)`**

Result
: N/A (see below)

Finished
: N/A (see below)

Notes
: Will be replaced with appropriate expression during template transformations.
