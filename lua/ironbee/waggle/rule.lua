#!/usr/bin/lua

--[[--------------------------------------------------------------------------
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
--]]--------------------------------------------------------------------------

--
-- IronBee Waggle --- Rule
--
-- Lua module file for building valid IronBee rule reprentations which 
-- can later be used to generate rule configurations for various IronBee
-- versions.
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
--

--
-- Rule DSL
-- ====================
--
-- Rule(id, ver) - create a new signature object.
--
-- Miscellaneous Functions
-- =======================
--

--[[--------------------------------------------------------------------------
-- Rule
--]]--------------------------------------------------------------------------
--
-- Class that the fluent internal DSL is built around.
--
local Util = require('ironbee/waggle/util')
local BaseRule = require('ironbee/waggle/base_rule')
local Rule = { type = "rule" }

-- Construct a new signature object.
--
-- @param[in] self The module table use to construct a new sig.
-- @param[in] rule_id Rule ID.
-- @param[in] rule_version Rule version.
-- @param[in] db Rule database.
--
-- @returns nil on previously defined rule id.
function Rule:new(rule_id, rule_version, db)
    self.__index = self
    setmetatable(self, BaseRule)

    local rule = BaseRule:new(rule_id, rule_version, db)

    rule.data.rule_type = 'Rule'

    return setmetatable(rule, self)
end

return Rule
