Sig("mySig", 10):
    fields("ARGS.trim()"):
    phase("REQUEST"):
    action("severity:1"):
    action("confidence:1"):
    tags("t1", "t2"):
    op("streq", "hi"):
    message("OK!")

Sig("mySig2", 11):
    fields("ARGS.trim()"):
    phase("REQUEST"):
    action("severity:1"):
    action("confidence:1"):
    tags("t1", "t2"):
    op("streq", "hi"):
    message("OK %{FIELD}")

Sig("mySig3", 1):
   fields("args.trim()"):
   phase("request"):
   action("severity:1"):
   action("confidence:1"):
   action("event"):
   tags("t1", "t2"):
   op("streq", "hi"):
   message("ok!")

Sig("mySig4", 1):
   fields("args.trim()"):
   phase("request"):
   action("severity:1"):
   action("confidence:1"):
   action("event"):
   tags("t1", "t2"):
   op("!rx", "hi"):
   message("ok!")

Sig("mySig5", 10):
   fields("ARGS.trim()"):
   phase("REQUEST"):
   action("severity:1"):
   action("confidence:1"):
   tags("t1", "t2"):
   op("!streq", "hi"):
   message("OK!")


