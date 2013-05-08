Title:  IronBee Predicate Standard Calls Reference
Author: Christopher Alfeld <calfeld@qualys.com>

Predicates Rules User Manual
============================

Christopher Alfeld <calfeld@qualys.com><br>

**Warning:** This document describes a feature that is under ongoing development.  Only portions of what is described are currently available and all of it is subject to change.  This document is intended for IronBee developers, those wanting to play on the bleeding edge, and those interested in upcoming major features.

Boolean
-------

- `(true)`: Always true.
- `(false)` Always false.
- `(and a1 a2 ...)`: Logical and.
- `(or a1 a2 ...)`: Logical or.

And and Or will transform themselves into equivalent expressions with arguments in a canonical order.  This transformation will allow, e.g., `(and a b)` to merge with `(and b a)`.  If it is possible to determine the value independent of context, they will replace themselves with the appropriate value.  E.g., `(and a 'foo')` will transform itself to True regardless of what `a` is.

- `(not a1)`: Logical not.

Not will transform itself to True or False when possible.

- `(if x t f)`: If `x`, then `t` else `f`.

If `x` is a literal, will transform itself to `t` or `f`, appropriately.

Data
----

- `(field field_name)`: Value of a data field.

- `(sub subfield_name collection)`: Subvalue of a collection.
- `(suball subfield_name collection)`: List of subvalues of a collection.

Sub returns the first found member of the collection with the given name.  SubAll returns a list of all members with the given name.  Both return Null if there is no member with the given name.

- `(list a...)`: Construct a list from one or more children.
- `(set_name n v)`: Construct a named value with name `n` and value `v`.

List and SetName can be used together to build collections, e.g.,

    (list (set_name 'foo' 1) (set_name 'bar' 2))

IronBee
-------

- `(operator opname literal_argument dynamic_argument)`: Generic access to any operator.

Operator requires both `opname` and `literal_argument` to be literals.

Some IronBee operators are provided directly via SpecificOperator.  All of these take a literal argument and a dynamic argument and transform into appropriate Operator nodes.

- `streq`
- `istreq`
- `rx`
- `eq`
- `ne`
- `gt`
- `lt`
- `ge`
- `le`

- `(transformation tfnname dynamic_argument)`: Generic access to any transformation.

Transformation requires `tfnname` to be a string literal.

Some IronBee transformations are provided directly via SpecificTransformations.  All of these take a dynamic argument and transform into appropriate transformation nodes.

- `normalizePathWin`
- `normalizePath`
- `htmlEntityDecode`
- `urlDecode`
- `min`
- `max`
- `count`
- `length`
- `compressWhitespace`
- `removeWhitespace`
- `trim`
- `trimRight`
- `trimLeft`
- `lowercase`
- `name`
- `names`
- `round`
- `ceil`
- `floor`
- `toString`
- `toInteger`
- `toFloat`
