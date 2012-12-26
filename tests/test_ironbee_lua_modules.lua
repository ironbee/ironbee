-- A Test Lua Module
t = ...

-- Register EVERY callback we can with a pretty simple function.
for k,v in pairs(t) do
    if (type(v) == "function") then
        v(t, function(ib)
            ib:logInfo("Inside running callback %s", k)
            return 0
        end)
    end
end

-- Debug output
t:logInfo("----- t Contents ------")
for k,v in pairs(t) do
    t:logInfo("Table contents: %s=%s", k, v)
end

t:logInfo("----- t.events Contents ------")
-- Debug output
for k,v in pairs(t.events) do
    t:logInfo("Events table contents: %s=%s", k, v)
end

t:logInfo("A test Lua Module.")

-- Return IB_OK
return 0
