--[[-------------------------------------------------------------------------
--]]-------------------------------------------------------------------------

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

-------------------------------------------------------------------
-- IronBee - Waggle
--
-- Waggle is a Domain Specific Language in Lua to describe IronBee rules.
--
-- The name, Waggle, refers to the dance that a bee will perform to
-- tell other bees that there is pollen to be had.
--
-- @module ironbee.waggle
--
-- @copyright Qualys, Inc., 2010-2015
-- @license Apache License, Version 2.0
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
-------------------------------------------------------------------

local M = {}
M.__index = M

-- Libraries required to build the basic API.
local SignatureDatabase = require('ironbee/waggle/signaturedatabase')
local Planner = require('ironbee/waggle/planner')
local Generator = require('ironbee/waggle/generator')
local Validator = require('ironbee/waggle/validator')

-- Put a default rule database in place.
M.DEFAULT_RULE_DB = SignatureDatabase:new()

-- Given a signature (Rule, Action, ExternalSignature, StreamSignature...)
-- and add a table named "meta" (not a Lua Meta Table) populated with
-- the source code name and line number.
local set_sig_meta = function(sig)
    local info = debug.getinfo(3, 'lSn')

    -- Do not do a normal get as it will return the action() proxy.
    if rawget(sig, 'meta') == nil then
        sig.meta = {}
    end

    sig.meta.source = info.short_src
    sig.meta.line = info.currentline
end

-- List of signature types so that these constructor functions
-- can be replaced.
M.SIGNATURE_TYPES = { "Rule", "Action", "RuleExt", "StrRule" }

-- Create a new, incomplete, signature (aka rule) representation
-- and register it with a global database of rules.
--
-- This also captures the line number and file that Rule is called on.
--
-- @param[in] self The module table use to construct a new sig.
-- @param[in] rule_id Rule ID.
-- @param[in] rule_version Rule version.
--
-- @returns nil on previously defined rule id.
M.Rule = function(self, rule_id, rule_version)

    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = M.DEFAULT_RULE_DB:Rule(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
M.Action = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = M.DEFAULT_RULE_DB:Action(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
M.Predicate = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = M.DEFAULT_RULE_DB:Predicate(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
M.RuleExt = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = M.DEFAULT_RULE_DB:RuleExt(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
M.StrRule = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = M.DEFAULT_RULE_DB:StrRule(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- Return a valid plan against the default database.
M.Plan = function()
    local p = Planner:new()
    local r = p:plan(M.DEFAULT_RULE_DB)
    if r == nil then
        return p.error_message
    else
        return r
    end
end

M.Generate = function()
    local g = Generator:new()
    return g:generate(M.Plan(), M.DEFAULT_RULE_DB)
end

M.GenerateJSON = function()
    local GeneratorJSON = require('ironbee/waggle/generatorjson')
    local g = GeneratorJSON:new()
    return g:generate(M.Plan(), M.DEFAULT_RULE_DB)
end

-- Load a set of rules from a JSON string.
-- @param[in] json The JSON string.
M.LoadJSON = function(self, json)
    local LoaderJSON = require('ironbee/waggle/loaderjson')

    -- Allow for calling M:LoadJSON or M.LoadJSON.
    if type(self) == 'string' then
        json = self
    end

    local l = LoaderJSON:new()

    return l:load(json, M.DEFAULT_RULE_DB)
end

-- Clear the default rule database.
M.clear_rule_db = function(self)
    M.DEFAULT_RULE_DB:clear()
end

-- Iterate over all tags.
M.all_tags = function(self)
    return M.DEFAULT_RULE_DB:all_tags()
end

-- Iterate over all IDSs.
M.all_ids = function(self)
    return M.DEFAULT_RULE_DB:all_ids()
end

-- Return a function the takes a list of signatures and builds a recipe.
-- For example,
--
--     Recipe '5' {
--       [[A signature.]],
--       Rule(...)...
--     }
--
-- Elements in the list that are stings are queued up as comments.
-- Elements that are table are treated as singature types and have
--   :tag(recipe_tag), :after(previous_id) and :comment(comment_text)
--   added to them.
M.Recipe = function(self, recipe_tag)
    if type(self) == 'string' then
        recipe_tag = self
    end

    return function(sigs)
        comment = ''
        prev_id = nil
        for i, element in ipairs(sigs) do
            -- A comment.
            if type(element) == 'string' then
                comment = comment .. element
            -- A signature.
            elseif type(element) == 'table' then

                -- Add comment.
                if #comment > 0 then
                   element:comment(comment)
                   comment = ''
                end

                -- Handle previous ID
                if prev_id then
                    element:after(prev_id)
                end
                prev_id = element.data.id

                -- Add recipe tag.
                element:tag(recipe_tag)
            end
        end
    end
end

-- Return the validator if there are any errors or warnings.
-- Returns the string "OK" otherwise.
M.Validate = function(self)
    local plan = M.Plan()
    local validator = Validator:new()
    if type(plan) == 'string' then
        error(plan)
    end

    validator:validate(M.DEFAULT_RULE_DB, plan)

    if validator:has_warnings() or validator:has_errors() then
        return validator
    else
        return 'OK'
    end
end


-- Aliases for backwards compatibility with 0.8.x. Remove.
M.Sig    = M.Rule
M.SigExt = M.RuleExt
M.StrSig = M.StrRule

return M
