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
 * @brief IronBee Lua --- Top level API documentation.
 *
 * This file contains no code, only API documentations.  It functions as the
 * main page of the API documentation.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 **/

/**
 * @page LuaAPI Lua API
 *
 * @tableofcontents
 *
 * @section IronBeeLuaAPI IronBee Lua Api
 *
 * The IronBee Lua api consists of several files. You should \c require
 * those that you need in your Lua code.
 *
 * - @c ironbee/config - Functions for the configuration DSL. Typically you
 *                       will not include this directly.
 * - @c ironbee/engine - An engine object. This is the API wrapper to
 *                       ib_engine_t pointers.
 * - @c ironbee/module - This is the module code. Like @c config.lua, you will
 *                       not typically use this directly.
 * - @c ironbee/rules - Another file that will not be immediately used
 *                      by user code.
 * - @c ironbee/tx - The transaction wrapper object. This provides access
 *                   to ib_tx_t objects.
 * - @c ironbee/util - A collection of utilty functions.
 * - @c ironbee/waggle - An alternate rule configuration language
 *                       which is exposed by @c config.lua. This
 *                       copy of the Waggle code removes support for
 *                       JSON parsing and generation.
 *
 * @subsection IronBeeLuaEngineApi The Engine API
 *
 * @fn add(name, value)
 * @param[in] name The name of the value to add to the DPI.
 * @param[in] value The value to add to the DPI.
 * Add a string, number or table.
 *
-- addEvent([msg], options) - Add a new event.
-- appendToList(list_name, name, value) - append a value to a list.
-- get(name) - return a string, number or table.
-- getFieldList() - Return a list of defined fields.
-- getNames(field) - Returns a list of names in this field.
-- getValues(field) - Returns a list of values in this field.
-- set(name, value) - set a string, number or table.
-- forEachEvent(function(event)...) - Call the given function on each event.
--                                    See the Event Manipulation section.
-- events() - Returns a next function, an empty table, and nil, used for
--            iteration. for index,event in ib:events() do ... end.
--
-- Event Manipulation
-- An event object, such as one passed to a callback function by
-- forEachEvent is a special wrapper object.
--
-- event.raw - The raw C struct representing the current event.
-- event:getSeverity() - Return the number representing the severity.
-- event:getAction() - Return the integer representing the action.
-- event:getConfidence() - Return the number representing the confidence.
-- event:getRuleId() - Return the string representing the rule id.
-- event:getMsg() - Return the string representing the message.
-- event:getType() - Return the string showing the suppression value.
--                       The returned values will be unknown,
--                       observation, or alert
--                       replaced, incomplete, partial, or other.
-- event:setType(value) - Set the type value. This is one of the
--                        very few values that may be changed in an event.
--                        Events are mostly immutable things.
--                        Allowed values are unknown, observation, or alert.
-- event:getSuppress() - Return the string showing the suppression value.
--                       The returned values will be none, false_positive,
--                       replaced, incomplete, partial, or other.
-- event:setSuppress(value) - Set the suppression value. This is one of the
--                            very few values that may be changed in an event.
--                            Events are mostly immutable things.
--                            Allowed values are false_positive, replaced,
--                            incomplete, partial, or other.
-- event:forEachField(function(tag)...) - Pass each field, as a string, to the callback function.
-- event:forEachTag(function(tag)...) - Pass each tag, as a string, to the callback function.
--
-- Logging
--
-- logError(format, ...) - Log an error message.
-- logInfo(format, ...) - Log an info message.
-- logDebug(format, ...)- Log a debug message.
-- 
 * @section Rules
 *
 * A section about rules.
 */

