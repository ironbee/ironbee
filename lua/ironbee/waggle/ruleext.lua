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

local BaseRule = require('ironbee/waggle/base_rule')

-- ###########################################################################
-- RuleExt - A rule that is an outside script.
-- ###########################################################################
local RuleExt = { type = "ruleext" }

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
function RuleExt:new(id, rev, db)
    self.__index = self
    setmetatable(self, BaseRule)

    local es = BaseRule:new(id, rev, db)
    es.data.rule_type = 'RuleExt'

    return setmetatable(es, self)
end

-- Set the script of this external rule.
function RuleExt:script(script)
    self.data.script = script
    return self
end

return RuleExt
