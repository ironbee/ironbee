#!/usr/bin/lua

-- Lua module file for building valid IronBee rule reprentations which 
-- can later be used to generate rule configurations for various IronBee
-- versions.
--
-- Signature / Rule DSL
-- ====================
--
-- Sig(id, ver) - create a new signature object.
--
-- Miscellaneous Functions
-- =======================
--
--

-- ###########################################################################
-- Signature
-- ###########################################################################
--
-- Class that the fluent internal DSL is built around.
--
local Util = require('ironbee/waggle/util')
local Signature = {}
Signature.__index = Signature
Signature.type = "signature"

-- Construct a new signature object.
--
-- @param[in] self The module table use to construct a new sig.
-- @param[in] rule_id Rule ID.
-- @param[in] rule_version Rule version.
-- @param[in] db Signature database.
--
-- @returns nil on previously defined rule id.
Signature.new = function(self, rule_id, rule_version, db)
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
            -- List of tags.
            tags = {},
            -- List of actions.
            actions = {},
            -- After list. List of rules / tags this must occur after.
            after = {},
            before = {},
            -- Signatures are of type Rule.
            rule_type = 'Rule',
            -- List of predicates rules which represent a list of disjuctions
            -- which, if all true, allow this signature to fire. 
            --
            -- Predicate Rules are tables that contain a 'rule_id' field that
            -- points to a Signature object and a 'result' field which is
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
Signature.fields = function(self, ...)
    local fields = {...}

    if #fields == 0 then
        self.data.fields = {}
    else
        for _, field in ipairs(fields) do

            local f = { collection = nil, selector = nil, transformation = nil }

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

    return self
end
Signature.field = Signature.fields

-- Append a list of tags to this rule.
--
-- If the list of tags is empty then the rule's tag list is truncated.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of tags.
--
-- @returns The signature/rule object.
Signature.tags = function(self, ...)
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
Signature.tag = Signature.tags

-- Append a list of actions to this rule.
--
-- If the list of actions is empty then the rule's action list is truncated.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of actions.
--
-- @returns The signature/rule object.
Signature.actions = function(self, ...)
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
Signature.action = Signature.actions

-- Append a list of rule ids or tags that this rule must execute after.
--
-- If the list empty then the rule's list is cleared.
--
-- @param[in,out] self The signature/rule.
-- @param[in] ... The list of strings representing rule IDs or tags.
--
-- @returns The signature/rule object.
Signature.after = function(self, ...)
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

Signature.before = function(self, ...)
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
Signature.op = function(self, operator, operator_argument)
    self.data.op = operator

    if operator_argument == nil then
        self.data.op_arg = ""
    else
        self.data.op_arg = operator_argument
    end

    return self
end

-- Set the phase that this should run in.
-- @param[in,out] self The Signature object.
-- @param[in] phase The phase name.
-- @return Self.
Signature.phase = function(self, phase)
    self.data.phase = phase
    return self
end

-- Set the message for an event.
--
-- @param[in,out] self Rule/signature object.
-- @param[in] message The message to set.
--
-- @returns The rule/signature object.
Signature.message = function(self, message)
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
Signature.comment = function(self, comment)
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
Signature.follows = function(self, ruleId, result)
    if Util.type(ruleId) == 'signature' then
        ruleId = ruleId.data.id
    end

    if result == nil then
        result = true
    end

    table.insert(self.data.follows, { rule = ruleId, result = result })

    return self
end

return Signature
