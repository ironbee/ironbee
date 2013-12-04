#!/usr/bin/lua

-- Lua module file for building valid IronBee rule reprentations which 
-- can later be used to generate rule configurations for various IronBee
-- versions.
--
-- Rule DSL
-- ====================
--
-- Sig(id, ver) - create a new signature object.
--
-- Miscellaneous Functions
-- =======================
--
--

-- ###########################################################################
-- Rule
-- ###########################################################################
--
-- Class that the fluent internal DSL is built around.
--
local Util = require('ironbee/waggle/util')
local Rule = {}
Rule.__index = Rule
Rule.type = "rule"

-- Construct a new signature object.
--
-- @param[in] self The module table use to construct a new sig.
-- @param[in] rule_id Rule ID.
-- @param[in] rule_version Rule version.
-- @param[in] db Rule database.
--
-- @returns nil on previously defined rule id.
Rule.new = function(self, rule_id, rule_version, db)
    local sig = setmetatable({
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
            op = '',
            op_arg = '',
            message = nil,
            -- List of fields to operate on.
            fields = {},
            -- Hash of tags where the key is the tag name.
            tags = { },
            -- True if a predicate expression has been added to the actions.
            has_predicate = false,
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
            rule_type = 'Rule',
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

    return sig
end
-- Append a list of fields to this rule.
--
-- If the field list is empty, the rule's field list is cleared.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of fields.
--
-- @returns The signature/rule object.
Rule.fields = function(self, ...)
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
                collection     = fields[1],
                selector       = nil,
                transformation = nil
            }
            table.insert(self.data.fields, f)
        -- The normal case, the phase is not implicitly set.
        else
            for _, field in ipairs(fields) do

                local f = {
                    collection     = nil,
                    selector       = nil,
                    transformation = nil
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
Rule.field = Rule.fields

-- Append a list of tags to this rule.
--
-- If the list of tags is empty then the rule's tag list is truncated.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of tags.
--
-- @returns The signature/rule object.
Rule.tags = function(self, ...)
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
Rule.tag = Rule.tags

-- Add predicate expression to this rule.
--
-- Only a single predicate expression is allowed per rule.
--
-- @param[in, out] self The signature/rule.
-- @param[in] expression String (sexpr) or front end expression object.
--
-- @returns The signature/rule object.
Rule.predicate = function(self, expression)
  local sexpr;

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
      error("predicate argument fatal to produce s-expression.")
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
Rule.actions = function(self, ...)
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
Rule.action = Rule.actions

-- Append a list of rule ids or tags that this rule must execute after.
--
-- If the list empty then the rule's list is cleared.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of strings representing rule IDs or tags.
--
-- @returns The signature/rule object.
Rule.after = function(self, ...)
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

Rule.before = function(self, ...)
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
Rule.op = function(self, operator, operator_argument)
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
Rule.phase = function(self, phase)
    self.data.phase = phase
    return self
end

-- Set the message for an event.
--
-- @param[in,out] self Rule/signature object.
-- @param[in] message The message to set.
--
-- @returns The rule/signature object.
Rule.message = function(self, message)
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
Rule.comment = function(self, comment)
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
Rule.follows = function(self, ruleId, result)
    if Util.type(ruleId) == 'signature' then
        ruleId = ruleId.data.id
    end

    if result == nil then
        result = true
    end

    table.insert(self.data.follows, { rule = ruleId, result = result })

    return self
end

-- Report if this rule type is a stream rule or not.
Rule.is_streaming = function(self)
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
Rule.waggle_owned = function(self, true_false)
    self.data.waggle_owned = true_false
end

return Rule
