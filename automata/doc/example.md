Title:  An Annotated Example of IronAutomata
Author: Christopher Alfeld <calfeld@qualys.com>
xhtml header: <script type="text/javascript" src="http://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS-MML_HTMLorMML"></script>

This document can be rendered to HTML (or many other formats) with [MultiMarkdown][].

[MultiMarkdown]: http://fletcherpenney.net/multimarkdown/

An Annotated Example of IronAutomata
====================================

Christopher Alfeld<br>
calfeld@qualys.com

Overview
--------

In this document a small Aho-Corasick automata will be created, explored, compiled and executed.  Along the way, many features of IronAutomata will be demonstrated and discussed.

This document is oriented at users rather than developers.  Developers interested in executing IronAutomata automata in their own code should look at `eudoxus.h`.  Developers interested in developing for IronAutomata, e.g., additional optimizers or generators, should look at `apidoc.h`.

This document does not cover using automata in IronBee.  However, the Eudoxus automata files (`.e`) generated are suitable for loading into IronBee.

In IronAutomata, a description is used to *generate* an automata.  That automata may then be manipulated by one or more *optimizers* and then *compiled* into a form optimized for applying the automata to an input.  The compiled automata can be then be executed one or more times against an input, generating results.  Automata frequently behave as filters and are used to classify the input or extract portions of it.

Automata and their execution are briefly described in step 2 below, however, if you are unfamiliar with automata, it may be beneficial to review them.  A review can be found on [wikipedia][] or in a variety of algorithm text books.

[wikipedia]: [http://en.wikipedia.org/wiki/Automata_theory]

Step 0: Environment
-------------------

At present, IronAutomata is part of the IronBee tree.  You will need to check
out, configure, and build IronBee.[^cpp]  The rest of the example will take place in the `automata` subdirectory of a build tree of IronBee.  The subdirectory `bin` will contain all the program you will use.

[^cpp]: If IronBee is compiled with `--disable-cpp`, only the Eudoxus portion (`ec` and `ee`) will be built as the rest of IronAutomata is written in C++.

You will also need GraphViz installed; it will provide the program `dot`.

Step 1: Aho-Corasick
--------------------

Aho-Corasick (AC) is an algorithm for solving the following problem: Given a set of strings \\(D = \{w_1, w_2, \ldots, w_n\}\\) and a text \\(T\\), find all locations in \\(T\\) where a \\(w_i\\) is a substring.  I.e., the result \\(R = \{(i, j) : w_i \in D \text{ appears in } T \text{ at location } j\}\\).  The set of strings \\(D\\) is referred to as the *dictionary*.

The strength of Aho-Corasick is its ability to handle a large dictionary.  Aho-Corasick runs in time proportional to the length of the input and (mostly) independently of the size of the dictionary.  In contrast, regular expressions are more expressive but (usually) oriented at single application.  E.g., using AC with 10,000 substrings is fine, searching for those 10,000 substrings one at a time is vastly slower, and applying 10,000 regular expressions one at a time is even slower.[^acre]

[^acre]: Aho-Corasick is similar to doing a global search for the regular expression \\(w_1|w_2|\ldots|w_n\\), where the \\(w_i\\) are literal strings (i.e., no regex operators).  AC is well oriented to performing this task and, unlike most regex engines, will tell you which \\(w_i\\) matched at each input location.

AC works in two stages.  In the first stage, the dictionary is converted into a specialized automata.  In the second stage, the automata is executed against an input to generate the result.  The automata from the first stage can be used against any input.  While generating the automata takes some time, executing it against an input is fast.  The advantage of AC is the fast execution time, the disadvantage is that the automata tends be large.

An animated description of Aho-Corasick can be found [here].

[here]: http://blog.ivank.net/aho-corasick-algorithm-in-as3.html

IronAutomata was developed with AC as a major application.  As such, the ultimate representation of the automata will be highly compact to accommodate the large AC automata.

This example will use a very simple dictionary: \\(D = \{he, she, his, hers\}\\).

    > echo -e "he\nshe\nhis\nhers" | bin/ac_generator > example.a
    > wc -c example.a
         527 example.a

The `ac_generator` program constructs an AC automata in a format known as *intermediate format*.  The intermediate format is oriented at generation and manipulation rather than execution.

Step 2: Looking at the Automata
-------------------------------

Any automata in intermediate format can be drawn via GraphViz.  In practice, GraphViz can only accommodate small to moderately sized automata

    > bin/to_dot example.a | dot -Tpng -Gsize=6 > example1.png

![Aho-Corasick of he, she, his, hers][example1]

[example1]: example_example1.png

The diamond is the *start node* of the automata.  Solid arrows point to elipses representing other nodes and are labeled by the input.  The dashed arrows represent *default* edges.  These edges are taken when no other edge from a node matches the input.  Black edges advance to the next input; red edges do not.  Dotted black arrows point to outputs which are generated when the node is entered.  The outputs in this example are four byte integers in network byte order.  Unprintable output bytes are rendered as decimals in angle brackets.  For example, when node 7 is entered, the outputs 3 and 2 will be emitted.

As example, the text "shep" will start at node 1 and enter nodes 2, 4, and 7 in that order.  At node 7, the outputs 3 and 2 will be emitted.  As there is no edge for the current input "p", the default edge will be followed to node 6 at which point input is done.

Step 3: Optimizing the Automata
-------------------------------

IronAutomata comes with a small but growing set of automata optimizations.  Some of these optimize for space and others for speed.  The generator, `ac_generator` automatically applies two optimizations: output deduplication and edge optimization.  Here a speed optimization, translate nonadvancing, will be applied:

    > bin/optimize --translate-nonadvancing-conservative < example.a > example_opt.a
    Translate Nonadvancing [conservative]: 2297
    > wc -c example_opt.a
         605 example_opt.a
    > bin/to_dot example_opt.a | dot -Tpng -Gsize=6 > example2.png

![Result of Translate Nonadvancing Conservative][example2]

[example2]: example_example2.png

The result is a more complicated automata.  But notice the lack of red edges.  That means that every edge advances the input and, thus, only one edge will be traveled for every byte of input.  In contrast, the original automata might traverse multiple edges for each byte.

In practice, for AC automata, the translate nonadvancing conservative optimization is often not worthwhile.  Although there are fewer edge transitions, determining the next edge can be more difficult and the larger automata size can negatively impact cache behavior as well as consume additional memory.  Any optimization should be measured, benchmarked, and evaluated.[^structural]

Additional discussion on translate nonadvancing can be found in an [appendix][Appendix:TranslateNonadvancing].

[^structural]: A more restricted version, translate nonadvancing structural, can be beneficial to AC automata and will never grow automata size.  In the example given, it would have no effect.  It can be applied via `bin/optimize --translate-nonadvancing-structural`.  It is also part of the space optimization suite: `bin/optimize --space`.

Step 4: Compiling the Automata
------------------------------

The intermediate format is good for generators and optimizers and allows them to function without any awareness of the specific execution engine the automata will run on.  However, to execute an automata it must be converted to a representation specific to an execution engine.  At present, there is a single execution engine in IronAutomata, Eudoxus.

Eudoxus is written in C and oriented at optimizing memory usage and thus supporting large automata.  To compile an automata into a Eudoxus automata, use `ec`:

    > bin/ec example.a
    bytes            = 107
    id_width         = 1
    align_to         = 1
    high_node_weight = 1
    ids_used         = 25
    padding          = 0
    low_nodes        = 8
    low_nodes_bytes  = 38
    high_nodes       = 0
    high_nodes_bytes = 0
    pc_nodes         = 1
    pc_nodes_bytes   = 5
    bytes @ 1        = 107
    bytes @ 2        = 132
    bytes @ 4        = 182
    bytes @ 8        = 282
    > wc -c example.e
         107 example.e

By default, `ec` converts `X.a` to `X.e`.  This behavior can be changed via  command line arguments.

Note that `ec` converted the 527 byte input to a 107 byte output.  Those 107 bytes also represents the amount of memory (plus a small additional amount that is independent of the automata) that Eudoxus will use during execution.  I.e., the automata will be loaded directly into memory without any decompression or modification.[^filecompression]

[^filecompression]: Intermediate format, `.a`, files are already compressed and further compression is of limited value.  They are decompressed when loaded into memory.  In contrast, Eudoxus, `.e`, files are not compressed and loaded directly into memory.  Their disk space usage can be further reduced with any compression program.  The API provides loading Eudoxus automata from a memory buffer without copying, so they could be loaded, decompressed, and then used in Eudoxus.

The `id_width` is an important aspect Eudoxus automata[^idwidth].  All Eudoxus automata have an id width that is either 1, 2, 4, or 8.  Lower id widths can result in significantly smaller automata but impose a maximum size, in bytes, on the automata:

[^idwidth]: Internally, ids are used to identify and refer to node and output objects in the automata.  They are interpreted as indices into the automata data, e.g., an id of 123 refers to an object at the 122nd byte (0 based indices) of the automata.

| ID Width | Maximum Automata Size ||
| -------- | ---------------------  |
| 1        | 256 Bytes              |
| 2        | 65  Kilobytes          |
| 4        | 4   Gigabytes          |
| 8        | 18  Exabytes[^exabyte] |
[ID width limits on automata size]

[^exabyte]: This 18 exabytes limit is theoretical as in order to actually reach it, a memory architecture with > 64 bit addresses would be needed.  In practice, at least for the present, the limit is the available memory.

The output of `ec` gives the current id width (`id_width`), bytes in that id width (`bytes`), and how many bytes the automata would be in every other id width (`bytes @ X`).

Space-time tradeoffs that increase the size sufficiently to necessitate a higher id width may lose their time benefit due to worse caching.  Alternatively, reducing an automata to fit within a lower id width can provide time performance benefits.

By default, `ec` choose the smallest id width that suffices.  The id width can be fixed via command line options.  Doing so can decrease compilation time: if you are in modify-compile-test cycle it may be worth fixing an id width to decrease compilation time, and then doing a minimal id width compile at the end.

See an [appendix][Appendix:Tradeoffs] for discussion of space-time tradeoffs.

Eudoxus automata are compiled for the Endianness of the platform `ec` is run on.  They will fail to execute on an architecture of a different Endianness.  In the future, converters (manual or automatic) may be provided.  At present, you must ensure that you compile automata on the same Endianness you wish to execute them.

Step 5: Executing the Automata
------------------------------

Eudoxus is intended to be embedded in other software.  However, a command line executor, `ee`, is available.

    > echo "she saw his world as he saw hers..." | bin/ee example.e
    Loaded automata in 0.086263 milliseconds
           3: she
           3: he
          11: his
          23: he
          30: he
          32: hers
    Timing: eudoxus=0.009763 milliseconds output=0.172022 milliseconds

The executor, `ee`, also provides some timing information.  For an automata and input this small, any signal in the timing will be lost in the noise, but for larger automata and over repeated runs[^repeatedruns], it can be used to evaluate automata, optimizations, and trade offs.  Timing is divided into load time (first line), time spent in Eudoxus (`eudoxus=X`) and time spent in `ee`'s output handling (`output=X`).  See an [appendix][Appendix:Tradeoffs] for an example of such evaluation.

[^repeatedruns]: `ee` has a command line flag `-n N` which causes it to execute the automata against the input `N` times.

The executor, `ee`, supports other output types and can count outputs rather than list them.  See `ee --help` for more information.

Appendix 1: A Bigger Example
----------------------------

If installed, `/usr/share/dict/words` is a list of words.  In this example, an AC automata will be generated from it and then run against it.  The automata will be far too large to draw with GraphViz and the outputs will be omitted from `ee` for length reasons.

    > wc -l /usr/share/dict/words
      235886 /usr/share/dict/words
    > time bin/ac_generator < /usr/share/dict/words > words.a
    bin/ac_generator < /usr/share/dict/words > words.a  112.39s user 0.57s system 99% cpu 1:53.02 total
    > du -sh words.a
     36M	words.a
    > time bin/ec words.a
    bytes            = 11745866
    id_width         = 4
    align_to         = 1
    ids_used         = 2382391
    padding          = 0
    low_nodes        = 792776
    low_nodes_bytes  = 11696837
    high_nodes       = 1
    high_nodes_bytes = 245
    pc_nodes         = 0
    pc_nodes_bytes   = 0
    bytes @ 1        = 4598693
    bytes @ 2        = 6981084
    bytes @ 4        = 11745866
    bytes @ 8        = 21275430
    bin/ec words.a  96.52s user 1.18s system 99% cpu 1:37.73 total
    > du -sh words.e
     11M	words.e
    > time bin/ee -r nop -a words.e -i /usr/share/dict/words
    Loaded automata in 14.1823 milliseconds
    Timing: eudoxus=102.051 milliseconds output=0 milliseconds
    bin/ee -r nop -a words.e -i /usr/share/dict/words  0.12s user 0.04s system 100% cpu 0.157 total

We can attempt to improve things by applying the space optimization suite.  In the current example, this will result in a slightly smaller automata that runs noticably faster.  The space suite applies every optimization that will not increase the space.

    > time bin/optimize --space < words.a > words_opt.a
    Translate Nonadvancing [structural]: 171896
    Deduplicate Outputs: 146
    Optimize Edges: done
    bin/optimize --space < words.a >| words_opt.a  383.80s user 1.05s system 99% cpu 6:25.27 total
    > du -sh words_opt.a
     36M	words_opt.a
    > time bin/ec words_opt.a
    bytes            = 11744150
    id_width         = 4
    align_to         = 1
    ids_used         = 2382248
    padding          = 0
    low_nodes        = 792776
    low_nodes_bytes  = 11696837
    high_nodes       = 1
    high_nodes_bytes = 245
    pc_nodes         = 0
    pc_nodes_bytes   = 0
    bytes @ 1        = 4597406
    bytes @ 2        = 6979654
    bytes @ 4        = 11744150
    bytes @ 8        = 21273142
    bin/ec words_opt.a  96.27s user 1.13s system 99% cpu 1:37.47 total
    > du -sh words_opt.e
     11M	words_opt.e
    > time bin/ee -r nop -a words_opt.e -i /usr/share/dict/words
    Loaded automata in 14.0316 milliseconds
    Timing: eudoxus=95.784 milliseconds output=0 milliseconds
    bin/ee -r nop -a words_opt.e -i /usr/share/dict/words  0.11s user 0.06s system 73% cpu 0.227 total

Appendix 2: A Translate Nonadvancing Example [Appendix:TranslateNonadvancing]
--------------------------------------------

This example uses a contrived automata to demonstrate the translate nonadvancing optimizations.

    > echo -e "a\naa\naaa\naaaa\n" | bin/ac_generator > aaaa.a
    > bin/to_dot aaaa.a | dot -Tpng -Gsize=6 > aaaa.png

![Aho-Corasick of a, aa, aaa, aaaa][aaaa]

[aaaa]: example_aaaa.png

    > bin/optimize --translate-nonadvancing-conservative < aaaa.a > aaaa_conservative.a
    Translate Nonadvancing [conservative]: 1021
    > bin/to_dot aaaa_conservative.a | dot -Tpng -Gsize=6 > aaaa_conservative.png

![Result of applying Translate Nonadvancing Conservative][aaaa_conservative]

[aaaa_conservative]: example_aaaa_conservative.png

Note that node 8 now has two edges (including the default) instead of one and that no edges are non-advancing.

    > bin/optimize --translate-nonadvancing-structural < aaaa.a > aaaa_structural.a
    Translate Nonadvancing [structural]: 3
    > bin/to_dot aaaa_structural.a | dot -Tpng -Gsize=6 > aaaa_structural.png

![Result of applying Translate Nonadvancing Structural][aaaa_structural]

[aaaa_structural]: example_aaaa_structural.png

Note that node 8 retains its single (default) edge which is non-advancing, thus, no edges are added to the automata.

Appendix 3: Eudoxus Tradeoffs [Appendix:Tradeoffs]
-----------------------------

By default, `ec` generates the smallest automata possible.  It supports, via command line arguments, options to trade off space against time.

**Alignment**

Alignment causes `ec` to align the location of each node of the automata to a certain number of bytes.  If this alignment matches the natural memory alignment of the underlying architecture, and the automata itself is loaded into memory at this alignment (usually the case), it may provide faster memory access and thus superior performance.[^aligncaveat]  The alignment used (`align_to`) and number of additional bytes added (`padding`) is given.  In the case above, the alignment was 1 resulting in no padding.

[^aligncaveat]: The C compiler, however, is unaware of this alignment and so must generate machine code to cover the non-aligned case.  As such, the benefits are not what they might be.  In the future, a distinct Eudoxus subengine with aligned memory loads may be utilized to increase the benefit of alignment.

Alignment can be specified via `-a`, e.g., `-a 8`.

**High Node Weight**

The compiler will decide between representing a node as a "high node" or a "low node"[^high_low] by calculating the space cost, in bytes, of both options and using the lower.  This calculation can be adjusted via the high node weight which is a multiplier of the high node cost.  As such, a value of 1 results in the smallest possible automata, values less than 1 favor high nodes, and values greater than one favor low nodes.

[^high_low]: High nodes are focused on nodes with many outgoing edges.  They make extensive use of tables, providing fast lookup but with a high per-node overhead.  In contrast, low nodes use vectors of edges.  These give slow lookup, especially when there are many edges, but have low per-node overhead.  There is also a specialized, "path compression" node used to represent sequences of nodes.

The high node weight can be specified via `-h`, e.g., `-h 0.5`.

**Benchmarking**

The best way to use these options is to prepare a sample of the type of input you will be running your automata against, and then measure the space and time at various values.  For example, an Aho-Corasick automata generated from an English dictionary was run against Pride and Prejudice at various high node weight values.  The graph below shows the time (total time for 10 runs) and space usage:

![English Dictionary applied to Pride and Prejudice][example_pp]

[example_pp]: example_pp.png

This graph suggests a high node weight between 0.35 and 0.65 will yield significant performance benefits at low space costs.  E.g., a value of 0.5 will, compared to a high node weight of 1, run 22% faster and use only 4% more bytes.

Appendix 4: Advice for Eudoxus Automata
---------------------------------------

Based on limited benchmarks of Aho-Corasick automata from English dictionaries on English text:

* Apply translate nonadvancing structural optimization.  It may not help, but it can't hurt: `bin/optimize --translate-nonadvancing-structural`.  If not using `ac_generator`, use `--space` instead.
* Use a high node weight below 1.0.
* Do not use alignment.  The effects are minimal.  If/when Eudoxus gains an aligned subengine, it may be worthwhile.
* Create and run benchmarks to determine the effect of any of the above and any other modifications you try.  See [the previous appendix][Appendix:Tradeoffs] for an example.

Appendix 5: Summary of Programs
-------------------------------

**Generator**

Generator construct automata.

- `ac_generator`: Aho-Corasick: Find all substrings in a text.
- `trie_generator`: Trie: Find longest matching prefix in a text.

**Utilities**

Utilities operate on the intermediate format generated by generators.

- `optimize`: Apply optimizations.
- `to_dot`: Generate GraphViz representation of an automata.

**Eudoxus**

Eudoxus is an automata execution engine oriented as compact representation.

- `ec`: Compile an automata in intermediate format to Eudoxus format.
- `ee`: Execute an automata in Eudoxus format against an input.

