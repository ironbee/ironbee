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
-- IronBee Waggle --- Base Rule
--
-- The base functionality used by all rule classes (Rule, Action, ExtRule,
-- StreamInspect, etc).
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
--

local BaseRule = { type = 'base_rule' }
function BaseRule:new(rule_id, rule_version, db)
    -- Allow child classes to use us as a meta table.
    self.__index = self

    -- Inherit from ourselves for the base calss.
    return setmetatable({
        -- The signature database. We need this to update tag information.
        db = db,
        -- The signature data. We hide the data in a data table
        -- because of name collisions between functions and data field names.
        data = {
            -- The rule ID.
            id = rule_id,

            -- The rule version.
            version = rule_version,

            -- What phase does this run in.
            phase = nil,

            -- Operator.
            op = '',

            -- Operator argument.
            op_arg = '',

            -- Message for when events are generated.
            message = nil,

            -- List of fields to operate on.
            fields = {},

            -- Hash of tags where the key is the tag name.
            tags = {},

            -- True if a predicate expression has been added to the actions.
            has_predicate = false,

            -- True if the rule engine should add fields such as FEILD_NAME to the vars.
            set_rule_meta_fields = false,

            -- True if this Rule will be owned by the Waggle execution
            -- engine. If false, then this rule will be be subject to
            -- Waggle rule injection and should be claimed by another
            -- module (such as predicate or fast).
            waggle_owned = true,

            -- List of actions.
            actions = {},

            -- After list. List of rules / tags this must occur after.
            after = {},
            before = {},

            -- Rule are of type Rule.
            rule_type = 'BaseRule',

            -- List of predicates rules which represent a list of disjuctions
            -- which, if all true, allow this signature to fire.
            --
            -- Predicate Rules are tables that contain a 'rule_id' field that
            -- points to a Rule object and a 'result' field which is
            -- true if the Predicate Rule must be true to continue
            -- or false if the Predicate Rule must be false for this
            -- rule to fire.
            follows = {}
        }
    }, self)
end

-- Report if this rule type is a stream rule or not.
function BaseRule:is_streaming()
    return false
end

-- Set if this Rule should be claimed by Waggle for rule injection.
--
-- Rule injection is a method by which a rule owner (such as Predicate,
-- Fast, or Waggle) can select a list of rules to execute at IronBee
-- runtime. A rule may not be owned by two rule injection modules.
-- If you specify a rule that you know will be claimed by another
-- injection module, you should set this to false so that Waggle
-- will not try and claim the produced rule.
function BaseRule:waggle_owned(true_false)
    self.data.waggle_owned = true_false
end

-- Set if meta fields for a rule should be set.
--
-- Meta fields are fields such as FIELD_NAME and FIELD_NAME_FULL.
-- This returns +self+ to allow chaining.
--
function BaseRule:set_rule_meta_fields()
    self.data.set_rule_meta_fields = true
    return self
end

-- Append a list of fields to this rule.
--
-- If the field list is empty, the rule's field list is cleared.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of fields.
--
-- @returns The signature/rule object.
function BaseRule:fields(...)
    local fields = {...}

    if #fields == 0 then
        self.data.fields = {}
    else
        -- Special case of stream fields. This sets the phase as a side effect.
        if #fields == 1 and
            (
               fields[1] == 'RESPONSE_HEADER_STREAM' or
               fields[1] == 'RESPONSE_BODY_STREAM'   or
               fields[1] == 'REQUEST_HEADER_STREAM'  or
               fields[1] == 'REQUEST_BODY_STREAM'
            )
        then
            self:phase(fields[1])
            local f = {
                original       = fields[1],
                collection     = fields[1],
                selector       = nil,
                transformation = nil
            }
            table.insert(self.data.fields, f)
        -- The normal case, the phase is not implicitly set.
        else
            for _, field in ipairs(fields) do

                local f = {
                    original       = field, -- raw, original string
                    collection     = nil,   -- the "source" in var terms.
                    selector       = nil,   -- The "filter" in var terms.
                    transformation = nil    -- Single transformation. This is wrong. Should be a list.
                }

                f.collection = string.match(field, "^([^:.]*)[:.]")

                if f.collection then
                    f.selector = string.match(field, ":([^.]*)")
                    f.transformation = string.match(field, "%.(.*)$")
                else
                    f.collection = field
                    f.selector = nil
                    f.transformation = nil
                end

                table.insert(self.data.fields, f)
            end
        end
    end

    return self
end
BaseRule.field = BaseRule.fields

-- Append a list of tags to this rule.
--
-- If the list of tags is empty then the rule's tag list is truncated.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of tags.
--
-- @returns The signature/rule object.
function BaseRule:tags(...)
    local tags = {...}

    if #tags == 0 then
        for tag, _ in pairs(self.data.tags) do
            self.db:untag(self, tag)
        end
        self.data.tags = {}
    else
        for _, tag in ipairs(tags) do
            self.data.tags[tag] = 1
            self.db:tag(self, tag)
        end
    end

    return self
end
BaseRule.tag = BaseRule.tags

-- Add predicate expression to this rule.
--
-- Only a single predicate expression is allowed per rule.
--
-- @param[in, out] self The signature/rule.
-- @param[in] expression String (sexpr) or front end expression object.
--
-- @returns The signature/rule object.
function BaseRule:predicate(expression)
  local sexpr

  if self.data.has_predicate then
    error("Rule can have at most one predicate expression.")
  end

  if #self.data.fields ~= 0 then
    error("Rule can have fields OR predicates, but not both.")
  end

  if self.data.op and self.data.op ~= "" then
    error("Rule can have an operator OR predicates, but not both: op=" .. tostring(self.data.op))
  end

  if type(expression) == 'string' then
    sexpr = expression
  else
    success, sexpr = pcall(expression)
    if not success then
      error("predicate argument failed to produce s-expression.")
      return self
    end
    if type(sexpr) ~= 'string' then
      error("predicate argument returned non-string.")
      return self
    end
  end

  self.data.has_predicate = true
  self:waggle_owned(false)
  table.insert(self.data.actions, {name = 'predicate', argument = sexpr})

  return self
end

-- Append a list of actions to this rule.
--
-- If the list of actions is empty then the rule's action list is truncated.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of actions.
--
-- @returns The signature/rule object.
function BaseRule:actions(...)
    local actions = {...}

    if #actions == 0 then
        self.data.actions = {}
    else
        for i,j in ipairs(actions) do
            local name, arg = string.match(j, "^([^:]+):?(.*)$")
            if arg == '' then
                arg = nil
            end
            table.insert(self.data.actions, {name = name, argument = arg} )
        end
    end

    return self
end
BaseRule.action = BaseRule.actions

-- Append a list of rule ids or tags that this rule must execute after.
--
-- If the list empty then the rule's list is cleared.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of strings representing rule IDs or tags.
--
-- @returns The signature/rule object.
function BaseRule:after(...)
    local after = {...}

    if #after == 0 then
        self.data.after = {}
    else
        for i,j in ipairs(after) do
            table.insert(self.data.after, j)
        end
    end

    return self
end

function BaseRule:before(...)
    local before = { ... }
    if #before == 0 then
        self.data.before = {}
    else
        for i,j in ipairs(before) do
            table.insert(self.data.before, j)
        end
    end

    return self
end


-- Set the rule operator and operator argument.
--
-- @param[in,out] self Rule/signature object.
-- @param[in] operator The operator. This should be the raw operator name,
--            such as "rx." This should *not* be "@rx" as the "@" prefix
--            is specific to the IronBee rule language.
-- @param[in] operator_argument The operator argument. If this is undefined
--            (nil) then "" is substituted.
--
-- @returns The rule/signature object.
function BaseRule:op(operator, operator_argument)
    self.data.op = operator

    if operator_argument == nil then
        self.data.op_arg = ""
    else
        self.data.op_arg = operator_argument
    end

    return self
end

-- Set the phase that this should run in.
-- @param[in,out] self The Rule object.
-- @param[in] phase The phase name.
-- @return Self.
function BaseRule:phase(phase)
    self.data.phase = phase
    return self
end

-- Set the message for an event.
--
-- @param[in,out] self Rule/signature object.
-- @param[in] message The message to set.
--
-- @returns The rule/signature object.
function BaseRule:message(message)
    self.data.message = message
    return self
end

-- Like message, but this tracks a comment from the rule writer.
-- It does not impact the rule execution.
--
-- @param[in,out] self Rule/signature object.
-- @param[in] comment The comment to set.
--
-- @returns The rule/signature object.
function BaseRule:comment(comment)
    self.data.comment = comment
    return self
end

-- Require rule to fire before this rule to conditionally execute it.
--
-- @param[in,out] self
-- @param[in] ruleId The rule ID to use.
-- @param[in] result The result of the ruleId when evaluated
--            at runtime (true or false) which is required to
--            execute this function.
function BaseRule:follows(ruleId, result)
    if type(ruleId) == 'table' then
        ruleId = ruleId.data.id
    end

    if result == nil then
        result = true
    end

    table.insert(self.data.follows, { rule = ruleId, result = result })

    return self
end

-- Check if this base rule has any metatable that equals the given table.
--
-- @param[in] clazz The class (object table) that defines self.
--
-- @returns true if self has any metatable that equals clazz.
function BaseRule:is_a(clazz)
    local mt = getmetatable(self)
    while mt ~= nil do
        if (mt == clazz) then
            return true
        else
            local mtmt = getmetatable(mt)
            -- If mt == mtmt, then there is no progress, and fail (false).
            if mt == mtmt then
                return false
            else
                mt = mtmt
            end
        end
    end

    return false
end
return BaseRule
