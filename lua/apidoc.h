/****************************************************************************
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
 *
 * @brief IronBee Lua --- Top level API documentation.
 *
 * This file contains no code, only API documentations.  It functions as the
 * main page of the API documentation.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @page LuaAPI Lua API
 *
 * @tableofcontents
 *
 * @section IronBeeLuaIntroduction Introduction
 *
 * IronBee ships with a copy of LuaJIT in the  @c libs directory. If you don't
 * disable support for Lua (@c --disable-lua) then you will get the
 * module @c ibmod_lua. This module gives you several configuration directives
 * which will interface with the Lua library code for IronBee.
 *
 * 1. @c LuaLoadModule - Load a module defined in Lua.
 * 2. @c LuaPackagePath - Set the Lua package path for Lua runtimes.
 * 3. @c LuaPackageCPath - Set the Lua C package path for Lua runtimes.
 * 4. @c LuaInclude - A Lua version of @c Include, this will include
 *                    a Lua file at configuration time and interpret it
 *                    in the context of a configuration DSL. The
 *                    @ref Waggle rule language is part of 
 *                    that configuration DSL.
 * 5. @c LuaCommitRules - Take rules defined in @ref Waggle and 
 *                       integrate them into the engine.
 *
 * @section LuaConfiguration Configuring IronBee through Lua
 *
 * Configuration IronBee is partially supported using Lua. Currently only
 * support of specifying Rules is available using the @c LuaInclude directive.
 *
 * @code{.lua}
 * LuaInclude "/usr/share/ironbee/lua/rules/myrules.lua"
 * @endcode
 *
 * See @ref Waggle documentation for how to write rules in Lua. Lua rules
 * expressed in Waggle are first-class rules when integrated with the IronBee
 * engine, and will execute with the same speed and semantics as rules
 * written in the configuration language. See the [IronBee Manual](https://www.ironbee.com/docs/manual/ironbee-reference-manual.html#N1011C).
 * details about the configuration rule language.
 *
 * @section LuaPerformance A Note About Performance
 *
 * As of IronBee 0.8.0, each connection is given a single Lua stack to work 
 * with. This means the Lua rules, and Lua modules all execute in the
 * same environment, and can use this environment to exchange information.
 *
 * This does mean that every connection pays a startup penalty in 0.8.0
 * to build the Lua stack. Future work will be to pool preallocated Lua stacks
 * and share them out. This will increase speed but will also require
 * the user to re-initialize any values. If you are coding Lua for IronBee
 * 0.8.0, you should clear all values you intend to use to make your code
 * future-compatible when shared Lua stacks are implemented.
 *
 * @section LuaModuleWriting Writing a Module in Lua
 *
 * Writing a module in Lua is an excellent way to quickly express complicated
 * security logic, prototype ideas, or simply protect a site that only
 * handles moderate traffic load. Modules are also the way to interface
 * with IronBee for purposes other than connections. For example, if you 
 * wanted to influence the engine at configuration time, there are
 * callbacks for configuration events. If you wanted to know
 * when IronBee's engine is going to cleanly shutdown, there is an event
 * that notifies listeners.
 *
 * Modules are single Lua files that are loaded onto the Lua stack
 * as anonymous functions, given a single argument, and evaluated.
 *
 * A simple module might be...
 *
 * @code{.lua}
 * -- Grab the module API instance.
 * local module = ...
 * module:logInfo("Loading module.")
 *
 * module:conn_opened_event(function(ib, event)
 *   ib:logInfo("Firing event %s.")
 *   return 0
 * end)
 *
 * module:logInfo("Done loading module!")
 * -- Tell the configuration system that we loaded correctly. Return IB_OK .
 * return 0
 * @endcode
 *
 *
 * The above module will log that a connection opened event is firing.
 * Notice that when building the module we use @c module whereas when
 * we log inside a callback we use @c ib. This is because @c ib is
 * an IronBee object which contains information specific to the connection
 * or transaction that is being handled at the time of the event
 * callback.
 *
 * The @c ib table is always an @ref IronBeeLuaEngineApi "engine" table.
 * But when in a transaction it will polymorphicaly specialize to a
 * @ref IronBeeLuaTxApi "tx" table and provide functions such as
 * @c addEvent.
 *
 * @code{.lua}
 * -- Grab the module API instance.
 * local module = ...
 *
 * module:tx_started_event(function(tx, event)
 *   tx:logDebug("Block all the things.")
 *   tx:addEvent("Block All Transactions!", { action = "block" })
 *   return 0
 * end)
 *
 * return 0
 * @endcode
 *
 * The above code is very similar to the previous code, but we've
 * changed the callback to @c tx_started_event so that the first
 * argument to our callback function is a @c tx, a child object of @c ib.
 *
 * We log, but at DEBUG level, that we are blocking everything. 
 * We do this by creating an event that has an action of "block".
 *
 * Moudules can also review created events and suppress them.
 *
 * @code{.lua}
 * -- Grab the module API instance.
 * local module = ...
 *
 * module:tx_finished_event(function(tx, event)
 *   for index, event in tx:events do
 *     event:setSuppress('false_positive')
 *   end
 *   return 0
 * end)
 *
 * return 0
 * @endcode
 *
 * The above code will suppress all events as @c false_positives created.
 *
 *
 * @section LuaRuleWriting Writing a Rule in Lua
 *
 * Choosing to write a Lua Rule involves a similar economy to that of
 * when to write a module. Performance is a cost, but the flexibility is
 * much greater. Perhaps this is a good way to prototype? Perhaps this is a 
 * good way to archive all data matching a particular rule?
 *
 * @subsection Lua Rules are Rules all the Same
 *
 * Lua Rules start in the IronBee configuration file:
 *
 * @code
 * RuleExt "lua:/home/myuser/myrule.lua" id:myrule rev:1 phase:RESPONSE event log action:block
 * @endcode
 *
 * Some things to observe. First, the directive is not Lua-specific.
 * @c RuleExt is for any supported external rule definition, for which 
 * there is only, currently, Lua.  Extending the external rule
 * languages is not covered in this guide, but is quite possible.
 * Second, notice, that this rule has no fields, no operator, and no
 * operator argument. It does, however, have a list of modifiers
 * that are executed if the Lua script should return 1, success.
 *
 * @c RuleExt statement are real rules, and so they can be chained. Perhaps
 * you would like to only use your Lua rule if there is a strong possibility
 * of it finding something. You could write:
 * 
 * @code
 * Rule ARGS @rx "my_check" id:myRule rev:1 phase:REQUEST chain
 * RuleExt "lua:/my_careful_check.lua"
 * @endcode
 *
 * When writing a Lua Rule realize that you are implementing the operator
 * portion of a rule. That operator returns 1 on success, 0 on failure,
 * but never errors. Also, it must fetch its fields as they are not provided.
 * Finally, since Lua Rules are implemented as operators, they have
 * no notion of the @c ib_rule_ext_t structure that an Action has 
 * available to it. 
 *
 * @subsection LuaRuleScript Inside the Rule Script
 *
 * We've shown how to wire a Lua script into the IronBee Rule Engine. Now
 * we will take a peak inside the @c .lua file.
 *
 * @code{.lua}
 * local ib = ...
 * ib:logInfo("In a rule.")
 * return 1
 * @endcode
 *
 * Lua rule files are loaded onto the Lua stack as anonymous functions,
 * and then stored for later retrieval. When they are run, they are
 * given a single argument, a table, which is an instance of the
 * @ref IronBeeLuaEngineApi "ib_engine" object. This Lua object
 * provides the Rule Writer with access to various functions to fetch
 * and set data fields and make a determination to fire the action set
 * associated with this rule, or not.
 *
 * @code{.lua}
 * local ib = ...
 *
 * local a = ib:get("ARGS:a")
 * if a and a == 'hello world' then
 *   ib:set("FOUND_A", 1)
 *   return 1
 * end
 * @endcode
 *
 * As a final example, the above rule will find a field @c a in the 
 * collection @c ARGS. If @c a is set to 'hello world', the modifier list is
 * fired.
 *
 * @section IronBeeLuaAPIReference IronBee Lua Api Reference
 *
 * The IronBee Lua api consists of several files. You should \c require
 * those that you need in your Lua code.
 *
 * - @c ironbee/config - Functions for the configuration DSL. Typically you
 *                       will not include this directly.
 * - @c ironbee/engine - An engine object. This is the API wrapper to
 *                       ib_engine_t pointers.
 * - @c ironbee/logevent - This class should not be required directly.
 *                         It contains the wrapper for ib_logevent_t
 *                         objects.
 * - @c ironbee/module - This is the module code. Like @c config.lua, you will
 *                       not typically use this directly.
 * - @c ironbee/rules - Another file that will not be immediately used
 *                      by user code.
 * - @c ironbee/tx - The transaction wrapper object. This provides access
 *                   to ib_tx_t objects.
 * - @c ironbee/util - A collection of utilty functions.
 * - @c ironbee/waggle - An alternate rule configuration language
 *                       which is exposed by @c config.lua.
 *
 * @subsection IronBeeLuaEngineApi The Engine API
 *
 * - @c ib:logError(msg, ...)
 * - @c ib:logWarn(msg, ...)
 * - @c ib:logInfo(msg, ...)
 * - @c ib:logDebug(msg, ...)
 *
 * @subsection IronBeeLuaTxApi The Transaction API
 *
 * - @c tx:addEvent([msg], options) - Add a new event.
 * - @c tx:appendToList(list_name, name, value) - append a value to a list.
 * - @c tx:get(name) - return a string, number or table.
 * - @c tx:getFieldList() - Return a list of defined fields.
 * - @c tx:getNames(field) - Returns a list of names in this field.
 * - @c tx:getValues(field) - Returns a list of values in this field.
 * - @c tx:set(name, value) - set a string, number or table.
 * - @c tx:forEachEvent(function(event)...) - Call the given function on each
 *                                            event.
 *                                            See Event Manipulation.
 * - @c tx:events() - Used to iterate over unsuppressed events.
 *                    Returns a next function, an empty table, and nil, used for
 *                    iteration. for index,event in ib:events() do ... end.
 * - @c tx:all_events() - Like events() but iterates over all events.
 *
 * @subsection IronBeeLuaLogEvents Event Manipulation
 *
 * An event object, such as one passed to a callback function by
 * forEachEvent is a special wrapper object.
 *
 * - @c event.raw - The raw C struct representing the current event.
 * - @c event:getSeverity() - Return the number representing the severity.
 * - @c event:getAction() - Return the integer representing the action.
 * - @c event:getConfidence() - Return the number representing the confidence.
 * - @c event:getRuleId() - Return the string representing the rule id.
 * - @c event:getMsg() - Return the string representing the message.
 * - @c event:getType() - Return the string showing the suppression value.
 *                        The returned values will be unknown,
 *                        observation, or alert
 *                        replaced, incomplete, partial, or other.
 * - @c event:setType(value) - Set the type value. This is one of the
 *                             very few values that may be changed in an event.
 *                             Events are mostly immutable things.
 *                             Allowed values are unknown, observation, or
 *                             alert.
 * - @c event:getSuppress() - Return the string showing the suppression value.
 *                            The returned values will be none, false_positive,
 *                            replaced, incomplete, partial, or other.
 * - @c event:setSuppress(value) - Set the suppression value. This is one of the
 *                                 very few values that may be changed in an
 *                                 event. Events are mostly immutable things.
 *                                 Allowed values are false_positive, replaced,
 *                                 incomplete, partial, or other.
 * - @c event:forEachField(function(tag)...) - Pass each field, as a string,
 *                                             to the callback function.
 * - @c event:forEachTag(function(tag)...) - Pass each tag, as a string, to
 *                                           the callback function.
 * - @c event:tags() - Tag iteration. for i, tagName in e:tags() do...
 * - @c event:fields() - Field iteration. for i, fieldName in e:fields() do...
 *
 * @subsection IronBeeLuaApiLogging Logging
 *
 * - @c ib:logError(format, ...) - Log an error message.
 * - @c ib:logInfo(format, ...) - Log an info message.
 * - @c ib:logDebug(format, ...) - Log a debug message.
 */

