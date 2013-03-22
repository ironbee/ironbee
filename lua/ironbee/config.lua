--[[-------------------------------------------------------------------------
-- IronBee configuration code.
--]]-------------------------------------------------------------------------

local ffi = require('ffi')

local _M = {}
_M.__index = _M
_M._COPYRIGHT = "Copyright (C) 2010-2013 Qualys, Inc."
_M._DESCRIPTION = "IronBee Configuration"
_M._VERSION = "1.0"

local Waggle = require('ironbee/waggle')

-- Pair of #defines from C code imported here.
local IB_RULEMD_FLAG_EXPAND_MSG = 1
local IB_RULEMD_FLAG_EXPAND_DATA = 2

-- Setup the configuration DLS, run the function provided, tear down the DSL.
--
-- param[in] f Function to run after the DSL is installed in _G.
--
local DoInDSL = function(f)

    local SkipTable = {
        _DESCRIPTION = 1,
        _COPYRIGHT = 1,
        __index = 1,
        _VERSION = 1
    }

    local to_nil = {}

    -- Setup all symbols.
    for k,v in pairs(Waggle) do
        if not SkipTable[k] then
            _G[k] = v
            table.insert(to_nil, k)
        end
    end

    f()

    -- Teardown.
    for _, k in ipairs(to_nil) do
        _G[k] = nil
    end
end

-- Include a configuration file.
--
-- Rules are not committed to the engine until the end of the 
-- configuration phase.
--
-- param[in] cp Configuraiton Parser.
-- param[in] file String point to the file to operate on.
_M.include = function(cp, file)
    local ib = ibapi.engineapi:new(ffi.cast("ib_cfgparser_t*", cp).ib)

    -- Not an absolute path, it is a candidate for searching.
    if string.sub(file, 1, 1) ~= '/' then
    end

    DoInDSL(function()
        ib:logInfo("Loading file %s", file)
        dofile(file)
        ib:logInfo("Done loading file %s", file)
    end)

    return ffi.C.IB_OK
end

--
-- param[in] ib IronBee engine.
-- param[in] rule Lua rule table.
-- param[in] prule An ib_rule_t*[1].
-- param[in] field Lua field table in rule.data.fields.
--
local add_fields = function(ib, rule, prule, field)
    local str = field.collection
    local name = field.collection
    local not_found = ffi.new("int[1]")
    local target = ffi.new("ib_rule_target_t*[1]")
    local tfn_names = ffi.new("ib_list_t*[1]")
    if field.selector then
        str = str .. ":" .. field.selector
        name = name .. ":" .. field.selector
    end
    if field.transformation then
        str = str .. "." .. field.transformation
    end

    rc = ffi.C.ib_list_create(
        tfn_names,
        ffi.C.ib_engine_pool_main_get(ib.ib_engine))
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to create new ib_list_t.")
        return rc
    end

    -- Build up transformation list.
    if field.transformation then
        -- Split and walk the transformation list.
        for tfn_name in string.gmatch(field.transformation, "([^.()]+)") do
            -- Add it to the list.
            rc = ffi.C.ib_list_push(tfn_names[0], ffi.cast("char*", tfn_name))
            if rc ~= ffi.C.IB_OK then
                ib:logError(
                    "Failed to add transformation %s to list.",
                    tfn_name)
                return rc
            end
        end
    end

    -- Create target
    rc = ffi.C.ib_rule_create_target(
        ib.ib_engine,
        str,
        name,
        tfn_names[0],
        target,
        not_found)
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to create field %s", str)
        return rc
    end

    -- Add target.
    rc = ffi.C.ib_rule_add_target(
        ib.ib_engine,
        prule[0],
        target[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to add field %s to rule.", str)
        return rc
    end

    return ffi.C.IB_OK
end

-- Create, setup, and register a rule in the given ironbee engine.
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

        ffi.C.ib_rule_set_id(ib.ib_engine, prule[0], rule_id)
        
        for _, action in ipairs(rule.data.actions) do
            local name, arg = action.name, action.argument
    
            if name == "logdata" then
                local expand = ffi.new("bool[1]")

                prule[0].meta.data = 
                    ffi.C.ib_mpool_memdup(
                        ffi.C.ib_engine_pool_main_get(ib.ib_engine),
                        arg,
                        #arg+1)
                rc = ffi.C.ib_data_expand_test_str(arg, expand)
                if rc ~= ffi.C.IB_OK then
                    ib:logError("Cannot expand logdata %d.", rc)
                end

                if expand[0] ~= 0 then
                    prule[0].meta.flags = ffi.C.ib_set_flag(
                        prule[0].meta.flags,
                        IB_RULEMD_FLAG_EXPAND_DATA)
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

            -- Handling of Actions
            else

                -- Detect inverted actions (actions starting with !)
                local is_inverted = ffi.new("ib_rule_action_t")
                if string.sub(name, 1, 1) == '!' then
                    name = string.sub(name, 2)

                    -- fire false actions.
                    is_inverted = ffi.C.RULE_ACTION_FALSE
                else
                    -- fire true actions.
                    is_inverted = ffi.C.RULE_ACTION_TRUE
                end

                -- Create the action instance.
                local action_inst = ffi.new("ib_action_inst_t*[1]")
                rc = ffi.C.ib_action_inst_create(
                    ib.ib_engine,
                    ctx,
                    name,
                    arg,
                    0,
                    action_inst)
                if rc ~= ffi.C.IB_OK then
                    ib:logError(
                        "Failed to create action %s instance for rule.", name)
                    return rc
                end

                -- Add the action instance.
                rc = ffi.C.ib_rule_add_action(
                    ib.ib_engine,
                    prule[0],
                    action_inst[0],
                    is_inverted)
                if rc ~= ffi.C.IB_OK then
                    ib:logError("Failed to add action instance to rule.")
                    return rc
                end
            end
        end

        -- Set tags
        for _, tag in ipairs(rule.data.tags) do
            rc = ffi.C.ib_list_push(prule[0].meta.tags, tag)
            if rc == ffi.C.IB_OK then
                ib:logError("Setting tag %s failed: %d", tag, rc)
            end
        end

        -- Set message
        if rule.data.message then

            -- Set the message.
            prule[0].meta.msg = 
                    ffi.C.ib_mpool_memdup(
                        ffi.C.ib_engine_pool_main_get(ib.ib_engine),
                        rule.data.message,
                        #rule.data.message + 1)

            -- Check if the string is expandable.
            local expand = ffi.new("bool[1]")
            rc = ffi.C.ib_data_expand_test_str(rule.data.message, expand)
            if rc ~= ffi.C.IB_OK then
                ib:logError("Expansion test failed")
            end

            if expand[0] then
                prule[0].meta.flags = ffi.C.ib_set_flag(
                    prule[0].meta.flags,
                    IB_RULEMD_FLAG_EXPAND_MSG)
            end
        end

        -- Set revision
        prule[0].meta.revision = rule.data.version

        -- Chains
        if i < #chain then
            rc = ffi.C.rule_set_chain(ib.ib_engine, prule[0])
            if rc ~= ffi.C.IB_OK then
                ib:logError("Failed to setup chain rule.")
            end
        end

        for _, field in ipairs(rule.data.fields) do
            add_fields(ib, rule, prule, field)
        end

        -- Create operator instance.
        local opinst = ffi.new("ib_operator_inst_t*[1]")
        local op_inst_create_stream_flags
        local op_inst_create_inv_flag
        local op

        -- Set the flag values.
        if rule.is_streaming() then
            op_inst_create_stream_flags = 4
        else
            op_inst_create_stream_flags = 2
        end

        -- Handle inverted operator.
        if string.sub(rule.data.op, 1, 1) == '!' then
            op = string.sub(rule.data.op, 2)
            op_inst_create_inv_flag = 1
        else
            op = rule.data.op
            op_inst_create_inv_flag = 0
        end

        -- Create the argument.
        rc = ffi.C.ib_operator_inst_create(
            ib.ib_engine,
            ctx,
            prule[0],
            op_inst_create_stream_flags,
            op,
            tostring(rule.data.op_arg),
            op_inst_create_inv_flag,
            opinst)
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to create operator instance for %s.", op)
            return rc
        end

        -- Set operator
        rc = ffi.C.ib_rule_set_operator(ib.ib_engine, prule[0], opinst[0])
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to set rule operator.")
            return rc
        end

        -- Find out of this is streaming or not, and treat that as an int.
        local is_streaming
        if rule.is_streaming() then
            is_streaming = 1
        else
            is_streaming = 0
        end

        -- Find the phase and set the rule's phase.
        rc = ffi.C.ib_rule_set_phase(
            ib.ib_engine,
            prule[0],
            ffi.C.ib_rule_lookup_phase(rule.data.phase, is_streaming))
        if rc ~= ffi.C.IB_OK then
            ib:logError("Cannot set phase %s", rule.data.phase)
        end

        rc = ffi.C.ib_rule_register(ib.ib_engine, ctx, prule[0])
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to register rule.")
            return rc
        end

    end

    return ffi.C.IB_OK
end

-- 
-- Build and add all rules configured to the engine.
-- param[in] ib_ptr IronBee engine ib_engine_t*.
--
_M.build_rules = function(ib_ptr)
    local ib = ibapi.engineapi:new(ffi.cast("ib_engine_t*", ib_ptr))

    -- Get the main context. All rules are added to the main context.
    local mainctx = ffi.C.ib_context_main(ib_ptr)

    ib:logInfo("Validating rules...")
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
            ib:logInfo("No warnings found")
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
            ib:logInfo("No errors found")
        end
    else
        ib:logInfo("Validation found no problems.")
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

return _M
