Title:  IronAutomata Internals
Author: Christopher Alfeld <calfeld@qualys.com>
xhtml header: <script type="text/javascript" src="http://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS-MML_HTMLorMML"></script>

This document can be rendered to HTML (or many other formats) with [MultiMarkdown][].

[MultiMarkdown]: http://fletcherpenney.net/multimarkdown/

**IronAutomata Internals**

Christopher Alfeld<br>
calfeld@qualys.com<br>
12/2012

This document provides notes on the internal workings of IronAutomata.  It is written for developers intending to develop for IronAutomata.  Developers interesting in using IronAutomata in their own code should see the doxygen documentation.  Also see `example.md`.

It is assumed that the reader is familiar with using IronAutomata (see `example.md`) and the public API (see Doxygen).

Automata Capabilities
=====================

The automata that IronAutomata generators, manipulates, and executes are essentially deterministic or non-deterministic finite automata with an output function that maps nodes to sets of outputs.  There are a few details, however:

Default Targets
---------------

Nodes may specify a default target which is a node to use as the next node if no matching edge is found.  This capability is sometimes referred to as a default edge, being viewed as an edge that is followed if no other edge matches the current input.

Epsilon Edge
------------

An epsilon edge is an edge that is followed on any input.  Unlike the default edge, it is followed even if other edges also match, in which case, the automata is non-deterministic.

Non-Advancing
-------------

Edges, including the default edge, can choose whether they advance the input or not.  A non-advancing edge will change the current node but not advance the input location.

No-Advance-No-Output
--------------------

An automata as a whole can declare No-Advance-No-Output which means that output should never be emitted unless the input was advanced on the last transition.

Metadata
--------

Automata can have arbitrary key-value pairs associated with them.

Varying Requirements
--------------------

Different parts of IronAutomata have different requirements on the automata they use.  Most of the optimizations are written to function on any automata.  The Eudoxus execution engine requires deterministic automata and does not support epsilon edges.

Default edges, epsilon edges, and non-advancing are all representational details.  Any automata with such features is semantically equivalent to an automata with none of them.  The Translate Non-Advancing optimization can completely eliminate non-advancing edges in an automata.  In the future, converters to eliminate epsilon or default edges may be written, or a non-deterministic-to-deterministic converter.  In practical terms, however, representation matters and an automata that is small and elegant when non-deterministic can explode exponentially when made deterministic (see Dragon book).

Intermediate Format
===================

The Intermediate Format (IF) is the interface between generation, optimization, and compilation.  It encompasses a variety of compromises to serve each domain and, as a result, is often adequate and rarely optimal.

It is expected that generation and optimization is allowed to use significant more resources than execution.  As such, the IF does not need to be as compact as, e.g., the Eudoxus format.  However, it must have some awareness of space constraints as IronAutomata is expected to be used with very large automata.  As a result, the IF supports features such as multiedges and value bitmaps.

Another influence on the IF is the desire to do certain general optimizations within the IF rather than within each compiler.  E.g., the IF directly represents the singly linked list structure for outputs.

Protobuf vs Intermediate::
--------------------------

On disk, there is a single IF format: chunked and gzipped protobuf.  In memory their are two formats, the protobuf format and the Intermediate:: classes.  The latter adds a variety of useful methods for manipulation and translates IDs into pointers, but is otherwise a direct translation of the protobuf.  The disadvantage of the Intermediate:: classes is that they require loading the entire automata into memory, i.e., there is no streaming interface.  Generally, in C++, it is easier to use the Intermediate:: classes and not worry about the underlying protobuf.

IDs
---

The protobuf uses unsigned 64 bit identifiers to refer to objects.  These identifiers are translated to and from pointers by the Intermediate:: code.  

There are two types of identifiers: node and output.  These occupy separate spaces, so a node identifier can be the same number as an output identifier.  There is no requirement that the identifiers be contiguous.  However, 0 is reserved as no-referent identifier.

The IF identifiers should not be confused with the identifiers used by Eudoxus.

Chunking
--------

The protobuf format is written to disk in one or more chunks.  Each chunk is a unsigned 32 bit integer in network byte order indicating how many bytes remain in the chunk, followed by gzipped protobuf containing one or more Node, Output, or Graph messages.

It is possible to store an entire automata in a single chunk.  There are two advantages to using multiple chunks:

1. Certain automata operations can operate in a streaming manner, reading a single chunk into memory at a time.  For example, the `to_dot` utility, loads a single chunk at a time.
2. Even when the entire automata is loaded into memory, the Intermediate:: reader and writer operate one chunk at a time.  Thus, smaller chunks means smaller working space.  For very large automata, this difference can be significant.

Multi-edges
-----------

The IF supports representing multiple edges in the automata theory sense in a single Edge message.  I.e., edges can have multiple values and are followed on inputs that match any of those values.

Values of edges can be represented in one of two different ways:

1. As a vector of values.
2. As a 256 bit bitmap with 1s for each value.

The Intermediate::Edge code contains methods for reading and writing values independently of the format as well as format specific methods and format conversion methods.  Generally, edges with few values use vectors and edges with many values use bitmaps.

Note that some algorithms such as the Aho-Corasick pattern generator use sorted vectors of values.

Outputs
-------

Outputs are byte sequences associated with nodes.  A node may have more than one output and an output be emitted by more than one node.  

The outputs are, at first glance, represented as singly linked lists.  However, there is no requirements that each output object is referred to by only one other object.  Thus outputs are actually represented as a tree, but a tree with only parent pointers; no child pointers. 

This reverse-tree representation is by design.  In particular, it works well with generators such as Aho-Corasick that take the outputs of one node and append them to the outputs of another.

There are a few "standard" output types.  These are only conventions supported by the existing command line programs.  Users of the API are free to use whatever outputs they wish.  The standard outputs are:

- Length: An unsigned 32 bit integer stored in host byte order.  This output is useful for substring searches where the desired knowledge is what portion of the output is the substring.  Since outputs are generated along with the current input location (which, for Aho-Corasick is the byte after the last byte of the substring), the location can be combined with the length to calculate the substring.  `ee -t length` will do exactly this and output the substring.
- Integer: An unsigned 32 bit integer stored in host byte order.  This output is similar to the previous, but `ee -t integer` will output the integer with no interpretation.
- String: A sequence of bytes.  `ee -t string` will directly output the string.
- Accept: For automata that only need to distinguish between accept and reject nodes, a 1 as a 32 byte unsigned integer in host byte order is stored as the output of accept nodes.

Generator: Trie
===============

The Trie generator is the simplest generator.  It is intended as a Hello World style example for generators.

The generator algorithm is essentially the add word algorithm from the Aho-Corasick generator.  For a word to add, it follows existing edges as far as they go and then adds the missing edges.

There is no attempt (or capability in the IF) for path compression.  As such, the result is a Trie rather than a Patricia trie.  However, Eudoxus does support path compression and will automatically do so as part of compilation.  Thus, while the IF is a Trie, the compiled Eudoxus automata is closer to a Patricia trie.

At present, the Trie generator exists only as a command line utility.  At some point, an API version will likely be written.  The command line utility uses the "Accept" output type.

Generator: Aho-Corasick
=======================

The Aho-Corasick (AC) generator builds automata that implement the Aho-Corasick algorithm.  It comes in two versions, normal and pattern.  Normal Aho-Corasick is a fairly direct implementation of the original AC paper and the IF has the capabilities (e.g., defaults, non-advancing edges) to directly represent such automata.  Just replace "fail" with "default".

Pattern AC is much more complicated algorithm.  In principle, an AC Pattern is equivalent to a set of normal words, however, a pattern such as `\d\d\d\d` is equivalent to \\( 10^4 = 10000 \\) words: `0000` through `9999`.  Adding all possible words would be infeasible both in terms of generator execution time and automata space.  The generator takes advantage that all such words have the same outputs and makes aggressive use of multiedges to represent the patterns compactly.

Both normal and pattern act in three stages, a shared initialization stage, separate repeated add stages, and a shared final stage.  The initialization stage simply creates a start node.  The normal add stage is essentially identical to the Trie generator.  The pattern add stage is complex and described in detail below.  The shared final stage adds in the default ("failure") edges and merges output sets.  It is written to work with multi-edges and supports the single-valued edges the normal add stage creates as a specific case of multi-valued edges.

At present, officially, mixing normal and pattern adds in the same automata will result in undefined behavior.  In practice, although untested, problems should only occur if a normal add takes place after a pattern add.  I.e., if all normal adds come first, it should work.  The normal add assumes all edges are single valued.

The API allows arbitrary outputs to be associated with each dictionary entry.  The command line utility uses the "Length" output type for normal AC and the 
"String" output type for pattern AC.  Note that for normal AC, the substring is the same as the matching dictionary entry.

Split Edge
----------

The Pattern AC algorithms depend on a sub-algorithm, split edge.  The split edge algorithm takes a multi-valued edge and a subset of its values and splits the edge into two edges, one of which contains the specified subset, and one of which contains the remainder.  Both edges have the same source node.  A new target node is created for the new edge.  The original target node is copied over to the new target node.  This copy is deep, i.e., it copies the entire subtree.

    SplitEdge(edge, tokens):
        newEdge <- AddEdge(source(edge), values(edge) - tokens)
        values(newEdge) <- values(edge) & tokens
        DeepCopy(target(newEdge), target(edge))
        return newEdge
   
        
The follow diagrams show splitting an edge with values \\( \{a, b, c \} \\) on \\( \{ b, c \} \\).

![Deep Split Edge][split_edge]

[split_edge]: internal_split_edge.png

Add Pattern
-----------

Note that an AC Pattern can be viewed as a sequence of sets of inputs.  In this view, a normal AC word is a sequence of sets of size 1.  E.g., the pattern `\d\d\d\d` becomes \\( \{0 \ldots 9\}\{0 \ldots 9\}\{0 \ldots 9\}\{0 \ldots 9\} \\) where \\( \{0 \ldots 9\} \\) is shorthand for \\( \{0, 1, 2, 3, 4, 5, 6, 7, 8, 9\} \\).

As with the normal add algorithm, the add pattern algorithm will consider each element of the sequence in turn.  In the normal case, at each element there is a single corresponding node in the AC Tree.  In the pattern case, there may be multiple nodes.  E.g., in

![Multiple nodes for a pattern.][add_pattern1]

[add_pattern1]: internal_add_pattern1.png

The pattern \\( \{ 1, 2, 3, 4, 5, 6 \} \\) would have two nodes, 2 and 3, at the second level of three.

The basic add algorithm, then looks like:

    AddPattern(startNode, pattern, output):
        lastNodes <- {}
        currentNodes <- {startNode}
        for each set inputs in pattern
            nextCurrentNodes <- {}
            for each node node in currentNodes
                nextCurrentNodes <- nextCurrentNodes & AddPatternInner(inputs, node)
            lastNodes <- currentNodes
            currentNodes <- nextCurrentNodes
        for each node in lastNodes
            outputs(node) <- {output}

Thus, the inner operation of add is considering a set of inputs for a node in the tree.  It looks for edges with values in its input set.  For edges whose values are subsets, it adds the target of the edge to the nodes to consider at the next stage and removes those values from its input set.  For edges whose values intersect but are not subsets, it splits using Split Edge, yielding an edge that is the subset case and an edge with no shared values.  After considering all edges, if any values remain in the inputs, it adds a new edge for those values.

    AddPatternInner(inputs, node):
        nextNodes <- {}
        for each edge of node with values(edge) & inputs != {}
            if values(edge) - inputs != {} # Not subset
                edge <- SplitEdge(edge, values(edge) & inputs)
            nextNodes <- nextNode & {target(edge)}
            inputs <- inputs - values(edge)
        if inputs != {}
            newEdge <- AddEdge(node, inputs)
            nextNodes <- nextNode & target(newEdge)
        return nextNodes

In the follow diagram, we have a tree for the pattern \\( \backslash l \backslash w = \{a\ldots z\}\{a\ldots zA\ldots Z0 \ldots 9\} \\) and are adding the pattern \\( \backslash w \backslash w = \{a\ldots zA\ldots Z0 \ldots 9\}\{a\ldots zA\ldots Z0 \ldots 9\} \\).

![Adding \w\w to \l\w][add_pattern2]

[add_pattern2]: internal_add_pattern2.png

The add algorithm stores all edge values as *sorted* vectors.  Doing so allows easy set operations, e.g., `std::set_difference`.

Process Failures
----------------

The process failures algorithm at heart is as described in the original AC paper:

    ProcessFailures(startNode):
        todo <- []
        for each edge of startNode:
            fail(target(edge)) <- startNode
            todo.push(target(edge))
        while todo != {}
            node <- todo.pop()
            for each edge in node
                todo <- todo + ProcessFailuresInner(startNode, node, edge)
                
The inner operation is responsible for calculating the failure for `target(edge)`.  For normal AC, this would look like:

    ProcessFailuresInnerNormal(startNode, node, edge): # Example; not actually used
        current <- fail(node)
        v <- value(edge)
        s <- target(edge)
        while true
            e <- edge of current such that value(e) = v
            if e exists
                fail(s) <- target(e)
                return s
            else
                current <- fail(current)
                if current = startNode
                    fail(s) <- startNode
                    return s

The issue with patterns is that `value(edge)` becomes `values(edge)`, all of which must be handled.  Different values may have different failures.  These pose a problem as, at the moment, there is only a single `target(edge)` and thus a single failure behavior.  To accommodate this, when different failures are needed, `edge` will be split, duplicating `target(edge)`.

    ProcessFailuresInner(startNode, node, edge):
        inputs <- values(edge)
        s <- target(edge)
        
        if fail(s) already defined # Can happen due to shallow splits.
            return
            
        newTodo <- [s]
        
        current <- fail(node)
        while cs != {}
            for each edge e of current with values(e) & inputs != {}
                if values(e) = inputs
                    # Easy case
                    fail(s) <- target(e)
                    inputs <- {}
                else
                    # Need to split edge
                    newEdge <- SplitEdge(edge, values(e) & inputs)
                    newTodo <- newTodo + [target(newEdge)]
                    fail(target(newEdge)) <- target(e)
                    inputs <- inputs - values(e)
                if inputs = {}
                    # Early break
                    break
            if inputs != {}
                if current = startNode
                    fail(s) <- startNode
                    inputs <- {}
                else
                    current <- fail(current)

In the following diagram, `\l\w\d` has been added to the automata and then process failures called.  The diagram illustrates `ProcessFailuresInner` for node 2 and the edge from 2 to 3.

![Processing Failures][add_pattern3]

[add_pattern3]: internal_add_pattern3.png

Below is the completed automata.  Note the added outputs.

![Final Automata][add_pattern4]

[add_pattern4]: internal_add_pattern4.png


Further Improvements
--------------------

Further improvements over the current algorithms are possible, both for normal and pattern.  The insight from the pattern algorithm is that the automata only needs to distinguish inputs in so far as it matters for determining distinct outputs.  That is, a node in the AC tree needs to represent potential outputs and future failures; it is fine if it represents multiple inputs so long as those inputs are, in some sense, equivalent.  In the following tree, node 2 represents 10 different inputs, but this is fine as all 10 inputs have the same output and failure behavior:

![One node, many inputs][ac_improve1]

[ac_improve1]: internal_ac_improve1.png

This role of nodes suggests that nodes that are equivalent in terms of future outputs and failure behavior could be merged.  As a concrete example, consider, the following modification of the final automata from the previous section:

![Final Automata, Improved][ac_improve2]

[ac_improve2]: internal_ac_improve2.png

Nodes 5 and 5' were identical to nodes 4 and 4' and merged together.

Such merging of identical nodes is not specific to Pattern AC and could be done as a generic optimization.

There may also be potential for AC specific optimization, taking advantage of the relation of outputs via the prefix ordering and the tree structure of the automata.  Developing such optimization is future work.

Optimization: Optimize Edges
============================

The intermediate format can represent a node and its edges in a variety of ways, some of which are more compact than others.  For example, a node that advanced to node 1 on inputs 0-200 and node 2 on inputs 201-255 could use any of the following:

- An edge with 201 values and an edge with 55 values.
- 255 edges with one value.
- An edge with 201 values and a default edge.
- An edge with 55 values and a default edge.

And many more possibilities besides.  In addition, for any of these possibilities, each edge could represent its values with a vector or a bitmap.

The optimize edges optimization chooses a "best" representation for a given edge set.  It operates on a node without considering the rest of the automata.  It represents edges to minimize space based on the following assumptions:

- A vector based edge uses one byte per value.
- A bitmap based edge uses 32 bytes regardless of the number of values.
- A default edge uses less space than any valued edge.
- An epsilon edges uses less space than any valued edge.

The optimization will lose multiplicity information.  That is, if there are multiple edges to node X on input Y, only one will be present after optimize edges is finished.

The current implementation is brute force.  It calculates a map of target node to which inputs lead to it.  If all 256 values lead to a target, then it uses the target with the most values as the default.  It then builds edges for each target using a bitmap when the number of values exceeds 32.  As a special case, if a target has all values, an epsilon edge is used.

Optimization: Deduplicate Outputs
=================================

The deduplicate outputs optimization looks for output objects with the same content and same next output (with no next output as a possible value) and merges them into the same object.  It repeats until no changes are made.  As an optimization, instead of looking at every output on each pass, on passes after the first, it only considers outputs whose next output was merged on the previous pass.  I.e., outputs that were not possible to merge in the previous pass but may be possible now.

In the IF, outputs are stored as *intrinsic* singly linked lists.  This can be problematic as there could be two outputs with large identical contents but different next outputs.  In the IF, there is no way to share space for the identical contents.  This issue is ameliorated in Eudoxus, which uses an output content table independent of the link structure.  Thus, for Eudoxus, deduplicate outputs only saves space in terms of link structure cost (a few bytes per output) rather than content cost.

Note that deduplicate outputs is completely transparent from an execution standpoint.  In particular, the order and multiplicity of outputs of a node is unchanged.

Improve Deduplicate Outputs
---------------------------

There is significant room for improvement if the order of outputs can be modified.  Consider the following output structure:

![Output Deduplication, Initial][dedup1]

[dedup1]: internal_dedup1.png

The existing deduplicate outputs algorithm can reduce this to:

![Output Deduplication, Current][dedup2]

[dedup2]: internal_dedup2.png

However, if the outputs are reordered, it could be reduced further to:

![Output Deduplication, Improved][dedup3]

[dedup3]: internal_dedup3.png

Implementing such an algorithm is future work.

Optimization: Translate Non-Advancing
=====================================

Translate Non-Advancing is a family of three related optimization algorithms.  All three eliminate non-advancing edges from the automata.  

The primary motivation for eliminating non-advancing edges is that it decreases the number of edges traversed per byte of input during execution.  In a deterministic automata, if there are no non-advancing edges, then each byte of input traverses at most one edge.  However, for two of the three algorithms, the resulting automata is usually larger and gains in speed from fewer transitions can be offset by cache performance.  As always in such cases, good benchmarks are important.

All three algorithms have far greater effect if non-advancing edges do not result in output, either because the target has no output or because no-advance-no-output is set for the automata.

The basic idea is to search for a non-advancing edge (in the single value, automata theory sense) from `A` to `B` on input `c` such that no output is generated by `B`.  In such cases, we "know" the next input, namely `c`, and so can determine the next edge of `B`.  There are three cases:

1. If there is no edge from `B` for `c`, then the execution will end.  The original non-advancing edge can safely be removed.
2. If there is a unique edge from `B` on `c`, to `D`, then the non-advancing edge can be retargeted to `D` (whether it advances or not is changed to match what the `B` to `D` edge does).
3. If there are multiple edges from `B` on `c`, then the original non-advancing edge can be replaced with multiple edges retargeted as in the previous case.

This search and replace operation is repeated until the automata stabilizes.

Note, again, that "edge" in the above is in the single-value automata theory sense, not in the multi-edge representation sense.  In practice, even when case 2 applies, a single multi-edge may be replaced with multiple multi-edges, resulting in a larger automata, in terms of bytes.

The three variants are:

- Aggressive: As described above.  Note that this will eventually eliminate every advancing edge in the automata.
- Conservative: In case 3, does nothing.  I.e., will not replace an edge with multiple edges.  Note that in the deterministic case, case 3 never occurs and aggressive and conservative are equivalent.
- Structural: This is a limited version of conservative that is aware of the existing multi-edge structure of the automata.  It will do a similar search-and-replace operation, but will only replace if can do so within the existing structure, i.e., by retargeting a multi-edge.

Note that the structural version never creates objects or changes values and thus is guaranteed to not grow the automata.  As such, it is generally suggested that the structural version be run on automata that it may benefit, such as Aho-Corasick.

The aggressive and conservative variants are currently implemented in a brute force fashion.  In particular, for each node, the algorithm looks at each of the 256 possible inputs in turn.  For nodes where few values have the same target, this is not too bad, although for nodes with few edges it can result in many iterations that do nothing.  For nodes where many values have the same target, this is grossly inefficient.  The space usage is ameliorated by optimizing the edges of each node before proceeding to the next, but the algorithm takes far more time than it could.  An improved version would operate on a per-target or per-multi-edge basis, rather than a per input value basis.  The algorithms could be further improved by intelligently limiting what edges are considered based on what edges changed in the previous pass.  Such enhancements are left for future work.

In contrast, the structural version operates on a per-multi-edge basis and, as such, is far faster.  However, it is both highly limited in what it can do and its behavior depends on how the edges of a node are mapped onto multi-edges.  E.g., if every multi-edge has a single value, it will behave equivalently (and faster than) the conservative variant, but edges with multiple values may prevent operations that the conservative variant could perform.

Examples of each variant are in `example.md`.

Utility: Variable Length Structures
===================================

Eudoxus makes extensive use of variable length structures (VLS).  Variable length structures are similar to normal structures except that their data members may be optional or have variable length.  The VLS code is generic and could be used in other application where highly compact data structures are needed.

The use of variable length structures is well documented in `vls.h`.  Internally, a state variable holds a pointer to the current location in the structure and macros both conditionally extract and advance the pointer.

Utility: Buffer Assembly
========================

The buffer assembly code provides mechanisms for building up regions of memory.  For example, it includes a templated function for appending a structure to the end of the buffer.  The code is described in `buffer.hpp`.

An important aspect of using the buffer assembly code is the difference between pointers and indices.  Pointers, e.g,. to a just appended structure, allow direct access may not be preserved in the future as the assembler may move the data into larger allocations.  Indices are counted from the beginning of the buffer and are preserved.  Methods exist for converting between indices and pointers.

Eudoxus
=======

Eudoxus is currently the only execution engine in IronAutomata.  Some important aspects:

- Oriented at compact representation of automata.  
- Supports only deterministic automata and does not support epsilon edges.
- Execution engine is written in C.
- Handles outputs via callbacks.  Callbacks may terminate automata execution.
- Also supports querying of current or final nodes outputs.
- Supports streaming: executing on data in a series of calls.
- Endian dependent: automata must be compiled for the same endianness that they are executed in.

The API of Eudoxus execution is in `eudoxus.h`.  The API of the compiler is in `eudoxus_compiler.hpp`.  The command line compiler is `ec.hpp` and the command line executor is `ee.hpp`.

Automata Format
---------------

Automata are represented in a custom format.  The on disk and in memory representations are identical.  Thus, loading an automata involves only loading the data into memory, some basic validation of version and endianness, and setting up a small, fixed amount of state, including a pointer to the data.

Automata objects, including nodes and outputs, are referred to with IDs which are the index of the first byte of the object in the automata data.  IDs are unsigned integers.  The width of an ID can be varied.  E.g., an automata that fits in less than 256 bytes will use an ID width of 1 byte, whereas an automata larger than 4 gigabytes will require an ID width of 8 bytes.

The automata begins with a header containing:

- Version.  This is checked for compatibility at automata load.
- ID width.
- Endianness.  Also checked for compatibility at automata load.
- No-Advance-No-Output.
- Number of nodes, outputs, and output lists.  These are not actually used by the execution engine but are provided to aid other tools involve Eudoxus automata.
- Length of the automata data, including the header.
- Index of the start node.  The start node is the first object after the header, but there may be some amount of padding between the header and the start node.
- Index of the first output list.  This index is used to determine whether an ID refers to an output or an output list, allowing the elimination of an output list object for an output with no next output.

Following the header are sections for node, output, and output list objects, in that order.  It is not required that objects be perfectly packed.  Padding may be inserted between objects, e.g., to align objects to certain byte boundaries.

Finally, any automata metadata as appended as a sequence of pairs of outputs representing keys and values, respectively.  No ID ever refers to these outputs, so the space they consume is not considered for determining id width.

An output object is length and content.  An output list object is the ID of an output object and the ID of the next object.  The next object may either be an output object or an output list object.  Which is determined by comparing it to the index of the first output list stored in the header.

There are three types of node objects:

- Low Degree Nodes represent edges as a vectors of value, target pairs.
- High Degree Nodes represent edges as bitmaps and vectors of targets.
- Path Compression Nodes represent linear sequences of automata nodes.

Every node has a flag indicating whether it has any outputs and, if set, the ID of the first output.

Low degree nodes add the following flags:

- `has_nonadvancing`: Do any edges not advance.
- `has_default`: Is there a default edge.
- `advance_on_default`: Does the default edge advance.
- `has_edges`: Are there any non-default edges.

In addition the object contains the degree (if there are edges), the default target (if there is a default edge), and a bitmap indexed by edge of index of whether an edge advances (if there are edges and some of them do not advance).  Finally, if there are edges, they are appended as a vector of value, target pairs.  At present, no requirements are placed on the order of edges but future versions may require a specific order.

High degree nodes add the `has_nonadvancing`, `has_default`, and `advance_on_default` flags that low degree nodes do.  In addition, they add two more flags:

- `has_target_bm`: Is there a bitmap indicating which values have targets.
- `has_ali_bm`: Is there a bitmap indicating when to advance the lookup index.

In addition the objects contains the default target (if there is a default edge), a bitmap of whether a value advances (if there are non-advancing edges), a bitmap of whether a value has a target (if `has_target_bm` is set), and an Advance-Lookup-Index (ALI) bitmap, described below.  Finally, the targets are appended as a vector of IDs.

The `target` and `ali` bitmaps interact to determine the index in the targets vector for a given value.  In the basic case, there is no `ali` bitmap.  To determine the index of a value, the number of 1s before that value are counted in the `target` bitmap.  This case requires that every targeted value has an entry in the targets vector.  For nodes with edges representing ranges of values, a more compact representation is available where a single entry in the targets vector represents a consecutive sequence of values.  This behavior is supported with the `ali` bitmap.  When the `ali` bitmap is present, the `target` bitmap determines whether a value has a target or not, but the (potentially different) `ali` bitmap is used to determine the index, again by counting the number of 1s before the value.  The `ali` bitmap thus indicates at which values the index into the targets vector should be advanced, hence Advance-Lookup-Index.

There are special cases.  If every value has a target, the `target` bitmap would be all 1s and can be omitted.  In this case, the `ali` bitmap is still optional.  If both bitmaps are omitted, then the index for a value is the value itself, i.e., there the targets vector will have 256 entries.

Path compression nodes represent a sequence of automata nodes.  Each node in the sequence must have the default behavior (target and advance).  Only the initial node in the sequence is allowed to have outputs.  And each node in the sequence has exactly one non-default edge and that edge must advance, i.e., a single value.  Path compression nodes are common, for example, in Tries.

Path compression nodes add the `has_default` and `advance_on_default` flags of the low node.  In addition, an `advance_on_final` flag is added indicating whether to advance if the full path is completed.  The remaining three flags encode the length of the path as 2, 3, 4, or more than 4.  Paths of length 1 are not supported: such single-edge nodes can be represented as equally compactly as low nodes.  Paths of length greater than 4 will have there length stored later in the node.

A final target ID is always present.  There is also a default ID if `has_default` is set, the length if needed, and then two or more values describing the path.

The semantic behavior of a path compression node is to emit outputs on entrance, consumes inputs as long as they match the path, and then goes to the final target or default target as appropriate.

Execution
---------

The execution engine interprets the automata as required by the format (see above).

One aspect of execution is the use of subengines.  There are four subengines, one for each of the ID widths: 1, 2, 4, and 8.  The subengines have otherwise identical code and make use of macro metaprogramming to share source code.  It is dubious whether this provides meaningful performance gain, however, it does allow the code to treat identifiers naturally and for the compiler to do type checking.

The subengine node data is defined in `eudoxus_subautomata.h` and the code in `eudoxus_subengine.c`.  These files are included, four times, in `eudoxus_automata.h` and `eudoxus_engine.c`, respectively.  The `eudoxus_subautomata.h` files also defines a traits template for C++ usage, e.g., the compiler.

Output for all nodes types is handled in a single routine.  Each node type defines its own next function to determine the next node.  The overall next function simply demuxes by node type.  The execute function loops so long as their is input and a next node, running the output function for the current node and then the next function.  The top most, public API, functions demux to specific subengine based on id width.

The `target` and ALI `bitmaps` are used via a population count function.  This function in turn calls the built in gcc popcount function.  There are known techniques for improving naive popcount performance by storing additional summary data about the bitmap.  However, many modern CPUs include hardware popcount support.  Eudoxus does not store additional summary information and hopes for hardware support.

Compiler
--------

The compiler is written in C++ and, as with the execution engine, is made up of four subcompilers, one for each ID width.  As this is C++, this is done via templates rather than macros.

Determining ID width is done simply.  The compiler tries ID width 1 and, if it grows beyond 256 bytes, restarts with ID width 2, and so on.  This could be improved for large automata by completing a compilation at a smaller ID width (IDs would be bogus) to gain a count of the number of IDs and then calculating the optimal size.  For small to moderate automata, this would be slower than the early abort and increment approach.  E.g., for automata that fit in an ID width of 2 (65K) but are significantly larger than 256 bytes, the current approach takes slightly over one compilation, whereas the other approach would take two: the first to count IDs and the second to compile with the proper ID width.

Compilation is done in two stages.  In the first stage, all objects are laid out but any IDs are left unset.  The locations of these IDs and what objects they refer to are stored in maps.  As the objects are laid out their own IDs are also stored.  In the second stage, now that the IDs of all objects are known, all IDs in the data are filled in.  I.e., in the first pass the locations of objects are calculated and in the second pass any references to these locations are filled in.

The compiler uses path compression nodes whenever conditions permit.  It decides between high and low nodes by calculating the number of bytes each would take and using whichever is smaller.  From a time performance view, this approach favors low nodes too strongly.  This choice can be adjusted by setting a high node weight (see `example.md`).

To avoid repeating calculations about IF data, a NodeOracle is used.  The NodeOracle does all needed calculations once and the compiler then asks the oracle for the results as needed.
