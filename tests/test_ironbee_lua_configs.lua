Sig("mySig", 10):
    fields("ARGS.trim()"):
    phase("REQUEST"):
    action("severity:1"):
    action("confidence:1"):
    tags("t1", "t2"):
    op("streq", "hi"):
    message("OK!")
