/****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 *
 * @brief IronBee Waggle --- Waggle DSL Documentation
 *
 * A Domain Specific Language in the Lua language for building rules for
 * IronBee. This is part of the LuaAPI.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @page Waggle Waggle
 *
 * @tableofcontents
 *
 * @section WaggleIntro Waggle Introduction
 *
 * A Domain Specific Language in the Lua language for building rules for
 * IronBee. This is part of the @ref LuaAPI.
 *
 * @section WaggleIntroApproach Approach
 *
 * The above is accomplished by using an
 * <a href="http://philcalcado.com/research-on-dsls/domain-specific-languages-dsls/internal-dsls/">internal DSL</a>
 * for Lua styled after the
 * <a href="http://en.wikipedia.org/wiki/Fluent_interface">Fluent interface</a>
 * pattern for building objects.
 *
 * @section WaggleIntroExample An Example
 *
 * @code{.lua}
 -- Building a Rule.
Rule("qrs/123", 1):
    fields("REQUEST_HEADERS"):
    op("rx", "foo|bar"):
    tags("t1"):
    actions("event"):
    actions("block:immediate"):
    follows("qrs/321", true):

-- Building a short-hand method for building a rule that checks the variables.
CheckVarMatches = function(name, var, regex)
    return Rule(name, 1):
        fields(var):
        op("rx", regex)
end

-- Utilizing shorthand method of checking for a no-body request
-- which is then used as a common dependency.
CheckVarMatches("NoBodyRequest", "REQUEST_METHOD", "^(?i:GET|HEAD)$")
Rule("qrs/1", 1):
    fields("REQUEST_HEADERS:Content-Length.count()"):
    op("gt", "0"):
    actions("event", "block"):
    message("No C-L header allowed for requests without a body."):
    follows("NoBodyRequest")
Rule("qrs/2", 1):
    fields("REQUEST_HEADERS:Content-Type.count()"):
    op("gt", "0"):
    actions("event", "block"):
    message("No C-T header allowed for requests without a body."):
    follows("NoBodyRequest")
 * @endcode
 *
 * @section WaggleIntroForTheRuleWriter For the Rule Writer
 *
 * This section contains documentation suitable for a rule writer to consult.
 * It is layed out as a series of short snippets followed by an explination
 * of what the snippet expresses. This should introduce the Rule Writer
 * to the Waggle DSL language. Notice that all examples are "just Lua".
 *
 * @code{.lua}
   Rule(<rule id>, <rule version>)
 * @endcode
 *
 * Create a rule given a required rule id and rule version.
 * These values should be unique. If they are not, a fatal error is
 * reported and processing the rules cannot continue.
 *
 * If this command executes correctly, a Rule table is returned. This
 * table contains various functions that modify the Rule table and return
 * that table to allow for configuration chaining.
 *
 * @code{.lua}
   fields("REQUEST_HEADERS", "RESPONSE_HEADERS", ... )
 * @endcode
 *
 * The fields method may be called multiple times on the Rule table.
 * It will append the list of fields to the rule's set of fields to select.
 * If no arguments are provided, then the list of fields is cleared. Note
 * that the term "field" here means the entire field selector portion of a
 * rule.
 *
 * @code{.lua}
   op("rx", "foo|bar")
 * @endcode
 *
 * This specifies the operator and the operator argument. If the second
 * argument is not specified, then an empty string is substituted to allow
 * for a more clean reading if an operator such as nop is employed. If
 * this is call a second time the operator is silently replaced.
 *
 * @code{.lua}
   message("message to display.")
 * @endcode
 *
 * Set the message for when an event is generated
 *
 * @code{.lua}
   tags("t1", "t2")
 * @endcode
 *
 * Append a list of tags to this Rule. If there are no tags provided to
 * the arguments, then the tag list is cleared.
 *
 * @code{.lua}
   actions("event", ...)
   actions("block:immediate")
 * @endcode
 *
 * The code actions function will append a list of actions to the rule. If
 * no arguments are given, then the list of actions are cleared.
 *
 * @code{.lua}
   follows(<rule id>, [true|false])
 * @endcode
 *
 * Given a rule ID this will, upon final rule generation, make this rule dependent
 * in some way upon the rule specified by the given rule ID. The rule specified
 * by the ID need not exist now, but only upon final rule processing. Also, in
 * some situations it is possible that a given rule ID will result in a copy of
 * that rule being generated with a different ID and chained to this rule.
 * In this situation the rule will potentially execute many time.
 *
 * @code{.lua}
    Rule("id100", 1):
       follows("id10", true):
           follows("id11", false):
           follows("id12", true):
       message("Rule id100 fired.")
 * @endcode
 *
 * The follows function, as a convenience, also will accept a Rule object
 * returned by Rule, but it is not expected that most rule users will
 * employ this.
 *
 * @code{.lua}
   after(<rule id | tag>)
 * @endcode
 *
 * This function speaks to ordering only. It does not care if the rule or the
 * rules represented by the tag is true or false. It does, however, re-order
 * this rule to always happen after the given tag. This is useful for rules
 * that define DPI values.
 *
 * @section WaggleIntroAliases Waggle Aliases
 *
 * While Waggle gives a very trasparent layer of organiztion for expression
 * IronBee Rules, sometimes short-hands or macros are conventient.
 *
 * The rule writer is encouraged to develop their own library of Rule
 * generation functions, but here are a few "official" aliases that
 * are built into Waggle:
 *
 * - `transform(arg)` - An alias for @c action('t', @c arg).
 * - `transformAll(arg)` - An alias for @c action('t', @c arg).
 *
 */
