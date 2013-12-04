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
-- IronBee Waggle --- Signature Database
--
-- Organizes Lua representations of rules.
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
local Util = require('ironbee/waggle/util')
local Rule = require('ironbee/waggle/rule')
local ActionRule = require('ironbee/waggle/actionrule')
local StreamInspect = require('ironbee/waggle/streaminspect')
local Predicate = require('ironbee/waggle/predicaterule')
local RuleExt = require('ironbee/waggle/ruleext')

-- ###########################################################################
-- SignatureDatabase
-- ###########################################################################
--
SignatureDatabase = {}
SignatureDatabase.__index = SignatureDatabase
SignatureDatabase.type = "signaturedatabase"
SignatureDatabase.clear = function(self)
    self.db = {}
    self.tag_db = {}
end
SignatureDatabase.Rule = function(self, rule_id, rule_version)

    if self.db[rule_id] ~= nil then
        error(
        {
            sig_id = rule_id,
            sig_rev = rule_version,
            msg = string.format("Cannot redefine signature/rule %s:%s.", rule_id, rule_version)
        }, 1)
    end

    local sig = Rule:new(rule_id, rule_version, self)

    self.db[rule_id] = sig

    return sig
end

SignatureDatabase.StrSig = function(self, rule_id, rule_version)

    if self.db[rule_id] ~= nil then
        error(
        {
            sig_id = rule_id,
            sig_rev = rule_version,
            msg = string.format("Cannot redefine streaming signature/rule %s:%s.", rule_id, rule_version)
        }, 1)
    end

    local sig = StreamInspect:new(rule_id, rule_version, self)

    self.db[rule_id] = sig

    return sig
end

SignatureDatabase.RuleExt = function(self, rule_id, rule_version)

    if self.db[rule_id] ~= nil then
        error(
        {
            sig_id = rule_id,
            sig_rev = rule_version,
            msg = string.format("Cannot redefine ext signature/rule %s:%s.", rule_id, rule_version)
        }, 1)
    end

    local sig = RuleExt:new(rule_id, rule_version, self)

    self.db[rule_id] = sig

    return sig
end

SignatureDatabase.Action = function(self, rule_id, rule_version)

    if self.db[rule_id] ~= nil then
        error(
        {
            sig_id = rule_id,
            sig_rev = rule_version,
            msg = string.format("Cannot redefine ext signature/rule %s:%s.", rule_id, rule_version)
        }, 1)
    end

    local sig = ActionRule:new(rule_id, rule_version, self)

    self.db[rule_id] = sig

    return sig
end

SignatureDatabase.Predicate = function(self, rule_id, rule_version)

    if self.db[rule_id] ~= nil then
        error(
        {
            sig_id = rule_id,
            sig_rev = rule_version,
            msg = string.format("Cannot redefine predicate signature/rule %s:%s.", rule_id, rule_version)
        }, 1)
    end

    local sig = Predicate:new(rule_id, rule_version, self)

    self.db[rule_id] = sig

    return sig
end


SignatureDatabase.new = function(self)
    return setmetatable({
        -- Table of rules indexed by ID.
        db = {},
        -- Table of rules indexed by the tags they have.
        tag_db = {}
    }, self)
end

SignatureDatabase.sigs_by_tag = function(self, tag)
    local sigs = self.tag_db[tag]

    if sigs == nil then
        sigs = {}
    end

    return sigs
end

SignatureDatabase.tag = function(self, rule, tag)
    if self.tag_db[tag] == nil then
        self.tag_db[tag] = {}
    end

    self.tag_db[tag][rule.data.id] = rule
end

SignatureDatabase.untag = function(self, rule, tag)
    self.tag_db[tag][rule.data.id] = nil
end

-- Returns an iterator function to iterate over each tag.
SignatureDatabase.all_tags = function(self)
    return pairs(self.tag_db)
end

-- Returns an iterator function to iterate over each id.
SignatureDatabase.all_ids = function(self)
    return pairs(self.db)
end

return SignatureDatabase
