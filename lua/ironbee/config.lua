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

    DoInDSL(function()
        ib:logInfo("Loading file %s", file)
        dofile(file)
        ib:logInfo("Done loading file %s", file)
    end)

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
            "[file]",
            -1, 
            ( rule.type == 'StreamRule' ),
            prule)
        if rc ~= ffi.C.IB_OK then
            ib:logError("Failed to create rule.")
            return rc
        end

        ffi.C.ib_rule_set_id(ib.ib_engine, prule[0], rule_id)
        
        -- FIXME actions
        -- FIXME modifiers
        -- FIXME   rev? tag? phase? etc...
        -- FIXME fields?
        -- FIXME operator

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
-- param[in] module The IronBee ib_module_t*.
--
_M.build_rules = function(ib_ptr, module)
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
