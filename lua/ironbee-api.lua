-- =========================================================================
-- Licensed to Qualys, Inc. (QUALYS) under one or more
-- contributor license agreements.  See the NOTICE file distributed with
-- this work for additional information regarding copyright ownership.
-- QUALYS licenses this file to You under the Apache License, Version 2.0
-- (the "License"); you may not use this file except in compliance with
-- the License.  You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--
-- =========================================================================
--
-- Author: Sam Baskinger <sbaskinger@qualys.com>
--
-- =========================================================================
--
-- Public API Documentation
--
--
-- Data Access
--
-- add(name, value) - add a string, number or table.
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

-- Lua 5.2 and later style class.
_M = {}
_M.__index = _M
_M._COPYRIGHT = "Copyright (C) 2010-2013 Qualys, Inc."
_M._DESCRIPTION = "IronBee Lua API"
_M._VERSION = "1.0"

-- Expose the Engine object.
_M.engineapi = require('ironbee/engine')

-- Expose the Tx object. This inherits from Engine.
_M.txapi = require('ironbee/tx')

-- Expose the Rule object. This inherits from Tx.
_M.ruleapi = require('ironbee/rules')

return _M
