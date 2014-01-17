IronBee Changes                                                   {#CHANGES}
===============

IronBee v0.9.0
--------------

**Build**

- Added `--with-boost-thread-suffix` to support packaging of `libboost_thread.so` without or without the `-mt` suffix.

- Ruby 1.9 now required.  This is a build dependency only; IronBee does not require Ruby to be used.  Build is now fully compatible with Ruby 2.0.  If you need to use alternative ruby or gem binaries, those can specified by setting RUBY and GEM in configure, e.g., 'configure RUBY=/usr/bin/ruby19 GEM=/usr/bin/gem19'.

- Added `--disable-ruby-code` and `--enable-ruby-code` configure options.  If `--disable-ruby-code` is specified, then ruby dependencies will not be checked and ruby based tests will not be run.  If `--enable-ruby-code` is specified, then dependencies will be checked and configure will fail if they are not present.  If neither option is specified, then dependencies will be checked but will not be fatal; tests will be run if all dependencies pass.

**Engine**

- The data field layer (rule targets) has been overhauled.  Data fields are now known as 'vars'.  Expansions and filters should be significantly faster.  `InitVarIndexed` and `InitCollectionIndexed` have been removed; their normal versions now Index automatically.

- Added API to load a module from a symbol rather than a file (`ib_module_load_from_sym()`) and an API to load a symbol from a file without initializing the module (`ib_module_file_to_sym()`).

- Rules claimed by alternative rule systems are allowed to not have a phase.  It is up to the alternative rule system to check the phase.

- All `void **` parameters have been replaced with `void *` parameters.  This allows them to be used to output to specific pointer types without a cast.

- Engine will now warn if loading a module compiled for a different version and error and refuse to load a module compiled for a different ABI.

- Moved request_header_finished_event after site context selection so that it has the correct configuration context.  Additionally added a request_header_process_event before site context selection to be used to process any header data prior to site context selection (e.g., normalize hostname, etc.)

- `ib_txdata_t` has been replaced with a `const char*`, `size_t` pair of arguments in all places.

- The parsed content interface (`parsed_content.h`) has been changed to take `ib_mpool_t` in place of `ib_tx_t`.  The header structures and methods have also been refactored to simpler names.

- Added rule tracing.  To use, configure IronBee with --enable-rule-trace.  This will add instrumentation code to the rule engine and enable the `RuleTrace` and `RuleTraceFile` directives.  `RuleTrace` takes a rule id as a parameter and enables tracing for that rule.  Traces will be output at the end of each transaction to either stderr or the file specified by `RuleTraceFile`.  Output is CSV of local ip, local port, remote ip, remote port, transaction id, rule id, number of times rule was evaluated, and total number of microseconds spent evaluating rule.  `RuleTraceFile` is context specific.

- Server callbacks now take pointer-length strings rather than NUL-terminated strings.

- Change buffer limit actions from Reject/RollOver to FlushAll/FlushPartial.

**Predicate**

- The Field call is now known as Var.  Field continues to exist as an alias for Var.

- Predicate now supports phaseless rules.  Phaseless rules will execute as early as possible.

- The long form of Var is now supported, allowing specification of a wait and final phase.

- A variety of simplifying transformations have been added.

- Added new directive, `PredicateTrace` which takes either "" (stderr) or a path and writes out a trace file of what Predicate is doing.  See `predicate/ptrace.pdf`.

- Fixed bug causing Predicate rules to fire multiple times.

- Predicate evaluation state has been moved out of the Node subclasses and into a NodeEvalState class.  This improves const correctness and removes the dependence of Predicate on specific multithreading approaches.  In particular, Predicate now works with continuation approaches.

- Utility functions like P.define(...) are moving to a new namespace (PUtil) and will all start with uppercase letters (e.g., PUtil.Define(...)).  Deprecation warnings are enabled and old naming conventions should be changed to the new format.

- Predicate now fires a Predicate rule for each value in the valuelist of the top node for that rule instead of only once.

- There is now a `set_predicate_vars` action.  This action can be placed as the **first** action.  It will set the `PREDICATE_VALUE` and `PREDICATE_VALUE_NAME` for each value in the valuelist.  These vars may then be used by other actions for that rule.
**Lua**

- LuaCommitRules is deprecated and should not be used. Lua rules are committed to the engine automatically at the end of every Lua file parse.

**Core**

- The trasnformation ifloor is now an available action that returns an number instead of a float.
- The trasnformation iceil is now an available action that returns an number instead of a float.
- The trasnformation iround is now an available action that returns an number instead of a float.
- The trasnformation floor is now aliased to ifloor and should be considered deprecated.
- The trasnformation ceil is now aliased to iceil and should be considered deprecated.
- The trasnformation round is now aliased to iround and should be considered deprecated.

**Servers**

- TrafficServer: Compatible with 4.1.x.
- TrafficServer: Added support for writting ironbee transaction logs.

**Fast**

- Added `extract_waggle.rb` to extract fast patterns from waggle rules and updated `build.rb` to use appropriately for `.lua` and `.waggle` files.

- Added support for Lua/Waggle to `suggest.rb`.  Use `suggest.rb --lua`.

**CLIPP**

- Added ClippScript, a Ruby DSL for creating CLIPP inputs.  See `clipp/clippscript.md`.

- view:summary now adds a "CLIPP INPUT" prefix to each summary line.

- Clipp Test now has support for asserting on a per-input basis.  See `assert_log_every_input_match` and `assert_log_every_input_no_match`.

- Added `@add` and `@addmissing` modifiers to add headers (always and conditionally, respectively).

- ClippTest is now more usable outside of `make check`.  Previously, ClippTest required `top_builddir` and `abs_top_builddir` to be defined in the environment and used the former for output and the latter for finding `clipp` and modules.  Now, if the former is missing, the current directory is used instead; if the latter is missing, ClippTest will try to use an installed IronBee's `clipp` and modules.

- The `clipp_announce` action now supports variable expansions.

**IronBee++**

- The `IBPPTestFixture` class used in IronBee++ test fixtures has been promoted to part of the public API as `IronBee::TestFixture`.  This makes it easier for other IronBee++ based code to write unit tests.
- `ConfigurationParser::create()` no longer informs the engine that configuration has started; `ConfigurationParser::destroy()` no longer informs the engine that configuration has finished.  Instead, use the new methods `Engine::configuration_started()` and `Engine::configuration_finished()`.  This change brings IronBee++ in line with C API semantics and will be useful for future support of other configuration modes.

- `IronBee::Server` now has methods for setting callbacks to C++ functionals.

- Added initial ParserSuite support: a function to translate a sequence of ParserSuite headers to a sequence of `IronBee::ParserHeader`s.

IronBee v0.8.1
--------------

**Build**

* Use EXTRA_LDFLAGS from apxs, but do not use non-existent library search paths.

**Engine**

* Do not process events when there is no data.

**Rule Engine**

* Fixed issues blocking outside of rules in response (XRules).

**XRules**

* Fixed path comparison that should have been a prefix match.

**Waggle**

* Fixed capture action (really a modifier).
* Fixed loop detection in follows().

**Bugs**

* Fixed a mis-placed assert() in whitespace removal.

**Clipp**

* Fixed issues with assert_log_evry_input_no_match.

**LibHTP**

* Updated LibHTP parser to v0.5.9.

IronBee v0.8.0
--------------

**Deprecations**

* The 'ac' module (deprecated in 0.7.0) has been removed.
* Directive "DefaultBlockStatus 403" is repaced by "BlockingMethod status=403"

**Build**

* Modules and plugins are now installed into libexec instead of lib.

* New macros are available, `NONULL` and `ALL_NONNULL_ATTRIBUTE`, for telling
  gcc and clang that certain parameters should never be NULL.  Some APIs
  (e.g., mpool, hash) make use of these new macros.

**Predicate**

* A new rule injection system, Predicate, was added.  Predicate provides a
  functional approach to writing rules and is designed to make rule logic
  composition and reuse easier and provide performance benefits.  See
  `predicate/predicate.md` for an overview.

**Engine Manager**

* An engine manager has been added.  The engine manager provides the ability
  for server plugins to easily handle reconfigurations.  Upon receiving
  notification of the reconfigure event, the server asks the engine manager
  to create a new IronBee engine.  If successful, the manager will then make
  the new engine current, and will destroy old engines once they are no longer
  used.

* The Traffic Server plugin has been modified to use the engine manager.

**Engine**

* Operators have been overhauled.  They are now entirely independent of the
  rule engine and can be called by any code.  The API has been significantly
  simplified as well.

* Added `ib_module_config_initialize()`.  This function provides an
  alternative approach to initializing module configuration data.  The
  original (still existent) method is to store an initial configuration data
  value and length in the module structure.  The new approach is to call
  this function in the module initialization handler.

* Modules now provide their static `ib_module_t` as a `const ib_module_t *`
  instead of an `ib_module_t *`.  The engine makes its own copy rather than
  reusing the static.  This change allows simultaneous use of modules by
  multiple engines.

* The context hook functions have been removed from the module initialization
  structures, and have been replaced with context hook registration functions.

* As part of the provider removal project, the matcher provider was
  removed -- nothing was using it; the parser provider was removed -- modhtp
  now provides parsing via engine hooks; and the audit log provider was
  removed -- audit logging is now contained entirely within core.

* Added indexed data fields which allows modules to register data field keys
  that are known at configuration time for rapid lookup.  Most pre-defined
  fields have been set as indexed.  Module authors that create fields should
  consider registering those keys as indexed during initialization via:
  `ib_data_register_indexed(ib_engine_data_configuration_get(ib), "my key")`.
  Custom data fields can be indexed via the `InitVarIndexed` and
  `InitCollectionIndexed` directives.

* Transformations have been overhauled: output flags have been removed;
  callback data is now the final argument; input flags have been changed to
  a single bool; added accessors; `ib_tfn_transform()` has been renamed to
  `ib_tfn_execute()` and now handles lists properly; separated creation and
  registration similar to operators.

* All `ib_hook_xxx_unregister()` functions have been removed.

**Util**

* Add external iterator support for hash.  See `ib_hash_iterator*`.
  `ib_hash_get()` and `ib_hash_get_ex()` now support NULL for the value
  argument to allow for membership tests.

* Hash keys are now consistently `const char *` instead of a mix of
  `const char *` and `const void *`.

* Hash now supports callback data for key hashing and equality.

**IronBee++**

* IronBee++ includes full support for operators and adds an optional
  functional based interface that can significantly simplify operator
  definitions, especially in C++11.

* Module delegates are now constructed on module initialization rather than
  load.  As a result, the `initialize()` method is no longer called.  This
  change makes it easier to write modules that function in multiple engine
  environments.

* Added static `Module::with_name(engine, name)` to acquire a module of a
  given name, i.e., `ib_engine_module_get()`.

* `convert_exception()` now only requires a ConstEngine instead of an Engine.

* Added `IronBee::Hash<T>`.

* Exceptions can now have a transaction or configuration parser attached to
  them (`errinfo_configuration_parser` and `errinfo_transaction`) which will
  be used to improve the log message.  Also, logging can be prevented by not
  attaching an `errinfo_what`.

* IronBee++ includes full support for transformations.

**CLIPP**

* Added `-e path` which causes `clipp` to handle consumer errors differently.
  On the first error, `clipp` will write the last input to `path` in protobuf
  format and exit.

* Added `@clipp_print` operator to IronBee modifier and consumer which outputs
  its argument and input to standard out.

**Other**

* Added `example_modules` directory with example modules.

* Major test organization overhaul.  The `tests` directory now holds only
  common test code. Module tests now in `modules/tests`, engine tests in
  `engine/tests`, and utility tests in `util/tests`.

* CLIPP based tests now use more meaningful filenames.  Filenames for the
  same test now use the same identifier.  Numbers in identifiers are
  incremental rather than random and identifiers now include the name of the
  test.

* CLIPP based tests no longer require modhtp.

* Added `ibmod_ps` ("ps" stands for ParserSuite), a module of mini parsers
  exposed as operators.  Can be used to validate format of any string and,
  via captures, to parse it into components.

* Various clean up and bug fixes.

IronBee v0.7.0
--------------

**Deprecations**

* The `ac` module is deprecated.  It will emit a warning if loaded.

**Documentation**

* Syntax added to all operators.

* Preface added.

**Build**

* libhtp is now configured as part of configure stage rather than build
  stage.  In addition, libhtp will make use of any configure options.  Use
  ``./configure --help=recursive`` to see libhtp specific configure options.

* Extensive cleanup regarding use of `CFLAGS`, `CXXFLAGS`, etc.  Those
  variables are now respected and may be specified at configure or make time.
  Several configure options used to control those variables have been removed
  in favor of directly setting them.

* Warning settings changed to `-Wall -Wextra`.  `-Werror` will be enabled on
  newer compilers (any clang or gcc 4.6 or later).

* Build system now compatible with automake 1.13.  In addition, IronBee will
  take advantage of the new parallel test harness if automake 1.13 is used.

* Configure now checks for `ruby`, `gem`, and `ruby_protobuf` gem if C++ code
  is enabled.

* Configure now checks for `libcurl` and `yajl` and only enabled RIAK support
  if present.

* The Clang Thread Sanitizer is now supported.  However, a few tests cause
  false positives or break the thread sanitizer.  Pass
  `--enable-thread-sanitizer-workaround` to `configure` to disable these
  tests.  See the thread sanitizer documentation for how to enable it.

* Several unneeded checks removed.

**Configuration**

* Added `InspectionEngineOptions` to set the defaults for the inspection
  engine.

* Added `IncludeIfExists` directive to include a file only if it exists and is
  accessible.  This allows for inclusion of optional files.

**Engine**

* `ib_tx_t::data` has changed from a generic hash to an array indexed by
  module index.  This change puts it in line with per-module engine data and
  per-module context data.  `ib_tx_data_set()` and `ib_tx_data_get()` can be
  used by modules to read/write this data.

* Added RIAK kvstore.

* Several fixes to dynamic collections in the DPI.

* Lua rule support moved from the rule component to the Lua module.  The rules
  component gained support for modules to register arbitrary external rule
  drivers (see `ib_rule_register_external_driver()`), which the Lua module
  now uses.

* Data fields were cleaned up and refactored.  Notable changes to the public
  API include:

  * All capture related data routines have been moved to capture.h and begin
    `ib_capture` instead of `ib_data`.
  * Several transformation functions have been moved to transformation.h and
	to `ib_tfn` from `ib_data`.
  * All remaining data routines are now in `data.h` instead of `engine.h`.
  * All public `dpi` fields are now `data`.
  * To disambiguate, previous module data code has moved from `data` to
    `module_data`.

* Added managed collections which allow TX collections to be automatically
  populated / persisted.

* Added a core collection manager which takes one or more name=value pairs,
  and will automatically populate a collection with the specified name/value
  pairs.

* Added a core collection manager which takes a JSON formatted file,
  will automatically populate a collection from the content of the file.
  Optionally, the collection can persist to the collection, as well.

* Removed backward compatibility support for the `ip=` and `port=` parameters
  to the Hostname directive.

* Removed backward compatibility support for `=+` to the `SetVar` action.

* Logging overhaul.

  * For servers, use `ib_log_set_logger` and `ib_log_set_loglevel` to setup
    custom loggers.  Provider interface is gone.
  * For configuration writers, use `Log` and `LogLevel`; `DebugLog` and
    `DebugLogLevel` are gone.  `LogHandler` is also gone.
  * For module writers, use `ib_log_vex` instead of `ib_log_vex_ex`.  Include
    `log.h` for logging routines.
  * For engine developers, logging code is now in `log.c` and `log.h`.

* LogEvents has been refactored to use a direct API rather than a provider.

* Added utility functions that wrap YAJL, using it to decode JSON into an
  `ib_list_t` of `ib_field_t` pointers, and to encode an `ib_list_t` of
  `ib_field_t` pointers into JSON.

* Added `@match` and `@imatch` operators to do string-in-set calculations.

* Added `@istreq`, a string insensitive version of `@streq`.

* Support for unparsed data has been removed from IronBee.

  * The `ib_conndata_t` type has been removed.
  * `ib_conn_data_create()` has been removed.
  * The `ib_state_conndata_hook_fn_t` function typedef has been removed.
  * The `ib_hook_conndata_register()` and `ib_hook_conndata_unregister()
`    functions have been removed.
  * The `ib_state_notify_conn_data_in()` and `ib_state_notify_conn_data_out()
`    functions have been removed.

* The libhtp library has been updated to 0.5.

* All memory pool routines now assert fail instead of returning `EINVAL` when
  passed NULLs for require arguments.

**Modules**

* The `pcre` module has been updated to use the new transaction data API.

* The `pcre` module `dfa` operator now supports captures.

* Added a 'persist' module, which implements a collection manager that can
  populate and persist a collection using a file-system kvstore.

* Added a 'fast' module which supports rapid selection of rules to evaluate
  based on Aho-Corasick patterns.  See below and `fast/fast.html`.

* Added a module implementing libinjection functionality to aid in detecting
  SQL injection. This module exposes the `normalizeSqli` and the
  `normalizeSqliFold` transformations as well as the `is_sqli` operator.

* Added a module implementing Ivan Ristic's sqltfn library for normalizing
  SQL to aid in detecting SQL injection. This module exposes the
  `normalizeSqlPg` transformation.

* The `htp` module has been vastly reworked to work properly with libhtp 0.5.

**Fast**

* Added a variety of support for the fast rule system (the fast module
  described above is the runtime component).  Support includes utilities to
  suggest fast patterns for rules and for easy generation of the fast automata
  needed by the fast module.  See above and `fast/fast.html`.

**IronBee++**

* Moved catch, throw, and data support from internals to public.  These
  routines are not needed if you only use IronBee++ APIs but are very useful
  when accessing the IronBee C API from C++.

* Fixed bug with adding to `List<T>` where `T` was a `ConstX` IronBee++ class.

**Automata**

* Intermediate format and Eudoxus now support arbitrary automata metadata in
  the form of key-value pairs.  All command line generators include an
  `Output-Type` metadata key with value set to the output type as defined by
  `ee`.  `ee` now defaults to using this metadata to determine output type.
  This changes increments the Eudoxus format version and, as such, is not
  compatible with compiled automata from earlier versions.

* Eudoxus output callbacks are now passed the engine.

* Added `ia_eudoxus_all_outputs()` to iterate through every output in an
  automata.  `ee -L` can be used to do this from the command line.

* Added '\\iX' to Aho Corasick patterns which matches upper case of X and
  lower case of X for any X in A-Za-z.

* Added '\$' to Aho Corasick patterns which matches CR or NL.

* Added union support to Aho Corasick patterns, e.g., `[A-Q0-5]`.

**Clipp**

* All generators except `pb` now produced parsed events.  Use `@unparse` to
  get the previous behavior.  But note that IronBee no longer supports
  unparsed events.

* Several tests have been added, including a randomized test of IronBee in
  both single and multithreaded mode (`test_holistic`).

* The parse modifier now generates a complete set of events even if some of them are data less.  For example, if no headers are present provided in
  connection data, clipp will still produce a `REQUEST_HEADERS` event; before
  this change it would not.

**Other**

* The old CLI (ibcli) has been removed.

* Removed FTRACE code.

* Various bug fixes and cleanup.

IronBee v0.6.0
--------------

**Build**

* IronBee++ and CLIPP are now built by default.  Use `--disable-cpp` to
  prevent.

* Build system now handles boost and libnids libraries better.  New
  `--with-boost-suffix` configuration option.

* Removed a number of unnecessary checks in configure.

* Included libhtp source, so this is no longer required.

**Engine**

* Enhanced support for buffering request/response data, including
  runtime support via the setflag action.

* Added initial support for persistent data. (see:
  `include/ironbee/kvstore.h`)

* Partial progress towards rework of configuration state transitions.
  Currently implicit.  Next version should be gone completely.

* Events can now be suppressed by setting the `suppress` field.

* Directory creation (`ib_util_mkpath`) rewritten.

**Rules, Configuration and Logging**

* Enhanced rule engine diagnostics logging (`RuleEngineLogData`,
  `RuleEngineLogLevel`).

* Simplified Hostname directive by moving IP/Port to a new
  Service directive.  For 0.6.x only, support the "ip=" and "port="
  parameters to the Hostname directive for backward compatibility with 0.5.x.

* Enhanced configuration context selection, which now takes Site,
  Service, Hostname and Location into account.

* Added an `InitVar` directive to set custom fields at config time.

* `SetVar` `=+` operator changed to `+=`.  Also added `-=` and `*=`.  For
  0.6.x only, support `=+` for backward compatibility with 0.5.x.

* Added floating point field type; removed unsigned field type.  Note that
  floating point values do not support eq and ne.

* The `ne` operator now correctly compares numbers.

* Initial support for implicit type conversions in operators.

* Fixed `pmf` operator so that relative filenames are based on
  config file location vs CWD.

* Enhanced PCRE matching to support setting limits.

* `AuditLogFileMode` now works.

* Default of `AuditEngine` is now `RelevantOnly`.

* Cleaned up audit log format, removing event level action and adding
  transaction level action, message, tags and threat level.

**Lua**

* Updated luajit code to v2.0.0.

* Enhanced Lua rule API with more access to internals.

**Modules**

* Enhanced GeoIP module to use O1/O01 country codes when
  lookups fail.

**Servers**

* Added support for regexp based header editing.

* Rewrote Apache httpd server module for httpd 2.4.

**Automata**

* Added IronAutomata framework for building, modifying, and executing automata
  (see: `automata/doc/example.md`).  Currently works as stand alone library
  but is not integrated into IronBee.

**CLIPP**

* CLIPP manual updated. (see: `clipp/clipp.md`)

* CLIPP tests now provide more information about failures.

**IronBee++**

* Support for new site API.

* Support for new float field type.

**Documentation**

* Added CHANGES file.

* Many manual updates.

* Doxygen dependency calculation fixed.  `make doxygen` in `docs` should now
  run only if files have changed.

* Removed long deprecated `fulldocs` doxygen.  Use `external` or `internal`
  instead.

* Updated to doxygen 1.8.1.

**Other**

* Various bug fixes and code cleanup.


