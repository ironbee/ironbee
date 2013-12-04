--[[-------------------------------------------------------------------------
  Waggle is a Domain Specific Language in Lua to describe IronBee rules.

  The name, Waggle, refers to the dance that a bee will perform to 
  tell other bees that there is pollen to be had.
--]]-------------------------------------------------------------------------

local _M = {}
_M.__index = _M
_M._COPYRIGHT = "Copyright (C) 2010-2013 Qualys, Inc."
_M._DESCRIPTION = "IronBee Lua Rule Management"
_M._VERSION = "1.0"

-- Libraries required to build the basic API.
local SignatureDatabase = require('ironbee/waggle/signaturedatabase')
local Planner = require('ironbee/waggle/planner')
local Generator = require('ironbee/waggle/generator')
local Validator = require('ironbee/waggle/validator')

-- Put a default rule database in place.
_M.DEFAULT_RULE_DB = SignatureDatabase:new()

-- Given a signature (Rule, Action, ExternalSignature, StreamSignature...)
-- and add a table named "meta" (not a Lua Meta Table) populated with
-- the source code name and line number.
local set_sig_meta = function(sig)
    local info = debug.getinfo(3, 'lSn')

    if sig.meta == nil then
        sig.meta = {}
    end

    sig.meta.source = info.short_src
    sig.meta.line = info.currentline
end

-- List of signature types so that these constructor functions
-- can be replaced.
_M.SIGNATURE_TYPES = { "Rule", "Action", "RuleExt", "StrSig" }

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
_M.Rule = function(self, rule_id, rule_version)

    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = _M.DEFAULT_RULE_DB:Rule(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
_M.Action = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = _M.DEFAULT_RULE_DB:Action(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
_M.Predicate = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = _M.DEFAULT_RULE_DB:Predicate(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
_M.RuleExt = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = _M.DEFAULT_RULE_DB:RuleExt(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- See Rule.
_M.StrSig = function(self, rule_id, rule_version)
    if type(self) == 'string' then
        rule_version = rule_id
        rule_id = self
    end

    local sig = _M.DEFAULT_RULE_DB:StrSig(rule_id, rule_version)

    set_sig_meta(sig)

    return sig
end

-- Return a valid plan against the default database.
_M.Plan = function()
    local p = Planner:new()
    local r = p:plan(_M.DEFAULT_RULE_DB)
    if r == nil then
        return p.error_message
    else
        return r
    end
end

_M.Generate = function()
    local g = Generator:new()
    return g:generate(_M.Plan(), _M.DEFAULT_RULE_DB)
end

_M.GenerateJSON = function()
    local GeneratorJSON = require('ironbee/waggle/generatorjson')
    local g = GeneratorJSON:new()
    return g:generate(_M.Plan(), _M.DEFAULT_RULE_DB)
end

-- Load a set of rules from a JSON string.
-- @param[in] json The JSON string.
_M.LoadJSON = function(self, json)
    local LoaderJSON = require('ironbee/waggle/loaderjson')

    -- Allow for calling _M:LoadJSON or _M.LoadJSON.
    if type(self) == 'string' then
        json = self
    end

    local l = LoaderJSON:new()

    return l:load(json, _M.DEFAULT_RULE_DB)
end

-- Clear the default rule database.
_M.clear_rule_db = function(self)
    _M.DEFAULT_RULE_DB:clear()
end

-- Iterate over all tags.
_M.all_tags = function(self)
    return _M.DEFAULT_RULE_DB:all_tags()
end

-- Iterate over all IDSs.
_M.all_ids = function(self)
    return _M.DEFAULT_RULE_DB:all_ids()
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
_M.Recipe = function(self, recipe_tag)
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
_M.Validate = function(self)
    local plan = _M.Plan()
    local validator = Validator:new()
    if type(plan) == 'string' then
        error(plan)
    end

    validator:validate(_M.DEFAULT_RULE_DB, plan)

    if validator:has_warnings() or validator:has_errors() then
        return validator
    else
        return 'OK'
    end
end

return _M
