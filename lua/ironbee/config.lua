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
-- IronBee - Config
--
-- IronBee configuration.
--
-- @module ironbee.config
--
-- @copyright Qualys, Inc., 2010-2014
-- @license Apache License, Version 2.0
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
-------------------------------------------------------------------

local ffi       = require('ffi')

local Action    = require('ironbee/waggle/actionrule')
local Waggle    = require('ironbee/waggle')
local Predicate = require('ironbee/predicate')

local M        = {}
M.__index      = M

-- Pair of #defines from C code imported here.
local IB_RULEMD_FLAG_EXPAND_MSG = 1
local IB_RULEMD_FLAG_EXPAND_DATA = 2

-- (IB_RULE_FLAG_NO_TGT) or (256)
local IB_RULE_FLAG_ACTION = 256

-- Import IB_RULE_FLAG_FIELDS flag value.
local IB_RULE_FLAG_FIELDS = math.pow(2, 9)

-- Setup the configuration DLS, run the function provided, tear down the DSL.
--
-- @param[in] f Function to run after the DSL is installed in _G.
-- @param[in] cp Configuration parser.
local DoInDSL = function(f, cp)
    local ib = ibapi.engineapi:new(ffi.cast("ib_cfgparser_t*", cp).ib)

    local SkipTable = {
        __index = 1,
    }

    local to_nil = {}

    -- Setup all symbols.
    for k,v in pairs(Waggle) do
        if not SkipTable[k] then
            _G[k] = v
            table.insert(to_nil, k)
        end
    end
    -- Currently the Predicate naming convention is to ucfirst() and camel
    -- case functions that are sexpr based and to lowercase utility
    -- functions.  This is a bit unclear and it is planned to split Predicate
    -- namespaces into P and PUtil.  For now, create a wrapper to alias
    -- the functions and generate deprecation warnings to allow for this
    -- transition to occur before the namespace split.
    --
    -- TODO Remove this when P/PUtil is split.
    --
    _G['P'] = { }
    local p_mt = {
        __index = Predicate
    }
    setmetatable(_G['P'], p_mt)
    _G['PUtil'] = { }
    local putil_mt = {
        __index = Predicate.Util
    }
    setmetatable(_G['PUtil'], putil_mt)

    _G['IB'] = ib
    _G['CP'] = cp
    local succeeded, error_obj = pcall(f)

    if not succeeded then
        if 'table' == type(error_obj) then
            ib:logError(
                "Failed to eval Lua DSL for rule %s rev %d: %s",
                error_obj.sig_id,
                error_obj.sig_rev,
                error_obj.msg)
            error("Failed to eval Lua DSL. See preceeding message.")
        elseif 'string' == type(error_obj) then
            ib:logError("Failure evaluating Lua DLS: %s", error_obj)
            error("Failed to eval Lua DSL. See preceeding message.")
        else
            error("Unknown error running Lua configuration DSL.")
        end
    end

    -- Teardown.
    _G['P'] = nil
    _G['PUtil'] = nil
    _G['IB'] = nil
    _G['CP'] = nil
    for _, k in ipairs(to_nil) do
        _G[k] = nil
    end
end

---
-------------------------------------------------------------------
-- Include a configuration file.
--
-- Rules are not committed to the engine until the end of the
-- configuration phase.
--
-- @param[in] cp Configuraiton Parser.
-- @param[in] file String point to the file to operate on.
-------------------------------------------------------------------
M.include = function(cp, file)
    local ib = ibapi.engineapi:new(ffi.cast("ib_cfgparser_t*", cp).ib)

    DoInDSL(function()
        ib:logInfo("Including lua file %s", file)
        dofile(file)
    end, cp)

    return ffi.C.IB_OK
end

-- Add fields to a rule.
--
-- @param[in] ib IronBee engine.
-- @param[in] rule Lua rule table.
-- @param[in] prule An ib_rule_t*[1].
-- @param[in] field Lua field table in rule.data.fields.
local add_fields = function(ib, rule, prule, field)
    local name = field.collection
    local not_found = ffi.new("int[1]")
    local target_name = ffi.new("char*[1]")
    local target = ffi.new("ib_rule_target_t*[1]")
    local tfn_names = ffi.new("ib_list_t*[1]")
    local tfn_insts = ffi.new("ib_list_t*[1]")
    local mm = ffi.C.ib_engine_mm_main_get(ib.ib_engine)
    local rc

    rc = ffi.C.ib_list_create(tfn_names, mm)
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to create transformation field list.")
        return rc
    end

    rc = ffi.C.ib_list_create(tfn_insts, mm)
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to create transformation instance list.")
        return rc
    end

    rc = ffi.C.ib_cfg_parse_target_string(
        mm,
        field.original,
        ffi.cast('const char **', target_name),
        tfn_names[0]);
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to parse target string: %s", field.original)
        return rc
    end

    rc = ffi.C.ib_rule_tfn_fields_to_inst(
        ib.ib_engine,
        mm,
        tfn_names[0],
        tfn_insts[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to map transformation fields to instances: %s", field.original)
        return rc
    end

    rc = ffi.C.ib_rule_create_target(
        ib.ib_engine,
        target_name[0],
        tfn_insts[0],
        target)
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to create target %s.", field.original)
        return rc
    end

    -- Add target.
    rc = ffi.C.ib_rule_add_target(
        ib.ib_engine,
        prule[0],
        target[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to add target %s to rule.", field.original)
        return rc
    end

    return ffi.C.IB_OK
end

-- Called by build_rule to add actions to rules.
local add_action_to_rule = function(
    ib,
    name,
    arg,
    rule
)
    local rc

    -- Detect inverted actions (actions starting with !)
    local is_inverted

    if string.sub(name, 1, 1) == '!' then
        name = string.sub(name, 2)

        -- fire false actions.
        is_inverted = ffi.C.IB_RULE_ACTION_FALSE
    else
        -- fire true actions.
        is_inverted = ffi.C.IB_RULE_ACTION_TRUE
    end

    -- Create the action instance.
    local action = ffi.new("const ib_action_t*[1]")
    rc = ffi.C.ib_action_lookup(
      ib.ib_engine,
      name, string.len(name),
      action
    )
    if rc ~= ffi.C.IB_OK then
      ib:logError(
          "Failed to find action %s for rule.", name)
      return rc
    end
    local action_inst = ffi.new("ib_action_inst_t*[1]")
    rc = ffi.C.ib_action_inst_create(
        action_inst,
        ffi.C.ib_engine_mm_main_get(ib.ib_engine),
        rule.ctx,
        action[0],
        arg)
    if rc ~= ffi.C.IB_OK then
        ib:logError(
            "Failed to create action %s instance for rule.", name)
        return rc
    end

    -- Add the action instance.
    rc = ffi.C.ib_rule_add_action(
        ib.ib_engine,
        rule,
        action_inst[0],
        is_inverted)
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to add action instance \"%s\" to rule.", action)
        return rc
    end

    return ffi.C.IB_OK
end

-- Called by build_rule to add the operator to a rule.
--
-- @param[in] ib IronBee engine.
-- @param[in] ctx Configuration context.
-- @param[in] rule The lua rule structure.
-- @param[in] prule the C rule pointer.
--
-- @return
-- - IB_OK On success.
-- - Other on failure.
local add_operator = function(
    ib,
    ctx,
    rule,
    prule)

    local rc

    -- Create operator instance.
    local opinst = ffi.new("ib_operator_inst_t*[1]")
    local op_inst_create_flags = 0
    local op = ffi.new("ib_operator_t*[1]")

    local opname

    -- Get the operator instance. If not defined, nop is used.
    if rule.data.op == nil or #rule.data.op == 0 then
        opname = "nop"
    elseif string.sub(rule.data.op, 1, 1) == '!' then
        opname = string.sub(rule.data.op, 2)
    else
        opname = rule.data.op
    end

    if rule.is_streaming() then
        rc = ffi.C.ib_operator_stream_lookup(
            ib.ib_engine,
            opname, string.len(opname),
            ffi.cast("const ib_operator_t**", op))
    else
        rc = ffi.C.ib_operator_lookup(
            ib.ib_engine,
            opname, string.len(opname),
            ffi.cast("const ib_operator_t**", op))
    end
    if rc ~= ffi.C.IB_OK then
        ib:logError("Could not locate operator %s", opname)
        return rc
    end

    -- C operator parameter copy. This is passed to the operater inst constructor and set in the rule.
    local cop_params = ffi.C.ib_mm_strdup(
        ffi.C.ib_engine_mm_main_get(ib.ib_engine),
        tostring(rule.data.op_arg))

    -- Create the operator.
    rc = ffi.C.ib_operator_inst_create(
        opinst,
        ffi.C.ib_engine_mm_main_get(ib.ib_engine),
        ctx,
        op[0],
        op_inst_create_flags,
        cop_params)
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to create operator instance for %s.", op)
        return rc
    end

    -- Set operator
    rc = ffi.C.ib_rule_set_operator(
        ib.ib_engine,
        prule[0],
        opinst[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to set rule operator.")
        return rc
    end

    -- Copy the parameters used to construct the operator instance into the rule for logging.
    ib:logDebug("Setting rule op inst params: %s", ffi.string(cop_params));
    rc = ffi.C.ib_rule_set_op_params(prule[0], cop_params);
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to copy params %s.", ffi.string(cop_params))
        return rc
    end

    -- Invert the operator.
    if string.sub(rule.data.op, 1, 1) == '!' then
        rc = ffi.C.ib_rule_set_invert(prule[0], 1)
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to invert operator %s.", op)
            return rc
        end
    end

    return ffi.C.IB_OK
end

-- Add actions from the lua rule to the c rule pointer.
--
-- @param[in] ib IronBee engine.
-- @param[in] ctx Configuration context.
-- @param[in] rule The lua rule structure.
-- @param[in] prule the C rule pointer.
--
-- @return
-- - IB_OK On success.
-- - Other on failure.
local add_actions = function(
    ib,
    ctx,
    rule,
    prule)

    local rc

    for _, action in ipairs(rule.data.actions) do
        local name, arg = action.name, action.argument

        if name == "logdata" then
          local expand = ffi.new("ib_var_expand_t*[1]")
          rc = ffi.C.ib_var_expand_acquire(
              expand,
              ffi.C.ib_engine_mm_main_get(ib.ib_engine),
              arg,
              #arg,
              ffi.C.ib_engine_var_config_get(ib.ib_engine)
          )
          if rc ~= ffi.C.IB_OK then
              ib:loggError("Failed to acquire rule data expand.")
          else
              prule[0].meta.data = expand[0]
          end
        elseif name == "severity" then
            local severity = tonumber(arg)
            if severity > 255 then
                ib:logError("Severity exceeds max value: %d", severity)
            elseif severity < 0 then
                ib:logError("Severity is less than 0: %d", severity)
            else
                prule[0].meta.severity = severity
            end
        elseif name == "confidence" then
            local confidence = tonumber(arg)
            if confidence > 255 then
                ib:logError("Confidence exceeds max value: %d", confidence)
            elseif confidence < 0 then
                ib:logError("Confidence is less than 0: %d", severity)
            else
                prule[0].meta.confidence = confidence
            end
        elseif name == 'capture' then
            rc = ffi.C.ib_rule_set_capture(ib.ib_engine, prule[0], arg)
            if rc ~= ffi.C.IB_OK then
                ib:loggerError("Failed to set capture value on rule.")
            end
        elseif name == 't' then
            if ffi.C.ib_rule_allow_tfns(prule[0]) then
                rc = ffi.C.ib_rule_add_tfn(ib.ib_engine, prule[0], arg)
                if rc == ffi.C.IB_ENOENT then
                    ib:logError("Unknown transformation: \"%s\".", arg)
                elseif rc ~= ffi.C.IB_OK then
                    ib:logError("Error adding transformation \"%s\".", arg)
                end
            else
                ib:logError("Transformations not supported for this rule.")
            end
        -- Handling of Actions
        else
            rc = add_action_to_rule(ib, name, arg, prule[0])
            if rc ~= ffi.C.IB_OK then
                return rc
            end
        end
    end

    -- While we're building up actions, add the wagle action
    -- which flags that this rule is subject to Waggle rule injection.
    if rule.data.waggle_owned then
        -- Non-predicate rules should be claimed by Waggle.
        rc = add_action_to_rule(ib, "waggle", "", prule[0])
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to add wagle action to rule.")
            return rc
        end
    end


    return ffi.C.IB_OK
end

-- Create, setup, and register a rule in the given ironbee engine.
--
-- @param[in] ib IronBee Engine.
-- @param[in] ctx The context the rules should be added to.
-- @param[in] chain A list of rule-result pairs.
--            Each element in this list is a table with
--            "rule" being the rule id and "result" being the
--            result required for the rule to allow the next rule
--            in the chain to fire.
--            The result is irrelevant in the last rule in the rule chain.
-- @param[in] db The database that contains the full rule definition.
local build_rule = function(ib, ctx, chain, db)
    for i, link in ipairs(chain) do
        local rule_id = link.rule
        local result = link.result
        local rule = db.db[rule_id]
        local prule = ffi.new("ib_rule_t*[1]")
        local rc

        rc = ffi.C.ib_rule_create(
            ib.ib_engine,
            ctx,
            rule.meta.source,
            rule.meta.line,
            rule.is_streaming(),
            prule)
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to create rule.")
            return rc
        end

        -- For actions, set the magic actionflag.
        if rule:is_a(Action) then
            prule[0].flags = ffi.C.ib_set_flag(
                prule[0].flags,
                IB_RULE_FLAG_ACTION)
        end

        if rule.data.set_rule_meta_fields then
            -- Tell the rule engine to populate the FIELD_NAME, FIELD_NAME_FULL,
            -- and related fields.
            prule[0].flags = ffi.C.ib_set_flag(prule[0].flags, IB_RULE_FLAG_FIELDS);
        end

        -- Add operator to the rule.
        rc = add_operator(ib, ctx, rule, prule)
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to add operator to rule.")
            return rc
        end

        rc = add_actions(ib, ctx, rule, prule)
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to add actions to rule.")
            return rc
        end

        -- Set tags
        for tag, _ in pairs(rule.data.tags) do
            local tagcpy =
                ffi.C.ib_mm_strdup(
                    ffi.C.ib_engine_mm_main_get(ib.ib_engine),
                    tostring(tag))
            ib:logDebug("Setting tag %s on rule.", tag)
            rc = ffi.C.ib_list_push(prule[0].meta.tags, tagcpy)
            if rc ~= ffi.C.IB_OK then
                ib:logError("Setting tag %s failed.", tag)
            end
        end

        -- Set message
        if rule.data.message then

            -- Set the message.
            local expand = ffi.new("ib_var_expand_t*[1]")
            rc = ffi.C.ib_var_expand_acquire(
                expand,
                ffi.C.ib_engine_mm_main_get(ib.ib_engine),
                rule.data.message,
                #rule.data.message,
                ffi.C.ib_engine_var_config_get(ib.ib_engine)
            )
            if rc ~= ffi.C.IB_OK then
                ib:loggError("Failed to acquire rule msg expand.")
            else
                prule[0].meta.msg = expand[0]
            end

        end

        -- If this rule is a member of a chain.
        if i < #chain then
            rc = ffi.C.ib_rule_set_chain(ib.ib_engine, prule[0])
            if rc ~= ffi.C.IB_OK then
                ib:logError("Failed to setup chain rule.")
            end
        end

        if rule.data.has_predicate then
            -- Predicates do not have targets
            prule[0].flags = ffi.C.ib_set_flag(
                prule[0].flags,
                IB_RULE_FLAG_ACTION)
        else
            for _, field in ipairs(rule.data.fields) do
                add_fields(ib, rule, prule, field)
            end
        end

        -- Find out of this is streaming or not, and treat that as an int.
        local is_streaming
        if rule.is_streaming() then
            is_streaming = 1
        else
            is_streaming = 0
        end

        -- If this rule is the first rule, it carries the id, rev, and phase
        -- for the chain of rules to follow. Set those values to the
        -- values in the last rule in the chain. Notice that rules
        -- that are in chains of length=1 this sets their id, rev, and phaes
        -- correctly.
        if i == 1 then
            -- Get last rule in the chain.
            local last_rule = db.db[chain[#chain].rule]

            -- Set id.
            ffi.C.ib_rule_set_id(
                ib.ib_engine,
                prule[0],
                ffi.C.ib_mm_strdup(
                    ffi.C.ib_engine_mm_main_get(ib.ib_engine),
                    tostring(last_rule.data.id)))

            -- Set rev.
            prule[0].meta.revision = tonumber(last_rule.data.version) or 1

            -- Lookup and set phase.
            rc = ffi.C.ib_rule_set_phase(
                ib.ib_engine,
                prule[0],
                ffi.C.ib_rule_lookup_phase(last_rule.data.phase, is_streaming))
            if rc ~= ffi.C.IB_OK then
                ib:logError("Cannot set phase %s", last_rule.data.phase)
            end
        end

        rc = ffi.C.ib_rule_register(ib.ib_engine, ctx, prule[0])
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to register rule.")
            return rc
        end

    end

    return ffi.C.IB_OK
end

-------------------------------------------------------------------
-- Build and add all rules configured to the engine.
--
-- @tparam cdata[ib_engine_t*] ib_engine IronBee engine ib_engine_t*.
--
-- @return Status code.
-------------------------------------------------------------------
M.build_rules = function(ib_engine)
    local ib = ibapi.engineapi:new(ffi.cast("ib_engine_t*", ib_engine))

    -- Get the main context. All rules are added to the main context.
    local mainctx = ffi.C.ib_context_main(ib_engine)

    ib:logDebug("Validating rules.")
    local validator = Waggle:Validate()
    if type(validator) ~= 'string' then
        if validator:has_warnings() then
            for _, rec in ipairs(validator.warnings) do
                ib:logWarn("%s:%d Rule %s %d - %s",
                    rec.source,
                    rec.line,
                    rec.sig_id,
                    rec.sig_rev,
                    rec.msg)
            end
        else
            ib:logDebug("No warnings found")
        end

        if validator:has_errors() then
            for _, rec in ipairs(validator.warnings) do
                ib:logError("%s:%d Rule %s %d - %s",
                    rec.source,
                    rec.line,
                    rec.sig_id,
                    rec.sig_rev,
                    rec.msg)
            end
        else
            ib:logDebug("No errors found")
        end
    else
        ib:logDebug("Validation found no problems.")
    end

    local plan = Waggle:Plan()
    local db = Waggle.DEFAULT_RULE_DB
    for _, chain in ipairs(plan) do
        local rc = build_rule(ib, mainctx, chain, db)
        if rc ~= ffi.C.IB_OK then
            return rc
        end
    end

    -- We're done. Clear out the rules.
    Waggle:clear_rule_db()

    return ffi.C.IB_OK
end

return M
