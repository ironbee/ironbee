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
 * @brief IronAutomata --- Top level API documentation.
 *
 * This file contains no code, only API documentations.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

/**
 * @defgroup IronAutomata IronAutomata
 * IronAutomata Framework, C code.
 *
 * @sa @ref ironautomata.
 */

/**
 * @namespace IronAutomata
 * Top-level IronAutomata namespace.
 *
 * @sa @ref ironautomata.
 */

/**
 * \page ironautomata IronAutomata --- A compact automata framework.
 *
 * IronAutomata is a framework for building and executing automata.  It
 * separates automata execution, optimization, and generation into separate
 * stages.  This separation provides three advantages:
 *
 * 1. Each stage can focus on its particular problems.  Generators can use an
 *    expressive automata representation and not worry about generic
 *    optimizations; Engines can use a compact or fast representation;
 *    etc.
 * 2. Code can be reused.  A generic enough engine can run automata from a
 *    variety of generators (algorithms).  Many optimizations can similarly
 *    be applied independent of generator.
 * 3. The automata can be generated and compiled into specific engine
 *    representations and then stored for execution.  This allows those
 *    earlier stages to be executed in environments with greater speed,
 *    memory, or dependencies than are available to the engine.  It also
 *    allows the earlier stages to be executed once and the result reused many
 *    times in the engine.
 *
 * @section terminology Terminology
 *
 * - The *intermediate format* is an expressive representation of automata.
 * - A *generator* constructs an automata in intermediate format according to
 *   some algorithm and inputs, e.g., Aho-Corasick on a list of words.
 * - An *optimizer* or *transformation* manipulates automata, reading and
 *   writing automata in intermediate format.  Examples include
 *   determinization.
 * - A *compiler* transforms an automata in intermediate format to one in an
 *   engine specific representation.
 * - An *engine* executes an automata on input.
 *
 * @section standard_model Standard Model
 *
 * The Standard Model is the set of capabilities provided to generators and
 * optimizers.  The intermediate format (see below) must be expressive enough
 * to allow for any automata that fits in the standard model, but other
 * formats are possible.
 *
 * An engine is not required to fully support any automata expressible in the
 * standard model, so long as its compiler reports failures.  For example, the
 * initial engine, Eudoxus, does not support non-deterministic automata.  For
 * some limitations, such as requiring deterministic automata, transformations
 * can be written to convert automata into compatible forms.
 *
 * The standard model is still under construction.  Here is a rough summary
 * of the current state.
 *
 * - Deterministic or Non-Deterministic finite automata.  It is possible to
 *   have multiple edges with the same value.
 * - Alphabet is [0,255], i.e., bytes.  This fixed alphabet may change, but
 *   at present, the use of bytes is sufficient for the needs and is small
 *   enough to allow the use of bitmaps to represent subsets of it.
 * - Default edges.  A node may have a special "default" edge that is followed
 *   if no other edge is.  Furthermore, this is cheap, i.e., cheaper than
 *   creating an edge for every other input.
 * - Non-Advancing edges.  An edge (including the default edge) can either
 *   advance (consume) or not advance the input.  As with default edges, this
 *   is not strictly necessary --- any automata with non-advancing edges
 *   can be converted into one without --- but the ability to express them
 *   directly allows for more efficient implementation.
 * - Outputs on node entrance.  Each node may have zero or more outputs that
 *   are reported to the user when that node is entered.
 * - Output Trees.  Outputs are arranged reverse trees.  I.e., each output
 *   contains a referent to the next output.  This arrangement allows cheap
 *   representation of outputs when the output set of one node is a superset
 *   of the output of another node.
 * - Output Suppression.  An automata may require, as a global property, that
 *   output is only emitted if the input was just advanced.
 *
 * @section intermediate_format Intermediate Format
 *
 * The intermediate format represents the interface between the expressive
 * world of generators and optimizers and the high focused world of engines.
 * As such, it is an important design point and future refinements are likely.
 *
 * The intermediate format is a chunked, gzipped, protobuf format:
 *
 * - *Chunked*: The format is a sequence of length, message pairs.  No
 *   requirements are put on the maximum or minimum length of a message.  The
 *   ability to represent automata in multiple chunks allows automata
 *   streaming.  For example, the `intermediate_to_dot` program only loads a
 *   single chunk into memory at a time.
 * - *Gzipped*: Each protobuf message is gzipped to reduce storage.
 * - *Protobuf*: Protocol Buffers are used to allow easy, cross-language,
 *    reading and writing of the intermediate format.
 *
 * The intermediate format fairly directly represents the standard model with
 * a couple caveats:
 *
 * - It makes use of numeric identifiers for nodes and outputs.  The 0
 *   identifier is reserved for indicating a nil value, i.e., no referent.
 *   Otherwise, identifiers can be any positive integer.  There is no
 *   requirement that identifiers be contiguous.  In addition, nodes and
 *   outputs have separate identifier spaces.
 * - Edges can have multiple values indicating that the edge should be taken
 *   on any matching input.  These values can either be represented as a
 *   vector of bytes (a string) or as a bitmap where a value of 1 in the *i*th
 *   bit indicates that an input of *i* is matched.
 *
 * @section Eudoxus The Eudoxus Engine
 *
 * The initial engine is called Eudoxus.  It is written in C and is highly
 * oriented and minimizing space costs.  This focus on space costs is
 * motivated by the requirement to support Aho-Corasick on large dictionaries.
 * An algorithm which can result in huge automata.
 *
 * For details, see eudoxus.h and eudoxus.c.  In summary,
 *
 * - Eudoxus loads the automata as a single buffer.  Identifiers are offsets
 *   into that buffer.  Eudoxus can support different identifier widths.
 *   E.g., if the automata fits into 2^16 bytes, then only 2 bytes will be
 *   used for each identifier.
 * - Eudoxus uses multiple nodes types.  The most common are low degree nodes
 *   which store edge values in a vector and use linear search; and high
 *   degree nodes which uses bitmaps to represent edges.  There is also a
 *   path compression node that represents chains of nodes with otherwise
 *   identical behavior.  (Note: At the moment, only low nodes are
 *   implemented.)
 * - Eudoxus makes heavy length of variable length structures to omit data
 *   that is unused.  E.g., if a node has no default edge then no space for
 *   the target identifier is used.
 */
