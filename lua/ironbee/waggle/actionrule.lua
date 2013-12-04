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
-- IronBee Waggle --- Action Rule
--
-- The base functionality used by all rule classes (Rule, Action, ExtRule, 
-- StreamInspect, etc).
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
--

local BaseRule = require('ironbee/waggle/base_rule')

-- ###########################################################################
-- ExternalSignature - A signature (rule) that is an outside script.
-- ###########################################################################
local ActionRule = { type = "actionrule" }

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
ActionRule.new = function(self, id, rev, db)
    self.__index = self
    setmetatable(self, BaseRule)

    local rule = BaseRule:new(id, rev, db)

    rule.data.rule_type = 'Action'

    return setmetatable(rule, self)
end

-- Set the script of this external rule.
ActionRule.script = function(self, script)
    self.data.script = script
    return self
end

return ActionRule
