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
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- User Agent Extraction Module
 *
 * This module extracts the user agent information
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "user_agent_private.h"

#include <ironbee/types.h>

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

/* The actual rules */
static modua_match_ruleset_t modua_match_ruleset =
{
    0,
    {

    /**** Start Match Ruleset ****/

    /*
     * Begin Auto Generated Block
     */

    /* aggregators from "../../data/user-agent-rules/aggregators.txt" */
    {
        /* aggregators.txt line 3 */
        .label = "ag01",
        .category = "aggregators/simplepie",
        .rules = {
            { PRODUCT, STARTSWITH, "SimplePie", YES, 0 },
            { PLATFORM, CONTAINS, "Feed Parser", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },

    /* bots from "../../data/user-agent-rules/bots.txt" */
    {
        /* bots.txt line 3 */
        .label = "bots01",
        .category = "crawler/yahoo",
        .rules = {
            { PRODUCT, STARTSWITH, "YahooSeeker-Testing", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; Mozilla 4.0; MSIE 5.5; http://search.yahoo.com/)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 9 */
        .label = "bots02",
        .category = "crawler/yahoo",
        .rules = {
            { PRODUCT, MATCHES, "YahooSeeker/1.2", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; Mozilla 4.0; MSIE 5.5; yahooseeker at yahoo-inc dot com ; http://help.yahoo.com/help/us/shop/merchant/)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 15 */
        .label = "bots03",
        .category = "crawler/yahoo",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; Yahoo! Slurp China; http://misc.yahoo.com.cn/help.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 21 */
        .label = "bots04",
        .category = "crawler/yahoo",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; Yahoo! Slurp; http://help.yahoo.com/help/us/ysearch/slurp)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 27 */
        .label = "bots05",
        .category = "crawler/newsgator",
        .rules = {
            { PRODUCT, MATCHES, "NewsGator/2.5", YES, 0 },
            { PLATFORM, MATCHES, "(http://www.newsgator.com; Microsoft Windows NT 5.1.2600.0; .NET CLR 1.1.4322.2032)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 33 */
        .label = "bots06",
        .category = "crawler/newsgator",
        .rules = {
            { PRODUCT, MATCHES, "NewsGator/2.0 Bot", YES, 0 },
            { PLATFORM, MATCHES, "(http://www.newsgator.com)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 39 */
        .label = "bots07",
        .category = "crawler/netseer",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; NetSeer crawler/2.0; +http://www.netseer.com/crawler.html; crawler@netseer.com)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 45 */
        .label = "bots09",
        .category = "crawler/msnbot",
        .rules = {
            { PRODUCT, MATCHES, "msnbot/", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 50 */
        .label = "bots10",
        .category = "crawler/msnbot",
        .rules = {
            { PRODUCT, MATCHES, "msnbot/", YES, 0 },
            { PLATFORM, MATCHES, "(+http://search.msn.com/msnbot.htm)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 56 */
        .label = "bots11",
        .category = "crawler/msnbot",
        .rules = {
            { PRODUCT, MATCHES, "msnbot/0.11", YES, 0 },
            { PLATFORM, MATCHES, "( http://search.msn.com/msnbot.htm)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 62 */
        .label = "bots12",
        .category = "crawler/msnbot",
        .rules = {
            { PRODUCT, MATCHES, "MSNBOT/0.1", YES, 0 },
            { PLATFORM, MATCHES, "(http://search.msn.com/msnbot.htm)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 68 */
        .label = "bots13",
        .category = "crawler/alexia",
        .rules = {
            { PRODUCT, STARTSWITH, "ia_archiver", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 73 */
        .label = "bots14",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "Googlebot-Image/1.0", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 78 */
        .label = "bots15",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; Googlebot/2.1; +http://www.google.com/bot.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 84 */
        .label = "bots16",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "Googlebot/2.1", YES, 0 },
            { PLATFORM, MATCHES, "(+http://www.googlebot.com/bot.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 90 */
        .label = "bots17",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "Googlebot/2.1", YES, 0 },
            { PLATFORM, MATCHES, "(+http://www.google.com/bot.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 96 */
        .label = "bots18",
        .category = "crawler/bing",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; bingbot/2.0; +http://www.bing.com/bingbot.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 102 */
        .label = "bots19",
        .category = "crawler/bing",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; bingbot/2.0 +http://www.bing.com/bingbot.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 108 */
        .label = "bots20",
        .category = "crawler/baidu",
        .rules = {
            { PRODUCT, MATCHES, "Baiduspider+(+http://www.baidu.com/search/spider_jp.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 113 */
        .label = "bots21",
        .category = "crawler/baidu",
        .rules = {
            { PRODUCT, MATCHES, "Baiduspider+(+http://www.baidu.com/search/spider.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 118 */
        .label = "bots22",
        .category = "crawler/baidu",
        .rules = {
            { PRODUCT, MATCHES, "BaiDuSpider", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 123 */
        .label = "bots23",
        .category = "crawler/uptimemonkey",
        .rules = {
            { PRODUCT, STARTSWITH, "UptimeMonkey", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 128 */
        .label = "bots24",
        .category = "crawler/nagios",
        .rules = {
            { PRODUCT, STARTSWITH, "check_http", YES, 0 },
            { PLATFORM, CONTAINS, "nagios-plugins", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 134 */
        .label = "bots25",
        .category = "crawler/pingdom",
        .rules = {
            { PRODUCT, STARTSWITH, "Pingdom.com_bot", YES, 0 },
            { PLATFORM, MATCHES, "(http://www.pingdom.com)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 140 */
        .label = "bots26",
        .category = "crawler/google",
        .rules = {
            { PLATFORM, CONTAINS, "+http://google.com/bot.html", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 145 */
        .label = "bots27",
        .category = "crawler/ahrefs",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, STARTSWITH, "(compatible; AhrefsBot", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 151 */
        .label = "bots28",
        .category = "crawler/aboundex",
        .rules = {
            { PRODUCT, STARTSWITH, "Aboundex", YES, 0 },
            { PLATFORM, CONTAINS, "http://www.aboundex.com/crawler/", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 157 */
        .label = "bots29",
        .category = "crawler/baidu",
        .rules = {
            { PRODUCT, STARTSWITH, "Baiduspider-image", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 162 */
        .label = "bots30",
        .category = "crawler/omgilibot",
        .rules = {
            { PRODUCT, STARTSWITH, "omgilibot", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 167 */
        .label = "bots31",
        .category = "crawler/msn",
        .rules = {
            { PRODUCT, STARTSWITH, "msnbot-media", YES, 0 },
            { PLATFORM, MATCHES, "(+http://search.msn.com/msnbot.htm)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 173 */
        .label = "bots32",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "Googlebot-News", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 178 */
        .label = "bots33",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "Googlebot-Video", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 183 */
        .label = "bots34",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "Mediapartners-Google", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 188 */
        .label = "bots35",
        .category = "crawler/google",
        .rules = {
            { PRODUCT, MATCHES, "AdsBot-Google", YES, 0 },
            { PLATFORM, MATCHES, "(+http://www.google.com/adsbot.html)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 194 */
        .label = "bots36",
        .category = "crawler/soso",
        .rules = {
            { PRODUCT, STARTSWITH, "Sosospider", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 199 */
        .label = "bots37",
        .category = "crawler/sogou",
        .rules = {
            { PRODUCT, STARTSWITH, "Sogou web spider", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* bots.txt line 204 */
        .label = "bots38",
        .category = "crawler/mj12",
        .rules = {
            { PLATFORM, CONTAINS, "MJ12bot/", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },

    /* browsers from "../../data/user-agent-rules/desktop-browsers.txt" */
    {
        /* desktop-browsers.txt line 3 */
        .label = "br01",
        .category = "browser/chrome",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, STARTSWITH, "AppleWebKit", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* desktop-browsers.txt line 9 */
        .label = "br02",
        .category = "browser/firefox",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, STARTSWITH, "Gecko", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* desktop-browsers.txt line 15 */
        .label = "br03",
        .category = "browser/msie",
        .rules = {
            { PRODUCT, STARTSWITH, "Mozilla", YES, 0 },
            { PLATFORM, CONTAINS, "MSIE", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* desktop-browsers.txt line 21 */
        .label = "br04",
        .category = "browser/opera",
        .rules = {
            { PRODUCT, STARTSWITH, "Opera", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* desktop-browsers.txt line 26 */
        .label = "br05",
        .category = "browser/opera",
        .rules = {
            { PRODUCT, STARTSWITH, "Mozilla", YES, 0 },
            { EXTRA, STARTSWITH, "Opera", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* desktop-browsers.txt line 32 */
        .label = "br06",
        .category = "browser/safari",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, CONTAINS, "Safari", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* desktop-browsers.txt line 38 */
        .label = "br07",
        .category = "browser/lynx",
        .rules = {
            { PRODUCT, STARTSWITH, "Lynx/", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },

    /* libraries from "../../data/user-agent-rules/libraries.txt" */
    {
        /* libraries.txt line 3 */
        .label = "lib01",
        .category = "library/binget",
        .rules = {
            { PRODUCT, STARTSWITH, "BinGet", YES, 0 },
            { PLATFORM, MATCHES, "(http://www.bin-co.com/php/scripts/load/)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 9 */
        .label = "lib02",
        .category = "library/curl",
        .rules = {
            { PRODUCT, STARTSWITH, "curl", YES, 0 },
            { PLATFORM, EXISTS, "", YES, 0 },
            { EXTRA, STARTSWITH, "libcurl", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 16 */
        .label = "lib03",
        .category = "library/java",
        .rules = {
            { PRODUCT, STARTSWITH, "java", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 21 */
        .label = "lib04",
        .category = "library/libwww-perl",
        .rules = {
            { PRODUCT, STARTSWITH, "libwww-perl", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 26 */
        .label = "lib05",
        .category = "library/MS URL Control",
        .rules = {
            { PRODUCT, STARTSWITH, "Microsoft URL Control", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 31 */
        .label = "lib06",
        .category = "library/peach",
        .rules = {
            { PRODUCT, STARTSWITH, "Peach", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 36 */
        .label = "lib07",
        .category = "library/php",
        .rules = {
            { PRODUCT, STARTSWITH, "PHP", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 41 */
        .label = "lib08",
        .category = "library/pxyscand",
        .rules = {
            { PRODUCT, STARTSWITH, "pxyscand", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 46 */
        .label = "lib09",
        .category = "library/PycURL",
        .rules = {
            { PRODUCT, STARTSWITH, "PycURL", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 51 */
        .label = "lib10",
        .category = "library/python-urllib",
        .rules = {
            { PRODUCT, STARTSWITH, "Python-urllib", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 56 */
        .label = "lib11",
        .category = "library/lwp-trivial",
        .rules = {
            { PRODUCT, STARTSWITH, "lwp-trivial", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 61 */
        .label = "lib12",
        .category = "library/wget",
        .rules = {
            { PRODUCT, STARTSWITH, "Wget", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 66 */
        .label = "lib13",
        .category = "library/urlgrabber",
        .rules = {
            { PRODUCT, STARTSWITH, "urlgrabber", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* libraries.txt line 71 */
        .label = "lib14",
        .category = "library/incutio",
        .rules = {
            { PRODUCT, STARTSWITH, "The Incutio XML-RPC", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },

    /* mobile from "../../data/user-agent-rules/mobile.txt" */
    {
        /* mobile.txt line 3 */
        .label = "mob01",
        .category = "mobile/uzard",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/4.0", YES, 0 },
            { PLATFORM, CONTAINS, "uZardWeb/1.0", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 9 */
        .label = "mob02",
        .category = "mobile/teleca",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(compatible; Teleca Q7; U; en)", YES, 0 },
            { EXTRA, MATCHES, "480X800 LGE VX11000", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 16 */
        .label = "mob03",
        .category = "mobile/teashark",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "Macintosh; U; Intel Mac OS X; en)", YES, 0 },
            { EXTRA, CONTAINS, "Shark", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 23 */
        .label = "mob04",
        .category = "mobile/skyfire",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, MATCHES, "(Macintosh; U; Intel Mac OS X 10_5_7; en-us)", YES, 0 },
            { EXTRA, CONTAINS, "Skyfire", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 30 */
        .label = "mob05",
        .category = "mobile/semc",
        .rules = {
            { PRODUCT, STARTSWITH, "SonyEricsson", YES, 0 },
            { EXTRA, CONTAINS, "SEMC-Browser", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 36 */
        .label = "mob06",
        .category = "mobile/opera",
        .rules = {
            { PRODUCT, STARTSWITH, "Opera", YES, 0 },
            { PLATFORM, CONTAINS, "Opera Mobi", YES, 0 },
            { EXTRA, STARTSWITH, "Presto", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 43 */
        .label = "mob07",
        .category = "mobile/opera",
        .rules = {
            { PRODUCT, STARTSWITH, "Mozilla", YES, 0 },
            { PLATFORM, CONTAINS, "Opera Mobi", YES, 0 },
            { EXTRA, CONTAINS, "Opera", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 50 */
        .label = "mob08",
        .category = "mobile/netfront",
        .rules = {
            { EXTRA, CONTAINS, "NetFront", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 55 */
        .label = "mob09",
        .category = "mobile/minimo",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, CONTAINS, "Minimo", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 61 */
        .label = "mob10",
        .category = "mobile/maemo",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, CONTAINS, "Maemo Browser", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 67 */
        .label = "mob11",
        .category = "mobile/iris",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, CONTAINS, "Iris", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 73 */
        .label = "mob12",
        .category = "mobile/msie mobile",
        .rules = {
            { PLATFORM, CONTAINS, "IEMobile", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 78 */
        .label = "mob13",
        .category = "mobile/symbian",
        .rules = {
            { PRODUCT, CONTAINS, "GoBrowser", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 83 */
        .label = "mob14",
        .category = "mobile/fennec",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, CONTAINS, "Fennec", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 89 */
        .label = "mob15",
        .category = "mobile/dorothy",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { EXTRA, CONTAINS, "Dorothy", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 95 */
        .label = "mob16",
        .category = "mobile/symbian",
        .rules = {
            { PRODUCT, STARTSWITH, "Doris/", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 100 */
        .label = "mob17",
        .category = "mobile/symbian",
        .rules = {
            { PRODUCT, CONTAINS, "SymbianOS", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 105 */
        .label = "mob18",
        .category = "mobile/symbian",
        .rules = {
            { PLATFORM, CONTAINS, "SymbianOS", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 110 */
        .label = "mob19",
        .category = "mobile/bolt",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, CONTAINS, "BOLT/", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 116 */
        .label = "mob20",
        .category = "mobile/blackberry",
        .rules = {
            { PRODUCT, STARTSWITH, "Mozilla", YES, 0 },
            { PLATFORM, STARTSWITH, "BlackBerry", YES, 0 },
            { EXTRA, STARTSWITH, "AppleWebKit", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 123 */
        .label = "mob21",
        .category = "mobile/blackberry",
        .rules = {
            { PRODUCT, STARTSWITH, "BlackBerry", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 128 */
        .label = "mob22",
        .category = "mobile/android",
        .rules = {
            { PRODUCT, STARTSWITH, "Mozilla/5.0", YES, 0 },
            { PLATFORM, CONTAINS, "Android", YES, 0 },
            { EXTRA, STARTSWITH, "AppleWebKit", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 135 */
        .label = "mob23",
        .category = "mobile/obigo",
        .rules = {
            { PRODUCT, CONTAINS, "Obigo", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 140 */
        .label = "mob24",
        .category = "mobile/iphone",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, STARTSWITH, "(iPhone;", YES, 0 },
            { EXTRA, STARTSWITH, "AppleWebKit", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 147 */
        .label = "mob25",
        .category = "mobile/ipad",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, STARTSWITH, "(iPad;", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 153 */
        .label = "mob26",
        .category = "mobile/qnx",
        .rules = {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES, 0 },
            { PLATFORM, STARTSWITH, "(Photon", YES, 0 },
            { EXTRA, STARTSWITH, "Gecko", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 160 */
        .label = "mob27",
        .category = "mobile/ucweb",
        .rules = {
            { PRODUCT, MATCHES, "IUC", YES, 0 },
            { EXTRA, CONTAINS, "UCWEB", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 166 */
        .label = "mob28",
        .category = "mobile/jasmine",
        .rules = {
            { PRODUCT, CONTAINS, "Jasmine", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 171 */
        .label = "mob29",
        .category = "mobile/maui",
        .rules = {
            { PRODUCT, MATCHES, "MAUI WAP Browser", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* mobile.txt line 176 */
        .label = "mob30",
        .category = "mobile/generic",
        .rules = {
            { PRODUCT, CONTAINS, "Profile/MIDP", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },

    /* social from "../../data/user-agent-rules/social.txt" */
    {
        /* social.txt line 3 */
        .label = "soc01",
        .category = "social/secondlife",
        .rules = {
            { PRODUCT, STARTSWITH, "Second Life", YES, 0 },
            { PLATFORM, MATCHES, "(http://secondlife.com)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* social.txt line 9 */
        .label = "soc02",
        .category = "social/secondlife",
        .rules = {
            { PRODUCT, MATCHES, "LSL Script", YES, 0 },
            { PLATFORM, MATCHES, "(Mozilla Compatible)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* social.txt line 15 */
        .label = "soc03",
        .category = "social/facebook",
        .rules = {
            { PRODUCT, STARTSWITH, "facebookexternalhit", YES, 0 },
            { PLATFORM, MATCHES, "(+http://www.facebook.com/externalhit_uatext.php)", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },

    /* torrent from "../../data/user-agent-rules/torrent.txt" */
    {
        /* torrent.txt line 3 */
        .label = "tor01",
        .category = "torrent/transmission",
        .rules = {
            { PRODUCT, STARTSWITH, "Transmission", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* torrent.txt line 8 */
        .label = "tor02",
        .category = "torrent/uTorrent",
        .rules = {
            { PRODUCT, STARTSWITH, "uTorrent", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    {
        /* torrent.txt line 13 */
        .label = "tor03",
        .category = "torrent/rtorrent",
        .rules = {
            { PRODUCT, STARTSWITH, "rtorrent", YES, 0 },
            { NONE, TERMINATE, "NULL", NO, 0 },
        },
    },
    /*
     * End Auto Generated Block
     */

    /**** End Match Ruleset ****/
    }
};

/**
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
    if (rule->string != NULL) {
        rule->slen = strlen(rule->string);
    }
    return IB_OK;
}

/* Initialize the static rules */
ib_status_t modua_ruleset_init(modua_match_rule_t **failed_rule,
                               unsigned int *failed_field_rule_num)
{
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
                *failed_rule           = match_rule;
                *failed_field_rule_num = field_rule_num;
                return IB_EUNKNOWN;
            }
            if (field_rule_num > MODUA_MAX_FIELD_RULES) {
                *failed_rule           = match_rule;
                *failed_field_rule_num = field_rule_num;
                return IB_EUNKNOWN;
            }
            ++match_rule->num_rules;
        }

        /* Update the match rule count */
        ++modua_match_ruleset.num_rules;
    }

    /* No failures */
    *failed_rule = NULL;
    *failed_field_rule_num = 0;

    /* Done */
    return IB_OK;
}

/* Get the match rule set */
const modua_match_ruleset_t *modua_ruleset_get( void )
{
    if (modua_match_ruleset.num_rules == 0) {
        return NULL;
    }
    return &modua_match_ruleset;
}
