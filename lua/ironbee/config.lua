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
    DoInDSL(function()
        dofile(file)
    end)

    return ffi.C.IB_OK
end

-- Build and add all rules configured to the engine.
_M.build_rules = function(cp)
    local validator = Waggle:Validate()
    if type(validator) ~= 'string' then
        if validator:has_warnings() then
            for _, rec in ipairs(validator.warnings) do
                -- rec.line
                -- rec.source
                -- rec.short_src
                -- rec.sig_id
                -- rec.sig_rev
                -- rec.msg
            end
        end
        if validator:has_errors() then
            for _, rec in ipairs(validator.warnings) do
                -- rec.line
                -- rec.source
                -- rec.short_src
                -- rec.sig_id
                -- rec.sig_rev
                -- rec.msg
            end
        end
    end

    local plan = Waggle:Plan()
    local db = Waggle.DEFAULT_RULE_DB
    for _, chain in ipairs(plan) do
        for i, link in ipairs(chain) do
            local rule_id = link.rule
            local result = link.result
            local rule = db.db[rule_id]
            -- FIXME register a rule
        end
    end

    -- We're done. Clear out the rules.
    Waggle:clear_rule_db()
end

return _M
