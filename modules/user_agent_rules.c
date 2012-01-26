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

    /**** Start Match Ruleset ****/

    /*
     * Begin Auto Generated Block
     */

    /* aggregators from "../../data/user-agent-rules/aggregators.txt" */
    {
        /* aggregators.txt line 3 */
        "ag01",
        "aggregators/simplepie",
        {
            { PRODUCT, STARTSWITH, "SimplePie", YES },
            { PLATFORM, CONTAINS, "Feed Parser", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },

    /* bots from "../../data/user-agent-rules/bots.txt" */
    {
        /* bots.txt line 3 */
        "bots01",
        "crawler/yahoo",
        {
            { PRODUCT, STARTSWITH, "YahooSeeker-Testing", YES },
            { PLATFORM, MATCHES, "(compatible; Mozilla 4.0; MSIE 5.5; http://search.yahoo.com/)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 9 */
        "bots02",
        "crawler/yahoo",
        {
            { PRODUCT, MATCHES, "YahooSeeker/1.2", YES },
            { PLATFORM, MATCHES, "(compatible; Mozilla 4.0; MSIE 5.5; yahooseeker at yahoo-inc dot com ; http://help.yahoo.com/help/us/shop/merchant/)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 15 */
        "bots03",
        "crawler/yahoo",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; Yahoo! Slurp China; http://misc.yahoo.com.cn/help.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 21 */
        "bots04",
        "crawler/yahoo",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; Yahoo! Slurp; http://help.yahoo.com/help/us/ysearch/slurp)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 27 */
        "bots05",
        "crawler/newsgator",
        {
            { PRODUCT, MATCHES, "NewsGator/2.5", YES },
            { PLATFORM, MATCHES, "(http://www.newsgator.com; Microsoft Windows NT 5.1.2600.0; .NET CLR 1.1.4322.2032)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 33 */
        "bots06",
        "crawler/newsgator",
        {
            { PRODUCT, MATCHES, "NewsGator/2.0 Bot", YES },
            { PLATFORM, MATCHES, "(http://www.newsgator.com)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 39 */
        "bots07",
        "crawler/netseer",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; NetSeer crawler/2.0; +http://www.netseer.com/crawler.html; crawler@netseer.com)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 45 */
        "bots09",
        "crawler/msnbot",
        {
            { PRODUCT, MATCHES, "msnbot/", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 50 */
        "bots10",
        "crawler/msnbot",
        {
            { PRODUCT, MATCHES, "msnbot/", YES },
            { PLATFORM, MATCHES, "(+http://search.msn.com/msnbot.htm)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 56 */
        "bots11",
        "crawler/msnbot",
        {
            { PRODUCT, MATCHES, "msnbot/0.11", YES },
            { PLATFORM, MATCHES, "( http://search.msn.com/msnbot.htm)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 62 */
        "bots12",
        "crawler/msnbot",
        {
            { PRODUCT, MATCHES, "MSNBOT/0.1", YES },
            { PLATFORM, MATCHES, "(http://search.msn.com/msnbot.htm)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 68 */
        "bots13",
        "crawler/alexia",
        {
            { PRODUCT, STARTSWITH, "ia_archiver", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 73 */
        "bots14",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "Googlebot-Image/1.0", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 78 */
        "bots15",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; Googlebot/2.1; +http://www.google.com/bot.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 84 */
        "bots16",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "Googlebot/2.1", YES },
            { PLATFORM, MATCHES, "(+http://www.googlebot.com/bot.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 90 */
        "bots17",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "Googlebot/2.1", YES },
            { PLATFORM, MATCHES, "(+http://www.google.com/bot.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 96 */
        "bots18",
        "crawler/bing",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; bingbot/2.0; +http://www.bing.com/bingbot.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 102 */
        "bots19",
        "crawler/bing",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; bingbot/2.0 +http://www.bing.com/bingbot.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 108 */
        "bots20",
        "crawler/baidu",
        {
            { PRODUCT, MATCHES, "Baiduspider+(+http://www.baidu.com/search/spider_jp.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 113 */
        "bots21",
        "crawler/baidu",
        {
            { PRODUCT, MATCHES, "Baiduspider+(+http://www.baidu.com/search/spider.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 118 */
        "bots22",
        "crawler/baidu",
        {
            { PRODUCT, MATCHES, "BaiDuSpider", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 123 */
        "bots23",
        "crawler/uptimemonkey",
        {
            { PRODUCT, STARTSWITH, "UptimeMonkey", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 128 */
        "bots24",
        "crawler/nagios",
        {
            { PRODUCT, STARTSWITH, "check_http", YES },
            { PLATFORM, CONTAINS, "nagios-plugins", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 134 */
        "bots25",
        "crawler/pingdom",
        {
            { PRODUCT, STARTSWITH, "Pingdom.com_bot", YES },
            { PLATFORM, MATCHES, "(http://www.pingdom.com)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 140 */
        "bots26",
        "crawler/google",
        {
            { PLATFORM, CONTAINS, "+http://google.com/bot.html", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 145 */
        "bots27",
        "crawler/ahrefs",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(compatible; AhrefsBot", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 151 */
        "bots28",
        "crawler/aboundex",
        {
            { PRODUCT, STARTSWITH, "Aboundex", YES },
            { PLATFORM, CONTAINS, "http://www.aboundex.com/crawler/", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 157 */
        "bots29",
        "crawler/baidu",
        {
            { PRODUCT, STARTSWITH, "Baiduspider-image", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 162 */
        "bots30",
        "crawler/omgilibot",
        {
            { PRODUCT, STARTSWITH, "omgilibot", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 167 */
        "bots31",
        "crawler/msn",
        {
            { PRODUCT, STARTSWITH, "msnbot-media", YES },
            { PLATFORM, MATCHES, "(+http://search.msn.com/msnbot.htm)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 173 */
        "bots32",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "Googlebot-News", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 178 */
        "bots33",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "Googlebot-Video", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 183 */
        "bots34",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "Mediapartners-Google", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 188 */
        "bots35",
        "crawler/google",
        {
            { PRODUCT, MATCHES, "AdsBot-Google", YES },
            { PLATFORM, MATCHES, "(+http://www.google.com/adsbot.html)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 194 */
        "bots36",
        "crawler/soso",
        {
            { PRODUCT, STARTSWITH, "Sosospider", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 199 */
        "bots37",
        "crawler/sogou",
        {
            { PRODUCT, STARTSWITH, "Sogou web spider", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* bots.txt line 204 */
        "bots38",
        "crawler/mj12",
        {
            { PLATFORM, CONTAINS, "MJ12bot/", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },

    /* browsers from "../../data/user-agent-rules/desktop-browsers.txt" */
    {
        /* desktop-browsers.txt line 3 */
        "br01",
        "browser/chrome",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* desktop-browsers.txt line 9 */
        "br02",
        "browser/firefox",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, STARTSWITH, "Gecko", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* desktop-browsers.txt line 15 */
        "br03",
        "browser/msie",
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { PLATFORM, CONTAINS, "MSIE", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* desktop-browsers.txt line 21 */
        "br04",
        "browser/opera",
        {
            { PRODUCT, STARTSWITH, "Opera", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* desktop-browsers.txt line 26 */
        "br05",
        "browser/opera",
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { EXTRA, STARTSWITH, "Opera", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* desktop-browsers.txt line 32 */
        "br06",
        "browser/safari",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Safari", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* desktop-browsers.txt line 38 */
        "br07",
        "browser/lynx",
        {
            { PRODUCT, STARTSWITH, "Lynx/", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },

    /* libraries from "../../data/user-agent-rules/libraries.txt" */
    {
        /* libraries.txt line 3 */
        "lib01",
        "library/binget",
        {
            { PRODUCT, STARTSWITH, "BinGet", YES },
            { PLATFORM, MATCHES, "(http://www.bin-co.com/php/scripts/load/)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 9 */
        "lib02",
        "library/curl",
        {
            { PRODUCT, STARTSWITH, "curl", YES },
            { PLATFORM, EXISTS, "", YES },
            { EXTRA, STARTSWITH, "libcurl", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 16 */
        "lib03",
        "library/java",
        {
            { PRODUCT, STARTSWITH, "java", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 21 */
        "lib04",
        "library/libwww-perl",
        {
            { PRODUCT, STARTSWITH, "libwww-perl", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 26 */
        "lib05",
        "library/MS URL Control",
        {
            { PRODUCT, STARTSWITH, "Microsoft URL Control", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 31 */
        "lib06",
        "library/peach",
        {
            { PRODUCT, STARTSWITH, "Peach", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 36 */
        "lib07",
        "library/php",
        {
            { PRODUCT, STARTSWITH, "PHP", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 41 */
        "lib08",
        "library/pxyscand",
        {
            { PRODUCT, STARTSWITH, "pxyscand", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 46 */
        "lib09",
        "library/PycURL",
        {
            { PRODUCT, STARTSWITH, "PycURL", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 51 */
        "lib10",
        "library/python-urllib",
        {
            { PRODUCT, STARTSWITH, "Python-urllib", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 56 */
        "lib11",
        "library/lwp-trivial",
        {
            { PRODUCT, STARTSWITH, "lwp-trivial", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 61 */
        "lib12",
        "library/wget",
        {
            { PRODUCT, STARTSWITH, "Wget", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 66 */
        "lib13",
        "library/urlgrabber",
        {
            { PRODUCT, STARTSWITH, "urlgrabber", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* libraries.txt line 71 */
        "lib14",
        "library/incutio",
        {
            { PRODUCT, STARTSWITH, "The Incutio XML-RPC", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },

    /* mobile from "../../data/user-agent-rules/mobile.txt" */
    {
        /* mobile.txt line 3 */
        "mob01",
        "mobile/uzard",
        {
            { PRODUCT, MATCHES, "Mozilla/4.0", YES },
            { PLATFORM, CONTAINS, "uZardWeb/1.0", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 9 */
        "mob02",
        "mobile/teleca",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(compatible; Teleca Q7; U; en)", YES },
            { EXTRA, MATCHES, "480X800 LGE VX11000", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 16 */
        "mob03",
        "mobile/teashark",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "Macintosh; U; Intel Mac OS X; en)", YES },
            { EXTRA, CONTAINS, "Shark", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 23 */
        "mob04",
        "mobile/skyfire",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, MATCHES, "(Macintosh; U; Intel Mac OS X 10_5_7; en-us)", YES },
            { EXTRA, CONTAINS, "Skyfire", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 30 */
        "mob05",
        "mobile/semc",
        {
            { PRODUCT, STARTSWITH, "SonyEricsson", YES },
            { EXTRA, CONTAINS, "SEMC-Browser", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 36 */
        "mob06",
        "mobile/opera",
        {
            { PRODUCT, STARTSWITH, "Opera", YES },
            { PLATFORM, CONTAINS, "Opera Mobi", YES },
            { EXTRA, STARTSWITH, "Presto", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 43 */
        "mob07",
        "mobile/opera",
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { PLATFORM, CONTAINS, "Opera Mobi", YES },
            { EXTRA, CONTAINS, "Opera", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 50 */
        "mob08",
        "mobile/netfront",
        {
            { EXTRA, CONTAINS, "NetFront", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 55 */
        "mob09",
        "mobile/minimo",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Minimo", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 61 */
        "mob10",
        "mobile/maemo",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Maemo Browser", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 67 */
        "mob11",
        "mobile/iris",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Iris", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 73 */
        "mob12",
        "mobile/msie mobile",
        {
            { PLATFORM, CONTAINS, "IEMobile", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 78 */
        "mob13",
        "mobile/symbian",
        {
            { PRODUCT, CONTAINS, "GoBrowser", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 83 */
        "mob14",
        "mobile/fennec",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Fennec", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 89 */
        "mob15",
        "mobile/dorothy",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { EXTRA, CONTAINS, "Dorothy", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 95 */
        "mob16",
        "mobile/symbian",
        {
            { PRODUCT, STARTSWITH, "Doris/", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 100 */
        "mob17",
        "mobile/symbian",
        {
            { PRODUCT, CONTAINS, "SymbianOS", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 105 */
        "mob18",
        "mobile/symbian",
        {
            { PLATFORM, CONTAINS, "SymbianOS", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 110 */
        "mob19",
        "mobile/bolt",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, CONTAINS, "BOLT/", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 116 */
        "mob20",
        "mobile/blackberry",
        {
            { PRODUCT, STARTSWITH, "Mozilla", YES },
            { PLATFORM, STARTSWITH, "BlackBerry", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 123 */
        "mob21",
        "mobile/blackberry",
        {
            { PRODUCT, STARTSWITH, "BlackBerry", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 128 */
        "mob22",
        "mobile/android",
        {
            { PRODUCT, STARTSWITH, "Mozilla/5.0", YES },
            { PLATFORM, CONTAINS, "Android", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 135 */
        "mob23",
        "mobile/obigo",
        {
            { PRODUCT, CONTAINS, "Obigo", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 140 */
        "mob24",
        "mobile/iphone",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(iPhone;", YES },
            { EXTRA, STARTSWITH, "AppleWebKit", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 147 */
        "mob25",
        "mobile/ipad",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(iPad;", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 153 */
        "mob26",
        "mobile/qnx",
        {
            { PRODUCT, MATCHES, "Mozilla/5.0", YES },
            { PLATFORM, STARTSWITH, "(Photon", YES },
            { EXTRA, STARTSWITH, "Gecko", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 160 */
        "mob27",
        "mobile/ucweb",
        {
            { PRODUCT, MATCHES, "IUC", YES },
            { EXTRA, CONTAINS, "UCWEB", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 166 */
        "mob28",
        "mobile/jasmine",
        {
            { PRODUCT, CONTAINS, "Jasmine", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 171 */
        "mob29",
        "mobile/maui",
        {
            { PRODUCT, MATCHES, "MAUI WAP Browser", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* mobile.txt line 176 */
        "mob30",
        "mobile/generic",
        {
            { PRODUCT, CONTAINS, "Profile/MIDP", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },

    /* social from "../../data/user-agent-rules/social.txt" */
    {
        /* social.txt line 3 */
        "soc01",
        "social/secondlife",
        {
            { PRODUCT, STARTSWITH, "Second Life", YES },
            { PLATFORM, MATCHES, "(http://secondlife.com)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* social.txt line 9 */
        "soc02",
        "social/secondlife",
        {
            { PRODUCT, MATCHES, "LSL Script", YES },
            { PLATFORM, MATCHES, "(Mozilla Compatible)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* social.txt line 15 */
        "soc03",
        "social/facebook",
        {
            { PRODUCT, STARTSWITH, "facebookexternalhit", YES },
            { PLATFORM, MATCHES, "(+http://www.facebook.com/externalhit_uatext.php)", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },

    /* torrent from "../../data/user-agent-rules/torrent.txt" */
    {
        /* torrent.txt line 3 */
        "tor01",
        "torrent/transmission",
        {
            { PRODUCT, STARTSWITH, "Transmission", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* torrent.txt line 8 */
        "tor02",
        "torrent/uTorrent",
        {
            { PRODUCT, STARTSWITH, "uTorrent", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    {
        /* torrent.txt line 13 */
        "tor03",
        "torrent/rtorrent",
        {
            { PRODUCT, STARTSWITH, "rtorrent", YES },
            { NONE, TERMINATE, "NULL", NO },
        },
    },
    /*
     * End Auto Generated Block
     */

    /**** End Match Ruleset ****/
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
ib_status_t modua_ruleset_init(modua_match_rule_t **failed_rule,
                               unsigned int *failed_field_rule_num)
{
    IB_FTRACE_INIT(modua_ruleset_init);
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
                IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
            }
            if (field_rule_num > MODUA_MAX_FIELD_RULES) {
                *failed_rule           = match_rule;
                *failed_field_rule_num = field_rule_num;
                IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
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
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Get the match rule set */
const modua_match_ruleset_t *modua_ruleset_get( void )
{
    IB_FTRACE_INIT(modua_ruleset_get);
    if (modua_match_ruleset.num_rules == 0) {
        IB_FTRACE_RET_PTR(modua_match_rule_t, NULL);
    }
    IB_FTRACE_RET_PTR(modua_match_ruleset_t, &modua_match_ruleset);
}
