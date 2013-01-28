Title:  IronBee Fast Pattern Manual
Author: Christopher Alfeld <calfeld@qualys.com>

Fast Pattern User Manual
========================

Christopher Alfeld <calfeld@qualys.com><br>

**Warning:** This document describes a feature that is under ongoing development.  Only portions of what is described are currently available and all of it is subject to change.  This document is intended for IronBee developers, those wanting to play on the bleeding edge, and those interested in upcoming major features.

Introduction
------------

The IronBee Fast Pattern system as an upcoming system for improved rule performance.  It allows certain rules to be selected for evaluation in a manner that is significantly faster than the default process.  In particular, properly setup, fast pattern rules that are not evaluated impose near-zero time cost.  In contrast, in normal IronBee, every rule is evaluated and thus imposed some time cost on every transaction.  The system does use more space (RAM) than default and should be viewed as a space/time trade-off.

The system works by attaching one or more fast patterns to a rule.  The rule will only be evaluated if the fast pattern appears in the input.  It is important to note that a rule may still evaluate to false.  Typically, a fast pattern represents a string (or set of strings) that must be present in the input.  For example, a rule for request headers that depends on the regular expression `^Foo:` could have a fast pattern of `Foo:`, in which case it would only be evaluated if 'Foo:' was present somewhere in the header data.  If that occurrence was `Content-Type: Foo:`, then the rule would evaluate to false as the regexp would not match.

An important constraint on fast pattern rules is that the order they execute in is not guaranteed.  Thus, any rule that depends on another rule in the same phase or that is depended on by another rule in the same phase should not use fast patterns.  The final constraint is that fast patterns do not work well with transformations.

Internally, all fast patterns for a phase are compiled into an IronAutomata automata.  At each phase, the automata is executed and searches for the patterns as substrings in the input.  For any patterns found, the associated rules are then evaluated.

Fast Pattern Syntax
-------------------

The fast pattern syntax is that of the IronAutomata Aho-Corasick patterns.  The syntax, unlike regular expressions, only allows fixed width expressions.  It provides operators for escaping, e.g., `\e` for escape, and for character sets, e.g., `\l` for any lower case character.  For the latest syntax, run `ac_generator --help` from IronAutomata.  The result as of this writing is:

	Patterns provide a variety of fixed width operators that are shortcuts for
	a byte or span of bytes.  E.g., "foo\dbar" is a pattern for "foo0bar",
	"foo1bar", ..., "foo9bar".

	Single Shortcuts:
	- \\ -- Backslash.
	- \t -- Horizontal tab.
	- \v -- Vertical tab.
	- \n -- New line
	- \r -- Carriage return.
	- \f -- Form feed.
	- \0 -- Null.
	- \e -- Escape.

	Parameterized Single Shortcuts:
	- \^X -- Control character, where X is A-Z, [, \, ], ^, _, or ?.
	- \xXX -- ASCII character XX in hex.
	- \iX -- Match lower case of X and upper case of X where X is A-Za-z.

	Multiple Shortcuts:
	- \d -- Digit -- 0-9
	- \D -- Non-Digit -- all but 0-9
	- \h -- Hexadecimal digit -- A-Fa-f0-9
	- \w -- Word Character -- A-Za-z0-9
	- \W -- Non-Word Character -- All but A-Za-z0-9
	- \a -- Alphabetic character -- A-Za-z
	- \l -- Lowercase letters -- a-z
	- \u -- Uppercase letters -- A-Z
	- \s -- White space -- space, \t\r\n\v\f
	- \S -- Non-white space -- All but space, \t\r\n\v\f
	- \$ -- End of line -- \r\f
	- \p -- Printable character, ASCII hex 20 through 7E.
	- \. -- Any character.

Using Fast Patterns
-------------------

**Step 1**: Add `fast:` modifiers to your rules.

Look for rules that require a certain substring in order to be meaningful.  Add `fast:substring` to those rules.  For more advanced use, specify AC patterns (see previous section).  For example, to require `foo` in a case insensitive manner, use `fast:\if\io\io`.

If there is no single required substring but instead a small number of alternatives, you can use multiple fast modifiers.  E.g., for a regular expression `foo|bar`, consider `fast:foo fast:bar`.

There is a script, `fast/suggest.rb` which takes rules on standard in and outputs the rules to standout with additional comments suggesting fast patterns based on regular expressions in the rule.  This script is currently very limited, but should eventually cover a wide range of regular expression based rules.  It requires the `regexp_parser` gem which can be installed via `gem install regexp_parser`.

**Step 2**: Build the automata.

In order for IronBee to take advantage of fast modifiers, it needs the corresponding automata.  This automata is an IronAutomata Eudoxus file with specific metadata.  The easiest way to build it is to run `fast/build.rb` (currently this must be run in the *object tree* `fast` directory) with a single argument specifying the rules file.  It will generate a bunch of build artifacts, including a `.e` file suitable for loading into IronBee.

Note that you must be run `build.rb` on a platform of the same endianness as where you intend to run IronBee.

Here is an example run:

	obj/fast> ../../ironbee/fast/build.rb test.txt
	Extracting rules from test.txt to test.txt.manifest
	  .../fast/extract.rb
	Generating AC automata from test.txt.manifest to test.txt.automata
	  ./generate
	Optimizing automata from test.txt.automata to test.txt.optimized
	  ../automata/bin/optimize --translate-nonadvancing-structural
	Translate Nonadvancing [structural]: 6
	Compiling optimized from test.txt.optimized to test.txt.e
	  ../automata/bin/ec -i test.txt.automata -o test.txt.e -h 0.5
	bytes            = 1993
	id_width         = 2
	align_to         = 1
	high_node_weight = 0.5
	ids_used         = 489
	padding          = 0
	low_nodes        = 177
	low_nodes_bytes  = 1361
	high_nodes       = 1
	high_nodes_bytes = 77
	pc_nodes         = 2
	pc_nodes_bytes   = 16
	bytes @ 1        = 1504
	bytes @ 2        = 1993
	bytes @ 4        = 2971
	bytes @ 8        = 4927
	
During this run the following files were created:

- `test.txt.manifest`: The patterns and rule ids.  Human readable.
- `test.txt.automata`: The initial automata.  This automata can be viewed as a GraphViz dot file via `automata/bin/to_dot`.
- `test.txt.optimized`: The automata after some optimizations.  This automata can also be viewed via `to_dot` but may be more confusing.
- `test.txt.e`: The result of compiling `test.txt.optimized` via the Eudoxus Compiler (`ec`).  This file is what you will load into IronBee.

Note that `bytes = 1993` line.  This line shows the space (RAM) cost of using fast patterns over normal IronBee.

**Step 3**: Tell IronBee about the automata.

**Note: This step is not yet supported in IronBee.**

IronBee must be told to use the fast pattern system and about the automata you built in step 2.  Make sure you load the `fast` module.  Then use the `FastAutomata` directive to provide the path to the `.e` file you built in step 2.  

At present, you should use a single automata built from every fast pattern rule, regardless of phase or context.  The fast pattern system will filter the results of the automata execution to only evaluate rules appropriate to the current context and phase.  The current assumption is that a single automata plus filtering is better choice in terms of space and time than per-context/phase automata.  This assumption may be incorrect or such usage may be too onerous to users.  As such, this behavior may change in the future.

Advanced Usage
--------------

Advanced users may want to tune their automata further.  Additional optimizations can be attempted and different space/time tradeoffs taken.  Users should be familiar with IronAutomata and the options available, especially high node weight in `ec`.  The initial automata can be generated from the manifest via the `fast/generate`.  That automata can then be optimized and compiled in whatever manner desired so long as an equivalent Eudoxus automata is the end result.

Performance Notes
-----------------

The underlying automata should execute in `O(n)` time where `n` is the size of the transaction data.  Given an automata execution that results in `k` rules, an additional `O(k)` time is needed to filter the rules down to the `k' <= k` rules appropriate to the phase and context.  Finally, `O(k')` time is needed to evaluate and potentially execute the rules.  In contrast, default IronBee uses `O(m)` time (where `m` is the number of rules in the current phase and context) to select, evaluate, and execute rules.  Thus fast pattern rules provide an advantage where `m` is large and `k` is small.  Such a situation occurs when there are many specific rules.  If you have a small rule set, or most of your rules are very general, default IronBee is likely the better choice.
