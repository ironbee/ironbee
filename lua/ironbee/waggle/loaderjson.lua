
-- ###########################################################################
-- LoaderJSON - generate a rules.conf or similar
-- ###########################################################################
local ibjson = require('ibjson')
local LoaderJSON = {}
if _VERSION ~= 'Lua 5.2' then
    table.unpack = unpack
end

LoaderJSON.__index = LoaderJSON
LoaderJSON.type = 'loaderjson'
LoaderJSON.new = function(self)
    local l = {
        follow_ids = {},
        prev_id = nil
    }
    return setmetatable(l, self)
end

-- Rule loading that is common to all rule types.
--
-- @returns Table of values used to build the rule and a sig object. 
--          data, sig = self:loadCommonRule(jsonsig)
LoaderJSON.loadCommonRule = function(self, jsonsig)
    local data = {
        id = nil,
        version = nil,
        actions = {},
        tags = {},
        fields = {},
        phase = nil,
        op = string.gsub(jsonsig.operator, '@', '', 1),
        op_arg = jsonsig.operator_argument,
        message = nil,
        comment = jsonsig.comment,
        after = {},
        before = {},
        follows = {},
        chain = false
    }

    for _, action in ipairs(jsonsig.actions) do
        local name, arg = action.name, action.argument
        if name == 'id'  then
            data.id = arg 
        elseif name == 'rev' then
            data.version = arg
        elseif name == 'msg' then
            data.message = arg
        elseif name == 'phase' then
            data.phase = arg
        elseif name == 'chain' then
            data.chain = true
        elseif name == 'tag' then
            table.insert(data.tags, arg)
        else
            table.insert(data.actions, string.format("%s:%s", name, arg))
        end
    end

    -- Not all rules have an ID.
    if not data.id then
        data.id = math.random(0, 100000)
        data.version = 1
    end

    return data
end

-- Once a rule's data is loaded, it needs some common parts applied.
-- This takes a data object returned by loadCommonRule apply the common parts to it.
LoaderJSON.applyCommonRule = function(self, jsonsig, sig, data)
    sig:tags(table.unpack(data.tags))
    sig:tags(table.unpack(jsonsig.tags))
    sig:actions(table.unpack(data.actions))

    if data.message then
        sig:message(data.message)
    end

    if data.comment then
        sig:comment(data.comment)
    end

    if data.op then
        sig:op(data.op, data.op_arg)
    end

    for _, field in ipairs(jsonsig.fields) do 
        local f = field.collection
        if field.selector then
            f = f .. ':' .. field.selector
        end
        if field.transformation then
            f = f .. '.' .. field.transformation
        end

        sig:fields(f)
    end
end

LoaderJSON.loadExtRule = function(self, jsonsig, sig, data, db)
end

LoaderJSON.loadStrRule = function(self, jsonsig, sig, data, db)
end

-- Load a normal rule.
LoaderJSON.loadRule = function(self, jsonsig, sig, data, db)

    if data.phase then
        sig:phase(data.phase)
    end

    -- If we have a chain action, note that following rules need to follow
    -- this one.
    if data.chain then
        table.insert(self.follow_ids, data.id)

    -- If we have no chain and we have follow_ids, build the chain.
    elseif #self.follow_ids > 0 then

        -- Copy the first rule's phase to all following rules in a chain.
        local first_rule_phase = db.db[self.follow_ids[1]].data.phase
        sig:phase(first_rule_phase)
        for _, rule_id in ipairs(self.follow_ids) do
            sig:follows(rule_id)
            db.db[rule_id]:phase(first_rule_phase)
        end

        self.follow_ids = {}

    -- If there is no chaining in play, maintain order by building up
    -- the after list.
    else
        sig:after(prev_id)
    end

    self.prev_id = id
end

LoaderJSON.load = function(self, json, db)
    local sigs = ibjson.to_value(json)

    if #sigs > 0 then
        for _, jsonsig in ipairs(sigs) do 

            local mksig_fn
            local load_fn

            -- Pick what functions to use per rule.
            if jsonsig.rule_type == 'Rule' then
                mksig_fn = db.Rule
                load_fn = self.loadRule
            elseif jsonsig.rule_type == 'RuleExt' then
                mksig_fn = db.ExtSig
                load_fn = self.loadExtRule
            elseif jsonsig.rule_type == 'StreamInspect' then
                mksig_fn = db.StrSig
                load_fn = self.loadStrRule
            end

            -- Do some common pre-processing that allows us to create a rule.
            local data = self:loadCommonRule(jsonsig)

            -- Call signature factory method against db.
            local sig = mksig_fn(db, data.id, data.version)

            -- Do some common post processing.
            self:applyCommonRule(jsonsig, sig, data)

            -- Finally, do signature-specific processing.
            load_fn(self, jsonsig, sig, data, db)
        end
    end
end

return LoaderJSON
