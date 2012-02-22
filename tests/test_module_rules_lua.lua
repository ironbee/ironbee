
t=...

local log = function(...)
  io.stderr:write(" -- ")
  io.stderr:write(...)
  io.stderr:write("\n")
end
  

if t == nil then
  log("T is null. No action")
else

  t.ib:logDebug("Starting rule.")

  if t.ib_tx == nil then
    t.ib:logDebug("T.ib_tx is null. Failing.")
    tx = nil
  else
    log(string.format("T is %s.", tostring(t)))
    log(string.format("T.ib_tx is %s.", tostring(t.ib_tx)))
    tx = ironbee.cast_tx(t.ib_tx)
    log(string.format("tx is %s.", tostring(tx)))
    log(string.format("tx.id is %s.", tostring(tx.id)))
  end

  t.ib:logDebug("TX id value is \"" .. ffi.string(tx.id) .. "\"")
end

-- Return 5
return 5
