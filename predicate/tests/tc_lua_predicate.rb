class TestLuaPredicate < CLIPPTest::TestCase
  include CLIPPTest

  def make_config(lua_program, extras = {})
    return {
      modules: ['lua', 'pcre', 'htp'],
      predicate: true,
      lua_include: lua_program,
      config: "",
      default_site_config: "RuleEnable all",
    }.merge(extras)
  end

  def test_basic
    lua = <<-EOS
      Action("basic1", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic1]]):
        predicate(P.Operator('rx', 'GET', P.Var('REQUEST_METHOD')))
    EOS

    clipp(make_config(lua,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic1/
  end

  def test_template
    lua = <<-EOS
      local getField = PUtil.Define('tc_lua_predicate_getField', {'name'},
        P.Var(P.Ref('name'))
      )
      Action("basic1", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic1]]):
        predicate(P.Operator('rx', 'GET', getField('REQUEST_METHOD')))
    EOS

    clipp(make_config(lua,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic1/
  end

  def test_string_replace_rx
    lua = <<-EOS
      Action("basic1", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:srr1]]):
        predicate(
          P.P(P.StringReplaceRx('a', 'b', 'bar'))
        )
    EOS

    clipp(make_config(lua,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: srr1/
    assert_log_match "'bbr'"
  end

  def test_foperator
    lua = <<-EOS
      Action("basic1", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:foperator]]):
        predicate(P.P(P.FOperator('rx', 'a', P.Cat('a', 'ab', 'cb'))))
    EOS

    clipp(make_config(lua,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: foperator/
    assert_log_match "['a' 'ab']"
  end

  def test_genevent
    lua = <<-EOS
      Action("genevent1", "1"):
        phase("REQUEST_HEADER"):
        action("clipp_announce:foo"):
        predicate(
          P.GenEvent(
            "some/rule/id",
            1,
            "observation",
            "log",
            50,
            50,
            "Big problem",
            { "a", "b" }
          )
        )
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo\""))

    assert_no_issues
    assert_log_match 'clipp_announce(foo)'
    assert_log_match "predicate((genEvent 'some/rule/id' 1 'observation' 'log' 50 50 'Big problem' ['a' 'b']))"
  end

  def test_gen_event_expand
    lua = <<-EOS

      InitVar("MY_TYPE", "observation")
      InitVar("MY_ACTION", "log")
      InitVar("MY_SEVERITY", "50")
      InitVar("MY_CONFIDENCE", "40")
      InitVar("MY_MSG", "Big problem")
      InitVar("MY_TAG", "TAG1")

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.GenEvent(
            "some/rule/id",
            1,
            "%{MY_TYPE}",
            "%{MY_ACTION}",
            "%{MY_CONFIDENCE}",
            "%{MY_SEVERITY}",
            "%{MY_MSG}",
            { "a", "b", "%{MY_TAG}" }
          )
        )
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)
        print("Got logevent.")
        print("Type: "..logevent:getType())
        print("Action: "..logevent:getAction())
        print("Msg: "..logevent:getMsg())
        print("RuleId: "..logevent:getRuleId())
        print("Severity: "..logevent:getSeverity())
        print("Confidence: "..logevent:getConfidence())

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match 'clipp_announce(foo)'
    assert_log_match("Type: observation")
    assert_log_match("Action: log")
    assert_log_match("Msg: Big problem")
    assert_log_match("Severity: 50")
    assert_log_match("Confidence: 40")
    assert_log_match("Msg: Big problem")
    assert_log_match("RuleId: some/rule/id")
    assert_log_match("Tag: a")
    assert_log_match("Tag: b")
    assert_log_match("Tag: TAG1")
  end

  # This tests a corner case of event tags. That is, when a tag list
  # is not a list, but a single expandable string.
  def test_gen_event_expand_single_tag
    lua = <<-EOS

      InitVar("MY_TAG", "TAG1")

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.GenEvent(
            "some/rule/id",
            1,
            "observation",
            "log",
            10,
            50,
            "Event happend.",
            "%{MY_TAG}"
          )
        )
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match 'clipp_announce(foo)'
    assert_log_match("Tag: TAG1")
  end

  # This tests a corner case of event tags. That is, when a tag list
  # is not a list, but a single expandable string.
  def test_gen_event_missing_expansion
    lua = <<-EOS

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.GenEvent(
            "some/rule/id",
            1,
            "observation",
            "log",
            10,
            50,
            "%{MY_MSG}",
            "%{MY_TAG}"
          )
        )
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)

        print("Msg: "..logevent:getMsg())

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match 'Msg: %{MY_MSG}'
    assert_log_match 'Tag: %{MY_TAG}'
  end

  def test_rule_msg_expand
    lua = <<-EOS

      InitVar("MY_MSG", "This is a rule message.")

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.GenEvent(
            "some/rule/id",
            1,
            "observation",
            "log",
            10,
            50,
            P.RuleMsg("genevent1"),
            "a_tag"
          )
        ):message("%{MY_MSG}")
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)

        print("Msg: "..logevent:getMsg())

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match 'Msg: This is a rule message.'
  end

  def test_rule_msg_no_expand
    lua = <<-EOS

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.GenEvent(
            "some/rule/id",
            1,
            "observation",
            "log",
            10,
            50,
            P.RuleMsg("genevent1"),
            "a_tag"
          )
        ):message('This is a rule message.')
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)

        print("Msg: "..logevent:getMsg())

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match 'Msg: This is a rule message.'
  end

  def test_rule_msg_failed_expand
    lua = <<-EOS

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.GenEvent(
            "some/rule/id",
            1,
            "observation",
            "log",
            10,
            50,
            P.RuleMsg("genevent1"),
            "a_tag"
          )
        ):message("%{MY_MSG}")
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)

        print("Msg: "..logevent:getMsg())

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match /Msg: $/

  end

  def test_lua_set_predicate_var1
    lua = <<-EOS

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.SetPredicateVar(
            P.WaitPhase('REQUEST', P.Var('ARGS')),
            P.GenEvent(
              "some/rule/id",
              1,
              "observation",
              "log",
              10,
              50,
              "Matched %{PREDICATE_VALUE_NAME}=%{PREDICATE_VALUE}",
              "a_tag"
            )
          )
        )
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)

        print("Msg: "..logevent:getMsg())

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo?a=b\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match 'Msg: Matched ARGS=b'
  end

  def test_lua_set_predicate_var2
    lua = <<-EOS

      Action("genevent1", "1"):
        phase("REQUEST"):
        action("clipp_announce:foo"):
        predicate(
          P.FinishAll(
            P.SetPredicateVar(P.WaitPhase('REQUEST', P.Var('ARGS'))),
            P.GenEvent(
              "some/rule/id",
              1,
              "observation",
              "log",
              10,
              50,
              "Matched %{PREDICATE_VALUE_NAME}=%{PREDICATE_VALUE}",
              "a_tag"
            )
          )
        )
    EOS

    lua_module = <<-EOS
      m = ...
      m:logevent_handler(function(tx, logevent)

        print("Msg: "..logevent:getMsg())

        for _, tag in logevent:tags() do
          print("Tag: "..tag)
        end

        return 0
      end)

      return 0
    EOS

    clipp(make_config(lua, input: "echo:\"GET /foo?a=b\"", lua_module: lua_module))

    assert_no_issues
    assert_log_match 'Msg: Matched ARGS=b'
  end

  def test_lua_predicate_add_to_graph

      lua_include = <<-EOS
        local label = "myLabel"
        local p1    = P.Label(label, P.P(P.Length(P.N('foo', true))))
        local p2    = P.Call(label)

        PredicateAddToGraph(p1())

        Predicate('mypredicate', 1):
          phase('REQUEST_HEADER'):
          action('clipp_announce:field_present'):
          predicate(p2())

      EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b\""))

    assert_no_issues
    assert_log_match '[rule:"main/mypredicate" rev:1] ACTION clipp_announce(field_present)'
    assert_log_match '[rule:"main/mypredicate" rev:1] ACTION predicate((call \'myLabel\'))'

  end

  def test_lua_predicate_add_to_graph_tags

      lua_include = <<-EOS
        local tag = "myTag"
        local p1  = P.Tag(tag, P.P(P.Length(P.N('foo', true))))
        local p2  = P.CallTagged(tag)

        PredicateAddToGraph(p1())
        print(p1())
        print(p2())

        Predicate('mypredicate', 1):
          phase('REQUEST_HEADER'):
          action('clipp_announce:field_present'):
          predicate(p2())

      EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b\""))

    assert_no_issues
    assert_log_match '[rule:"main/mypredicate" rev:1] ACTION clipp_announce(field_present)'
    assert_log_match '[rule:"main/mypredicate" rev:1] ACTION predicate((callTagged \'myTag\'))'

  end

  def test_lua_predicate_add_to_graph_2_tags

      lua_include = <<-EOS
        local tag = "myTag"
        local p1  = P.Tag({tag, "myOtherTag"}, P.P(P.Length(P.N('foo', true))))
        local p2  = P.CallTagged(tag)

        PredicateAddToGraph(p1())
        print(p1())
        print(p2())

        Predicate('mypredicate', 1):
          phase('REQUEST_HEADER'):
          action('clipp_announce:field_present'):
          predicate(p2())

      EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b\""))

    assert_no_issues
    assert_log_match '[rule:"main/mypredicate" rev:1] ACTION clipp_announce(field_present)'
    assert_log_match '[rule:"main/mypredicate" rev:1] ACTION predicate((callTagged \'myTag\'))'

  end

  def test_lua_predicate_negate_foperator_rx
    lua_include = <<-EOS
      Predicate('mypredicate', 1):
        phase('REQUEST_HEADER'):
        action('clipp_announce:announce'):
        predicate(P.P(P.FOperator('!rx', 'b', P.Var('ARGS'))))
    EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b&c=d&e=f\""))

    assert_no_issues
    assert_log_match "[c:'d' e:'f']"
  end

  def test_lua_predicate_negate_notNamed
    lua_include = <<-EOS
      Predicate('mypredicate', 1):
        phase('REQUEST_HEADER'):
        action('clipp_announce:announce'):
        predicate(P.P(P.NotNamed('a', P.Var('ARGS'))))
    EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b&c=d&e=f\""))

    assert_no_issues
    assert_log_match "[c:'d' e:'f']"
  end

  def test_lua_predicate_negate_notNamedI
    lua_include = <<-EOS
      Predicate('mypredicate', 1):
        phase('REQUEST_HEADER'):
        action('clipp_announce:announce'):
        predicate(P.P(P.NotNamedI('A', P.Var('ARGS'))))
    EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b&c=d&e=f\""))

    assert_no_issues
    assert_log_match "[c:'d' e:'f']"
  end

  def test_lua_predicate_negate_notSub
    lua_include = <<-EOS
      Predicate('mypredicate', 1):
        phase('REQUEST_HEADER'):
        action('clipp_announce:announce'):
        predicate(P.P(P.NotSub('A', P.Var('ARGS'))))
    EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b&c=d&e=f\""))

    assert_no_issues
    assert_log_match "[c:'d' e:'f']"
  end

  def test_lua_predicate_negate_notNamedRx
    lua_include = <<-EOS
      Predicate('mypredicate', 1):
        phase('REQUEST_HEADER'):
        action('clipp_announce:announce'):
        predicate(P.P(P.NotNamedRx('a', P.Var('ARGS'))))
    EOS

    clipp(make_config(lua_include, input: "echo:\"GET /foo?a=b&c=d&e=f\""))

    assert_no_issues
    assert_log_match "[c:'d' e:'f']"
  end
end
