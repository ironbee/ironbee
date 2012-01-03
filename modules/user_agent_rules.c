/*****************************************************************************
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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee - User Agent Extraction Module
 *
 * This module extracts the user agent information
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <user_agent_private.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>

/* The actual rules */
static modua_match_ruleset_t modua_match_ruleset =
{
    0,
    {

    /* Aggregators */
    {
        "aggregators/simplepie", __LINE__,
        {
            { PRODUCT, STARTSWITH, "SimplePie", YES },
            { PLATFORM, CONTAINS, "Feed Parser", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },

    /* Bots */
    {
        "crawler/yahoo", __LINE__,
        {
            { PRODUCT, STARTSWITH, "YahooSeeker-Testing", YES },
            { PLATFORM, MATCHES,
              "(compatible; Mozilla 4.0; MSIE 5.5; http://search.yahoo.com/)",
              YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/yahoo", __LINE__,
        {
            { PRODUCT, MATCHES, "YahooSeeker/1.2", YES },
            { PLATFORM, MATCHES,
              "(compatible; Mozilla 4.0; MSIE 5.5; yahooseeker at yahoo-inc dot com ;"
              " http://help.yahoo.com/help/us/shop/merchant/)",
              YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/yahoo", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES,
              "(compatible; Yahoo! Slurp China; http://misc.yahoo.com.cn/help.html)",
              YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/yahoo", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES,
              "(compatible; Yahoo! Slurp; http://help.yahoo.com/help/us/ysearch/slurp)",
              YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/newsgator", __LINE__,
        {
            { PRODUCT, MATCHES, "NewsGator/2.5", YES },
            { PLATFORM, MATCHES,
              "(http://www.newsgator.com; Microsoft Windows NT 5.1.2600.0;"
              " .NET CLR 1.1.4322.2032)",
              YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/newsgator", __LINE__,
        {
            { PRODUCT, MATCHES, "NewsGator/2.0 Bot", YES },
            { PLATFORM, MATCHES, "(http://www.newsgator.com)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/netseer", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES,
              "(compatible; NetSeer crawler/2.0; +http://www.netseer.com/crawler.html;"
              " crawler@netseer.com)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/msnbot", __LINE__,
        {
            { PRODUCT, MATCHES, "msnbot/", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/msnbot", __LINE__,
        {
            { PRODUCT, MATCHES, "msnbot/", YES },
            { PLATFORM, MATCHES, "(+http://search.msn.com/msnbot.htm)" },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/msnbot", __LINE__,
        {
            { PRODUCT, MATCHES, "msnbot/0.11", YES },
            { PLATFORM, MATCHES, "( http://search.msn.com/msnbot.htm)" },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/msnbot", __LINE__,
        {
            { PRODUCT, MATCHES, "MSNBOT/0.1", YES },
            { PLATFORM, MATCHES, "(http://search.msn.com/msnbot.htm)" },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/alexia", __LINE__,
        {
            { PRODUCT, STARTSWITH, "ia_archiver", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "Googlebot-Image/1.0", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES,
              "(compatible; Googlebot/2.1; +http://www.google.com/bot.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "Googlebot/2.1", YES },
            { PLATFORM, MATCHES, "(+http://www.googlebot.com/bot.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "Googlebot/2.1", YES },
            { PLATFORM, MATCHES, "(+http://www.google.com/bot.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/bing", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES,
              "(compatible; bingbot/2.0; +http://www.bing.com/bingbot.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/bing", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES,
              "(compatible; bingbot/2.0 +http://www.bing.com/bingbot.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/baidu", __LINE__,
        {
            { PRODUCT, MATCHES,
              "Baiduspider+(+http://www.baidu.com/search/spider_jp.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/baidu", __LINE__,
        {
            { PRODUCT, MATCHES,
              "Baiduspider+(+http://www.baidu.com/search/spider.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/baidu", __LINE__,
        {
            { PRODUCT, MATCHES, "BaiDuSpider", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/uptimemonkey", __LINE__,
        {
            { PRODUCT, STARTSWITH, "UptimeMonkey", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/nagios", __LINE__,
        {
            { PRODUCT, STARTSWITH, "check_http", YES },
            { PLATFORM, CONTAINS, "nagios-plugins", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/pingdom", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Pingdom.com_bot", YES },
            { PLATFORM, MATCHES, "(http://www.pingdom.com)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PLATFORM, CONTAINS, "+http://google.com/bot.html", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    { 
        "crawler/ahrefs", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(compatible; AhrefsBot", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/aboundex", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Aboundex", YES },
            { PLATFORM, CONTAINS, "http://www.aboundex.com/crawler/", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/baidu", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Baiduspider-image", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/omgilibot", __LINE__,
        {
            { PRODUCT, STARTSWITH, "omgilibot", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/msn", __LINE__,
        {
            { PRODUCT, STARTSWITH, "msnbot-media", YES },
            { PLATFORM, MATCHES, "(+http://search.msn.com/msnbot.htm)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "Googlebot-News", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "Googlebot-Video", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "Mediapartners-Google", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/google", __LINE__,
        {
            { PRODUCT, MATCHES, "AdsBot-Google", YES },
            { PLATFORM, MATCHES, "(+http://www.google.com/adsbot.html)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/soso", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Sosospider", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/sogou", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Sogou web spider", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "crawler/mj12", __LINE__,
        {
            { PLATFORM, CONTAINS, "MJ12bot/", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },

    /* Desktop Browsers */
    {
        "browser/chrome", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "browser/firefox", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, STARTSWITH, "Gecko", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "browser/msie", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { PLATFORM, CONTAINS, "MSIE", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "browser/opera", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Opera", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "browser/opera", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { EXTRA, STARTSWITH, "Opera", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "browser/safari", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Safari", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "browser/lynx", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Lynx/", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },

    /* Libraries */
    {
        "library/binget", __LINE__,
        {
            { PRODUCT, STARTSWITH, "BinGet", YES },
            { PLATFORM, MATCHES, "(http://www.bin-co.com/php/scripts/load/)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    { 
        "library/curl", __LINE__,
        {
            { PRODUCT, STARTSWITH, "curl", YES },
            { PLATFORM, EXISTS, "", YES },
            { EXTRA, STARTSWITH, "libcurl", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/java", __LINE__,
        {
            { PRODUCT, STARTSWITH, "java", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/libwww-perl", __LINE__,
        {
            { PRODUCT, STARTSWITH, "libwww-perl", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/MS URL Control", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Microsoft URL Control", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/peach", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Peach", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/php", __LINE__,
        {
            { PRODUCT, STARTSWITH, "PHP", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/pxyscand", __LINE__,
        {
            { PRODUCT, STARTSWITH, "pxyscand", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/PycURL", __LINE__,
        {
            { PRODUCT, STARTSWITH, "PycURL", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/python-urllib", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Python-urllib", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/lwp-trivial", __LINE__,
        {
            { PRODUCT, STARTSWITH, "lwp-trivial", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/wget", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Wget", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/urlgrabber", __LINE__,
        {
            { PRODUCT, STARTSWITH, "urlgrabber", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "library/incutio", __LINE__,
        {
            { PRODUCT, STARTSWITH, "The Incutio XML-RPC", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },

    /* Mobile */
    {   
        "mobile/uzard", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/4.0", YES },
            { PLATFORM, CONTAINS, "uZardWeb/1.0", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/teleca", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; Teleca Q7; U; en)", YES },
            { EXTRA, MATCHES, "480X800 LGE VX11000", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/teashark", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "Macintosh; U; Intel Mac OS X; en)", YES },
            { EXTRA, CONTAINS, "Shark", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/skyfire", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(Macintosh; U; Intel Mac OS X 10_5_7; en-us)", YES },
            { EXTRA, CONTAINS, "Skyfire", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/semc", __LINE__,
        {
            { PRODUCT, STARTSWITH, "SonyEricsson", YES },
            { EXTRA, CONTAINS, "SEMC-Browser", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/opera", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Opera", YES },
            { PLATFORM, CONTAINS, "Opera Mobi", YES },
            { EXTRA, STARTSWITH, "Presto", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/opera", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { PLATFORM, CONTAINS, "Opera Mobi", YES },
            { EXTRA, CONTAINS, "Opera", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    { 
        "mobile/netfront", __LINE__,
        {
            { EXTRA, CONTAINS, "NetFront", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/minimo", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Minimo", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/maemo", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Maemo Browser", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/iris", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Iris", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/msie mobile", __LINE__,
        {
            { PLATFORM, CONTAINS, "IEMobile", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/symbian", __LINE__,
        {
            { PRODUCT, CONTAINS, "GoBrowser", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/fennec", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Fennec", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/dorothy", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Dorothy", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/symbian", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Doris/", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/symbian", __LINE__,
        {
            { PRODUCT, CONTAINS, "SymbianOS", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/symbian", __LINE__,
        {
            { PLATFORM, CONTAINS, "SymbianOS", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/bolt", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, CONTAINS, "BOLT/", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/blackberry", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { PLATFORM, STARTSWITH, "BlackBerry", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/blackberry", __LINE__,
        {
            { PRODUCT, STARTSWITH, "BlackBerry", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/android", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Mozilla/5.0", YES },
            { PLATFORM, CONTAINS, "Android", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/obigo", __LINE__,
        {
            { PRODUCT, CONTAINS, "Obigo", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/iphone", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(iPhone;", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/ipad", __LINE__,
        { 
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(iPad;", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/qnx", __LINE__,
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(Photon", YES },
            { EXTRA, STARTSWITH, "Gecko", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/ucweb", __LINE__,
        {
            { PRODUCT, MATCHES, "IUC", YES },
            { EXTRA, CONTAINS, "UCWEB", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/jasmine", __LINE__,
        {
            { PRODUCT, CONTAINS, "Jasmine", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/maui", __LINE__,
        {
            { PRODUCT, MATCHES, "MAUI WAP Browser", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "mobile/generic", __LINE__,
        {
            { PRODUCT, CONTAINS, "Profile/MIDP", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },

    /* Social */
    {
        "social/secondlife", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Second Life", YES },
            { PLATFORM, MATCHES, "(http://secondlife.com)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "social/secondlife", __LINE__,
        {
            { PRODUCT, MATCHES, "LSL Script", YES },
            { PLATFORM, MATCHES, "(Mozilla Compatible)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "social/facebook", __LINE__,
        {
            { PRODUCT, STARTSWITH, "facebookexternalhit", YES },
            { PLATFORM, MATCHES,
              "(+http://www.facebook.com/externalhit_uatext.php)", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },

    /* Torrent */
    {
        "torrent/transmission", __LINE__,
        {
            { PRODUCT, STARTSWITH, "Transmission", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "torrent/uTorrent", __LINE__,
        {
            { PRODUCT, STARTSWITH, "uTorrent", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },
    {
        "torrent/rtorrent", __LINE__,
        {
            { PRODUCT, STARTSWITH, "rtorrent", YES },
            { NONE, TERMINATE, NULL, NO },
        },
    },

    }
};

/**
 * @internal
 * Initialize the specified match rule.
 *
 * Initialize a field match rule by storing of the length of it's string.
 *
 * @param[in,out] rule Match rule to initialize
 *
 * @returns status
 */
static ib_status_t modua_field_rule_init(modua_field_rule_t *rule)
{
    IB_FTRACE_INIT(modua_field_rule_init);
    if (rule->string != NULL) {
        rule->slen = strlen(rule->string);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize the static rules */
ib_status_t modua_ruleset_init(unsigned int *failed_rule,
                               unsigned int *failed_frule,
                               unsigned int *failed_line )
{
    IB_FTRACE_INIT(modua_rules_init);
    modua_match_rule_t  *match_rule;
    unsigned int         match_rule_num;
    ib_status_t          rc;
    modua_field_rule_t  *field_rule;

    /* For each of the rules, */
    for (match_rule_num = 0, match_rule = modua_match_ruleset.rules;
         match_rule->category != NULL;
         ++match_rule_num, ++match_rule) {

        unsigned int field_rule_num;
        match_rule->num_rules = 0;
        match_rule->rule_num = match_rule_num;
        for (field_rule_num = 0, field_rule = match_rule->rules;
             field_rule->match_type != TERMINATE;
             ++field_rule_num, ++field_rule) {

            /* Initialize the field rules for the match rule */
            rc = modua_field_rule_init(field_rule);
            if (rc != IB_OK) {
                *failed_rule  = match_rule_num;
                *failed_frule = field_rule_num;
                *failed_line  = match_rule->line_num;
                IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
            }
            if (field_rule_num > MODUA_MAX_FIELD_RULES) {
                *failed_rule  = match_rule_num;
                *failed_frule = field_rule_num;
                *failed_line  = match_rule->line_num;
                IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
            }
            ++match_rule->num_rules;
        }

        /* Update the match rule count */
        ++modua_match_ruleset.num_rules;
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Get the match rule set */
const modua_match_ruleset_t *modua_ruleset_get( void )
{
    IB_FTRACE_INIT(modua_rules_get);
    if (modua_match_ruleset.num_rules == 0) {
        IB_FTRACE_RET_PTR(modua_match_rule_t, NULL);
    }
    IB_FTRACE_RET_PTR(modua_match_ruleset_t, &modua_match_ruleset);
}
