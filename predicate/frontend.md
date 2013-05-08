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

Examples
--------

    local long_uri = P.Gt(P.Length(P.Field('REQUEST_URI')), 1000)

The variable `long_uri` is an object that can be called to produce an sexpr.

    = long_uri() --> (gt (length (field 'REQUEST_URI')) '1000')

In this simple example, the front end may not appear to be very helpful; the Lua code is longer than the sexpr it produces.  Indeed, for short expressions it may be faster/easier to write them out directly.  The front end supports this with `P.Raw`:

    = P.Raw("(gt (length (field 'REQUEST_URI')) '1000')")() --> (gt (length (field 'REQUEST_URI')) '1000')

The value of the front end increases with expression complexity.  Consider, e.g., a parameter expression:

    local function long(a, long_length)
      long_length = long_length or 1000
      return P.Lt(long_length, P.Length(a))
    end

Here we define a function, `long`, that produces an expression.  This function can now be reused, e.g.:

    local function header(header_name)
      return P.Sub(P.Field('REQUEST_HEADERS'), header_name)
    end
    local long_request = P.Or(
      long(header('Content-Type')),
      long(header('Host'))
    )
    = long_request() --> (or (lt '1000' (length (sub (field 'REQUEST_HEADERS') 'Content-Type'))) (lt '1000' (length (sub (field 'REQUEST_HEADERS') 'Host'))))

The result sexpr is significantly longer.

The front end provides a method for formatting sexprs for human consumption.

    print(P.pp(long_request()))

prints

    (or
      (lt
        '1000'
        (length
          (sub
            (field 'REQUEST_HEADERS')
            'Content-Type'
          )
        )
      )
      (lt
        '1000'
        (length
          (sub
            (field 'REQUEST_HEADERS')
            'Host'
          )
        )
      )
    )

You may have noticed a feature of Predicate in use above, the automatic conversion of Lua types.  Numbers and Lua strings were automatically converted to Predicate strings when used as parameters to functions.  This conversion includes proper escaping:

    = P.List("An 'important' example")() --> (list 'An \'important\' example')

Boolean types are also converted:

    = P.List(true, false)() --> (list (true) (false))

Boolean expressions can be written using operators:

    = (P.Field('A') + P.Field('B'))() --> (and (field 'A') (field 'B'))

Note that addition was transformed into `and`.

In the long function, we used `Lt` becomes the static value must be the first argument in Predicate.  However, the following would have been clearer:

    local function long(a, long_length)
      long_length = long_length or 1000
      return P.Gt(P.length(a), long_length)
    end

This version is actually legal because the front end understands `Gt` and automatically switches it to `Lt` with the static argument first:

    = long(P.Field('A'))() --> (lt '1000' (length (field 'A')))

Some calls such as `Length` make sense as methods.  The front end provides a number of these as alternatives:

    local function long(a, long_length)
      long_length = long_length or 1000
      return P.Gt(a:length(), long_length)
    end

    local function header(header_name)
      return P.Field('REQUEST_HEADERS'):sub(header_name)
    end

The front end will also evaluate some functions at configuration time, if possible:

    = P.Gt(100, 50)() --> (true)

Finally, the front end adds a number of calls that do not exist in the back end but can be implemented in terms of other calls:

    = P.Xor(P.Field('A'), P.Field('B'))() --> (or (and (field 'A') (not (field 'B'))) (and (field 'B') (not (field 'A'))))

Reference
---------

**Warning**: Both Predicate and the Front End are under active development.  Expect this section to grow or change frequently.

**Expression Objects**

Most `P.*` methods return an expression object.  All expression objects support conversion to an sexpr string by calling and the operators (see below).  Expression objects are further divided into String, Call, Null, and Raw.

- String expression objects support String methods (see below).
- Call expression objects support String method and Call methods (see below).
- Null and Raw expression objects do not have any additional methods.

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

**String Methods**

String expression objects support the following methods:

Form          | Meaning             | Example                         |
------------- | ------------------- | ------------------------------- |
`s:length()`  | Length(s) of value  | `P.S("foo"):length()() --> '3'` |

**Call Methods**

Call expression objects support the following methods:

Form              | Meaning                           | Equivalent           |
----------------- | --------------------------------- | -------------------- |
`c:length()`      | Length of value                   | `P.Length(c)`        |
`c:sub(name)`     | First subfield named `name`       | `P.Sub(name, c)`     |
`c:suball(name)`  | All subfields named `name`        | `P.Suball(name, c)`  |
`c:streq(other)`  | Value string equal to `other`     | `P.Streq(other, c)`  |
`c:istreq(other)` | Case insensitive of previous      | `P.Istreq(other, c)` |
`c:eq(other)`     | Value number equal to `other`     | `P.Eq(other, c)`     |
`c:ne(other)`     | Value number not equal to `other` | `P.Ne(other, c)`     |
`c:T()`           | Run transformation `T`            | `P.T(c)`             |

In the last row, replace `T` with any transformation described below.

**Introspection**

It is possible to look inside expression objects.

Which       | Member     | Meaning                                         |
----------- | ---------- | ----------------------------------------------- |
All         | `type`     | Type of object: `null`, `raw`, `call`, `string` |
String, Raw | `value`    | Value as Lua string                             |
Call        | `name`     | Name of Call node as Lua string                 |
Call        | `children` | Array of child objects (arguments)              |

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

**Methods**

Fundamentals:

Method              | Meaning                  | Sexpr        |
------------------- | ------------------------ | ------------ |
`P.Call(name, ...)` | Call expression object   | `(name ...)` |
`P.C(name, ...)`    | Synonym for `P.Call`     | `(name ...)` |
`P.String(value)`   | String expression object | `'value'`    |
`P.S(name, ...)`    | Synonym for `P.String`   | `'value'`    |
`P.Raw(value)`      | Raw expression object    | `value`      |
`P.R(value)`        | Synonym for `P.Raw`      | `value`      |

Boolean:

Method         | Meaning      | Notes |
-------------- | ------------ | ----- |
`P.And(...)`   | Boolean and  |       |
`P.Or(...)`    | Boolean or   |       |
`P.Xor(a, b)`  | Boolean xor  | 1     |
`P.Not(a)`     | Boolean not  |       |
`P.Nand(...)`  | Boolean nand | 1     |
`P.Nor(...)`   | Boolean nor  | 1     |
`P.Nxor(a, b)` | Boolean nxor | 1     |

Comparison:

Method           | Meaning                          | Notes   |
---------------- | -------------------------------- | ------- |
`P.Streq(a, b)`  | String equality                  | 2, 3, 4 |
`P.Istreq(a, b)` | Case insensitive string equality | 2, 3, 4 |
`P.Eq(a, b)`     | `a == b` as numbers              | 2, 3, 4 |
`P.Ne(a, b)`     | `a ~= b` as numbers              | 2, 3, 4 |
`P.Gt(a, b)`     | `a > b`                          | 2, 3, 4 |
`P.Lt(a, b)`     | `a < b`                          | 2, 3, 4 |
`P.Ge(a, b)`     | `a >= b`                         | 2, 3, 4 |
`P.Le(a, b)`     | `a <= b`                         | 2, 3, 4 |

Decision:

Method                          | Meaning                      | Notes |
------------------------------- | ---------------------------- | ----- |
`P.If(pred, if_true, if_false)` | Choose value based on `pred` | 3     |

Data:

Method                       | Meaning                               | Notes |
---------------------------- | ------------------------------------- | ----- |
`P.Length(collection)`       | Number of items                       |       |
`P.Sub(name, collection)`    | First subfield `name` in `collection` | 4     |
`P.Suball(name, collection)` | All subfields `name` in `collection`  | 4     |
`P.SetName(name, value)`     | Construct named value                 |       |
`P.List(...)`                | Construct list                        |       |
`P.Field(name)`              | Data field `name`                     |       |

IronBee Operators:

See IronBee manual for details.  All operators take a static argument passed to them at configuration time and a dynamic argument passed to them at evaluation time.  The static argument must be a string literal.

Method                              | Meaning             | Notes |
----------------------------------- | ------------------- | ----- |
`P.Operator(name, static, dynamic)` | Use operator `name` | 4     |
`P.Rx(pattern, data)`               | Regular expression  | 4     |

IronBee Transformations:

See IronBee manual for details.

Method                             | Meaning                     | Notes |
---------------------------------- | --------------------------- | ----- |
`P.Transformation(name,  dynamic)` | Use transformation `name`   | 4     |
`P.NormalizePathWin(path)`         | Normalize windows path      | 4     |
`P.NormalizePath(path)`            | Normalize path              | 4     |
`P.HtmlEntityDecode(s)`            | Decode `s` as HTML          | 4     |
`P.UrlDecode(url)`                 | Decode `url`                | 4     |
`P.Min(list)`                      | Smallest number in list     | 4     |
`P.Max(list)`                      | Largest number in list      | 4     |
`P.Count(list)`                    | Number of elements of list  | 4     |
`P.Length(list_or_s)`              | Length(s) of value          | 4     |
`P.CompressWhitespace(s)`          | Compress whitespace         | 4     |
`P.RemoveWhitespace(s)`            | Remove all whitespace       | 4     |
`P.Trim(s)`                        | Remove edge whitespace      | 4     |
`P.TrimRight(s)`                   | Remove whitespace suffix    | 4     |
`P.TrimLeft(s)`                    | Remove whitespace prefix    | 4     |
`P.Lowercase(s)`                   | Convert to lowercase        | 4     |
`P.Name(f)`                        | Name of `f`                 | 4     |
`P.Names(list)`                    | Name of items of `list      | 4     |
`P.floor(n)`                       | Round `n` down.             | 4     |
`P.ceil(n)`                        | Round `n` up.               | 4     |
`P.round(n)`                       | Round `n` to nearest.       | 4     |
`P.toString(n)`                    | Convert `n` to string.      | 4     |
`P.toInteger(s)`                   | Convert `s` to integer.     | 4     |
`P.toFloat(s)`                     | Convert `s` to float.       | 4     |

1. Implemented in terms of other calls.
2. Requires one static argument.  If second argument is the static argument, will swap, possibly changing operator.  E.g., `P.Gt(P.Field('a'), 5)` becomes `P.Lt(5, P.Field('a'))`.
3. Will evaluate at configuration time if possible.
4. Implemented via an IronBee operator or transformation (see `P.Operator`, `P.Transformation`, and IronBee manual).
