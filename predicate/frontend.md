Title:  IronBee Predicate Lua Front End Manual
Author: Christopher Alfeld <calfeld@qualys.com>

Predicates Lua Front End Manual
===============================

Christopher Alfeld <calfeld@qualys.com><br>

**Warning:** This document describes a feature that is under ongoing development.  Only portions of what is described are currently available and all of it is subject to change.  This document is intended for IronBee developers, those wanting to play on the bleeding edge, and those interested in upcoming major features.

Below, it is assumed that the module is loaded as `P`.  It may be loaded as `Predicate` in your environment.

Introduction
------------

The Lua Front End provides an API for easy construction of Predicate expressions.

The easiest way to use in IronBee is via waggle.  Add `predicate(...)` to your signatures and then `LuaInclude` the file.  Predicate will be available as both `P` and `Predicate`.  Be sure to load `ibmod_predicate` before the `LuaInclude`.  If you are using it outside of IronBee, then load `../lua/ironbee/predicate.lua` as a module.

This document describes various aspects of the frontend.  Documentation of the available calls and frontend specific details of them are described in reference.txt.

Examples
--------

    local long_uri = P.Gt(P.Length(P.Field('REQUEST_URI')), 1000)

The variable `long_uri` is an object that can be called to produce an sexpr.

    = long_uri() --> (gt (length (field 'REQUEST_URI')) 1000)

In this simple example, the front end may not appear to be very helpful; the Lua code is longer than the sexpr it produces.  Indeed, for short expressions it may be faster/easier to write them out directly.  The front end supports this with `P.Raw`:

    = P.Raw("(gt (length (field 'REQUEST_URI')) 1000)")() --> (gt (length (field 'REQUEST_URI')) 1000)

The value of the front end increases with expression complexity.  Consider, e.g., a parameter expression:

    local function long(a, long_length)
      long_length = long_length or 1000
      return P.Lt(long_length, P.Length(a))
    end

Here we define a function, `long`, that produces an expression.  This function can now be reused, e.g.:

    local function header(header_name)
      return P.Sub(header_name, P.Field('REQUEST_HEADERS'))
    end
    local long_request = P.Or(
      long(header('Content-Type')),
      long(header('Host'))
    )
    = long_request() --> (or (lt 1000 (length (sub  'Content-Type' (field 'REQUEST_HEADERS')))) (lt 1000 (length (sub 'HOST' (field 'REQUEST_HEADERS')))))

The resulting sexpr is significantly longer.

The front end provides a method for formatting sexprs for human consumption.

    print(P.pp(long_request()))

prints

    (or
      (lt
        1000
        (length
          (sub
            'Content-Type'
            (field 'REQUEST_HEADERS')
          )
        )
      )
      (lt
        1000
        (length
          (sub
            'Host'
            (field 'REQUEST_HEADERS')
          )
        )
      )
    )

The above example illustrates another feature of Predicate, the automatic conversion of Lua types.  Numbers and Lua strings were automatically converted when used as parameters to functions.  This conversion includes proper escaping:

    = P.Cat("An 'important' example")() --> (cat 'An \'important\' example')

Boolean types are also converted:

    = P.Cat(true, false)() --> (list (true) (false))

Boolean expressions can be written using operators:

    = (P.Field('A') + P.Field('B'))() --> (and (field 'A') (field 'B'))

Note that addition was transformed into `and`.

Some calls such as `Length` make sense as methods.  The front end provides a number of these as alternatives:

    local function long(a, long_length)
      long_length = long_length or 1000
      return P.Gt(a:length(), long_length)
    end

    local function header(header_name)
      return P.Field('REQUEST_HEADERS'):sub(header_name)
    end

Finally, the front end adds a number of calls that do not exist in the back end but can be implemented in terms of other calls:

    = P.Xor(P.Field('A'), P.Field('B'))() --> (or (and (field 'A') (not (field 'B'))) (and (field 'B') (not (field 'A'))))

Reference
---------

**Warning**: Both Predicate and the Front End are under active development.  Expect this section to grow or change frequently.

**Expression Objects**

Most `P.*` methods return an expression object.  All expression objects support conversion to an sexpr string by calling and the operators (see below).  Expression objects are further divided into String, Float, Integer, Call, Null, and Raw.

- String expression objects support String methods (see below).
- Call expression objects support String method and Call methods (see below).
- Float, Integer, Null and Raw expression objects do not have any additional methods.

**Naming Conventions**

Functions in predicate that logically correspond to portions of the expression tree are capitalized; utility functions are not.  The gray case is `P.from_lua`.

Methods of expression objects are lowercased to more visually distinguish them from their capitalized alternatives.

**Type Conversion**

All arguments to `P.*` methods are converted as needed via `P.from_lua`.  Lua numbers and strings are converted into String objects and booleans are converted into `P.True` or `P.False` as appropriate.  In some cases, explicit conversion will be needed, e.g., for operators.

For example:

    -- Not valid!
    a = true + false
    -- Valid
    a = P.True + false

    -- Not valid!
    a = "foo" + "bar"
    -- Valid
    a = P.S("foo") + "bar"
    -- Valid
    a = P.from_lua("foo") + "bar"

**Operators**

All expression objects support the following operators:

Form            | Meaning                   | Equivalent            |
--------------- | ------------------------- | --------------------- |
`a1 + ... + an` | Logical and               | `P.and(a1, ..., an)`  |
`a1 / ... / an` | Logical or                | `P.or(a1, ..., an)`   |
`-a`            | Logical not               | `P.not(a)`            |
`a - b`         | `a` and not `b`           | `a + (-b)`            |
`a ^ b`         | Exactly one of `a` or `b` | `(a - b) + (b - a)`   |

**Introspection**

It is possible to look inside expression objects.

Which       | Member     | Meaning                                                               |
----------- | ---------- | -----------------------------------------------                       |
All         | `type`     | Type of object: `null`, `raw`, `call`, `string`, `float`, `integer` |
String, Raw | `value`    | Value as Lua string                                                   |
Call        | `name`     | Name of Call node as Lua string                                       |
Call        | `children` | Array of child objects (arguments)                                    |

**Utilities**

Utility         | Meaning                                      |
--------------- | -------------------------------------------- |
`P.pp(s)`       | Format sexpr `s` for easy human reading      |
`P.from_lua(a)` | Convert `a` to appropriate expression object |
`P.to_lua(a)`   | Convert `a` to appropriate lua object        |

`P.pp` returns a string.

`P.from_lua(a)` returns converted strings, numbers, and booleans as described above and returns anything else directly.

`P.to_lua(a)` returns two values, the Lua value and whether the conversion was successful.  Conversion is only successful for String and Null expression objects and the `P.True` and `P.False`.  Note that nothing is converted to numbers.

**Constants**

The Null value and Calls with no parameters are represented as constants.

Constant   | Meaning        | Equivalent    |
---------- | -------------- | ------------- |
`P.Null`   | Null value     | None          |
`P.True`   | True function  | P.C('true')   |
`P.False`  | False function | P.C('false')  |

