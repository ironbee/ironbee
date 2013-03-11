-- ###########################################################################
-- Planner
-- ###########################################################################
--
-- Define a planner class. This holds the logic for taking
-- a SignatureDatabase and producing an execution plan.
--
-- The planner interface is only planner:plan(db). Everything else is
-- optional.
--
-- The planner borrows a C++ism by prefixing its data fields with m_
-- for "member" where functions have no such prefix. This it avoid
-- name collisions with fields like "plan" which are the verb and the noun,
-- or the function and the resultant data.
--
local Planner = {}
Planner.__index = Planner
Planner.type = "planner"
Planner.new = function(self)
    local p = {
        -- Table of rule_ids that have been put in the m_plan list.
        m_inplan = {},

        -- Lists of list rule IDs. This represents the final execution plan.
        -- The sub-lists, composed of rule ids, represent rule chains.
        -- Most lists will be of size 1, in which case there is no rule chain
        -- in use.
        m_plan = {},

        -- Table indexed by rule ids indicating that the rule may not
        -- be scheduled yet because we are currently trying to schedule
        -- a rule that must come before it. The value is the number
        -- of rules blocking the key from being scheduled.
        m_notyet = {},

        -- Table of rules that are in the planning stage.
        m_inplanning = {}

    }
    return setmetatable(p, self)
end

-- Insert or increment the rule_id in the m_notyet table to prevent
-- planning the rule.
--
-- If a rule is attempted to be planned that is in the m_notyet table
-- then a loop in the rule dependency graph has been detected and it is
-- a fatal error to the plan.
Planner.block_planning = function(self, rule_id)
    if self.m_notyet[rule_id] == nil then
        self.m_notyet[rule_id] = 1
    else
        self.m_notyet[rule_id] = self.m_notyet[rule_id] + 1
    end
end

-- Perform the reverse of block_planning, allowing a rule to be planned.
Planner.unblock_planning = function(self, rule_id)
    if self.m_notyet[rule_id] == 1 then
        self.m_notyet[rule_id] = nil
    elseif self.m_notyet[rule_id] ~= nil then
        self.m_notyet[rule_id] = self.m_notyet[rule_id] - 1
    end
end

-- Is a rule planned? 
-- @param[in] self Self.
--
-- @param[in] rule_id String representing the rule id.
--
-- @returns true or false
Planner.is_planned = function(self, rule_id)
    return self.m_inplan[rule_id] ~= nil
end

-- Take a list of rule_ids and tags and build a list of rule ids only.
--
-- @param[in] self The planner.
-- @param[in] list List of tags and rule ids.
-- @param[in] db The database of signatures (rules) to use for
--            conversion.
-- @returns List of rule ids, only. This list may have
--          duplicates in it, which is OK.
Planner.to_rule_ids = function(self, list, db)
    local r = {}

    for _, value in ipairs(list) do
        if db.db[value] ~= nil then
            table.insert(r, value)
        elseif db.tag_db[value] ~= nil then
            for rule_id, _ in pairs(db.tag_db[value]) do
                table.insert(r, rule_id)
            end
        end
    end

    return r
end

-- Plan a single rule and all its dependencies.
-- Returns true on success and exits with error({sig_id, sig_rev, msg}, 1) on error.
Planner.plan_rule = function(self, rule, db)
    -- Block before rules.
    for _, before_rule_id in ipairs(self:to_rule_ids(rule.data.before, db)) do
        self:block_planning(before_rule_id)
    end

    -- Loop detection: We will only plan rule X while planning rule X if
    --                 we recurse the rule.data.after list and encounter
    --                 rule X again. 
    if self.m_inplanning[rule.data.id] ~= nil then
        error( {
            sig_id = rule.data.id,
            sig_rev = rule.data.version,
            msg = string.format("Rule %s was attempted to be scheduled after itself.", rule.data.id)
        }, 1)
    end

    -- Loop detection: If a rule is reachable through its own before list,
    --                 then error is detected here as the rule is still
    --                 blocked from being scheduled.
    if self.m_notyet[rule.data.id] ~= nil then
        error( {
            sig_id = rule.data.id,
            sig_rev = rule.data.version,
            msg = string.format("Rule %s was attempted to be scheduled before itself.", rule.data.id)
        }, 1)
    end

    -- After the inplanning check, mark this rule as in planning.
    self.m_inplanning[rule.data.id] = 1

    -- Plan all the rules that we come after first.
    for _, after_rule_id in ipairs(self:to_rule_ids(rule.data.after, db)) do
        if not self.m_inplan[after_rule_id] then
            if not self:plan_rule(db.db[after_rule_id], db) then
                return nil
            end
        end
    end

    -- Build the rule chain to insert
    local rule_chain = {}
    for _, rule_link in ipairs(rule.data.follows) do
        -- Insert the rule_link table. Simple.
        table.insert(rule_chain, rule_link)

        -- NOTE: This if block is a little unintuitive. 
        --       Rules used in chains may never be independent rules.
        --       Because we may have already scheduled a rule,
        --       not then knowing it would be used in a chain,
        --       we must remove it (the if-block). We remove
        --       a rule by replacing its chain table with an empty list.
        --       If a chained rule is not-yet used in the plan, we
        --       mark it as such (the else-block) to prevent it
        --       from ever being scheduled.
        if self.m_inplan[rule_link.rule] ~= nil then
            self.m_plan[self.m_inplan[rule_link.rule]] = {}
        else
            self.m_inplan[rule_link.rule] = #self.m_plan
        end
    end
    table.insert(rule_chain, { rule = rule.data.id, result = true })

    -- Insert ourselves in the plan.
    table.insert(self.m_plan, rule_chain)
    self.m_inplan[rule.data.id] = #self.m_plan

    -- Note that we are not currently planning this rule.
    self.m_inplanning[rule.data.id] = nil 

    -- Unblock before rules.
    for _, before_rule_id in ipairs(self:to_rule_ids(rule.data.before, db)) do
        self:unblock_planning(before_rule_id)
    end

    -- Indicate success.
    return true
end

-- Produce a valid rule execution plan.
--
-- In the case of chained rules, the last rule in the chain's ID is
-- produced to identify the end of the chain. During
-- final RuleConf generation a suitable ID will be used.
--
-- @param[in] self This database.
-- @returns A list of lists rule IDs or nil on error.
--          See the field "error_message" for a description of what may
--          have gone wrong.
--          The list elements (the sub lists) represent rule chains.
--          A rule chain is a list of rules that execute in 
--          order until one of them fails. Think of them as a Horn clause
--          or a lazy-evaluated left-to-right list of conjunctions (ands).
--
--          A rule chain may be length 0 in cases where the planner decided to
--          removed a rule that was already inserted into the plan.
--
--          More typically the rule chains will contain 1 or more
--          tables, each table containing 2 fields, "rule" and "result".
--          The "rule" field is the rule id in the SignatureDatabase
--          used to generate the plan. For generation, look up the full
--          rule in that object. The second field, "result", is
--          the result that the "rule" must return in order for the 
--          next rule in the list to be executed.
--
--          In the case of the last rule in the list, the value can be ignored. 
--          It is always true, but shouldn't actually effect execution 
--          in any way.
Planner.plan = function(self, db)
    self.m_plan = {}

    for rule_id, rule in pairs(db.db) do
        if not self:is_planned(rule_id) then
            if not self:plan_rule(rule, db) then
                return nil
            end
        end
    end

    return self.m_plan
end

return Planner
