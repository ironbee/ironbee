-- A Test Lua Module
t = ...

-- Register EVERY callback we can with a pretty simple function.
for k,v in pairs(t) do
    if (type(v) == "function") then
        v(t, function()
            print("Running callback " .. k)
            return 0
        end)
    end
end

-- Debug output
print("----- t Contents ------")
for k,v in pairs(t) do
    print(k,v)
end

print("----- t.events Contents ------")
-- Debug output
for k,v in pairs(t.events) do
    print(k,v)
end

print("A test Lua Module")

-- Return IB_OK
return 0
