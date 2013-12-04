Rule("mySig", 10):
    fields("ARGS.trim()"):
    phase("REQUEST"):
    action("severity:1"):
    action("confidence:1"):
    tags("t1", "t2"):
    op("streq", "hi"):
    message("OK!")

Rule("mySig2", 11):
    fields("ARGS.trim()"):
    phase("REQUEST"):
    action("severity:1"):
    action("confidence:1"):
    tags("t1", "t2"):
    op("streq", "hi"):
    message("OK %{FIELD}")

Rule("mySig3", 1):
   fields("args.trim()"):
   phase("request"):
   action("severity:1"):
   action("confidence:1"):
   action("event"):
   tags("t1", "t2"):
   op("streq", "hi"):
   message("ok!")

Rule("mySig4", 1):
   fields("args.trim()"):
   phase("request"):
   action("severity:1"):
   action("confidence:1"):
   action("event"):
   tags("t1", "t2"):
   op("!rx", "hi"):
   message("ok!")

Rule("mySig5", 10):
   fields("ARGS.trim()"):
   phase("REQUEST"):
   action("severity:1"):
   action("confidence:1"):
   tags("t1", "t2"):
   op("!streq", "hi"):
   message("OK!")


