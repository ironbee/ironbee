
t=...

local log = function(...)
  io.stderr:write(" -- ")
  io.stderr:write(...)
  io.stderr:write("\n")
end
  

if t == nil then
  log("T is null. No action")
else

  if t.tx == nil then
    log("T.tx is null. Failing.")
    tx = nil
  else
    log(string.format("T is %s.", tostring(t)))
    log(string.format("T.tx is %s.", tostring(t.tx)))
    tx = ironbee.cast_tx(t.tx)
    log(string.format("tx is %s.", tostring(tx)))
    log(string.format("tx.id is %s.", tostring(tx.id)))
  end

  log("TX id value is \"" .. ffi.string(tx.id) .. "\"")
end

-- Return 5
return 5
