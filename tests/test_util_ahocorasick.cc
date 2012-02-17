//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee - Aho Corasick Pattern Matcher provider tests
/// 
/// @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "util/util.c"
#include "util/mpool.c"
#include "util/debug.c"

/* -- Helper Function -- */
void callback(ib_ac_t *ac_tree,
              ib_ac_char_t *pattern,
              size_t pattern_len,
              void *data,
              size_t offset,
              size_t relative_offset)
{
#if 0
    char *d = (char *) data;
    printf("Call: Matched '%s', len:%d offset:%d rel_offset:%d data %x\n",
           pattern, pattern_len, offset, relative_offset, data);
#endif
}


/* -- Tests -- */

/// @test Parse patterns of the original paper, build_links, match all
TEST(TestIBUtilAhoCorasick, generic_ac_test)
{
    ib_mpool_t *pool = NULL;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_mpool_create(&pool, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    const char *text = "shershis";

    ib_ac_t *ac_tree = NULL;

    rc = ib_ac_create(&ac_tree, 0, pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_create() failed "
                                "- rc != IB_OK";

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *) "he", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *) "she", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *) "his", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *) "hers", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";

    /* Create links and init the matching context */
    ib_ac_context_t ac_mctx;
    rc = ib_ac_build_links(ac_tree);

    ib_ac_init_ctx(&ac_mctx, ac_tree);


    /* Check direct links */
    ASSERT_TRUE(ac_tree->root != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid root" ;
    ASSERT_TRUE(ac_tree->root->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->letter == 'h')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->fail == ac_tree->root)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->letter == 'e')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;
    ASSERT_TRUE(ac_tree->root->child->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->letter == 'r')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child->letter == 's')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child->child == NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;

    /* We reached 'hers' */

    ASSERT_TRUE(ac_tree->root->child->child->sibling != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid sibling (expect 'i')";
    ASSERT_TRUE(ac_tree->root->child->child->sibling->letter == 'i')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid sibling letter";
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->letter == 's')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->child == NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;

    /* We reached 'his' */

    ASSERT_TRUE(ac_tree->root->child->sibling != NULL)
                              << "ib_ac_add_pattern() "
                                "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->letter == 's')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->fail == ac_tree->root)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->letter == 'h')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->letter == 'e')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->child == NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;

    /* At this point we reached 'she' */

    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(&ac_mctx,
                       text,
                       strlen(text),
                       IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
                       pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_consume() "
                           "failed - rc != IB_OK" ;

    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 4)<<"ib_ac_consume()"
               "failed - The number of elements matched should be 4";


    ib_mpool_destroy(pool);
}

/// @test check that it match a pattern over multiple chunks
TEST(TestIBUtilAhoCorasick, test_ib_ac_consume)
{
    ib_mpool_t *pool = NULL;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_mpool_create(&pool, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    const char *text = "shershis";

    ib_ac_t *ac_tree = NULL;

    rc = ib_ac_create(&ac_tree, 0, pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_create() failed "
                                "- rc != IB_OK";

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *) "he", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *) "she", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *) "his", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *) "hers", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";

    ib_ac_context_t ac_mctx;
    rc = ib_ac_build_links(ac_tree);
    ASSERT_EQ(IB_OK, rc);
    
    ib_ac_init_ctx(&ac_mctx, ac_tree);

    while (ac_mctx.processed < strlen(text)) {
        /* Call consume with length == 1 byte each (like 1byte chunks) */
        rc = ib_ac_consume(&ac_mctx,
                           text + ac_mctx.processed,
                           1,
                           IB_AC_FLAG_CONSUME_DOLIST |
                           IB_AC_FLAG_CONSUME_MATCHALL,
                           pool);

        ASSERT_TRUE(rc == IB_OK || rc == IB_ENOENT) << "ib_ac_consume() "
                               "failed - rc != IB_OK && rc != IB_ENOENT" ;
    }
    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 4)<<"ib_ac_consume()"
               "failed - The number of elements matched should be 4";

    ib_ac_reset_ctx(&ac_mctx, ac_tree);
    ASSERT_TRUE(ib_list_elements(ac_mctx.match_list) == 0)
                << "ib_ac_consume() failed - The number of elements should"
                   " be reset to 0";

    while (ac_mctx.processed < strlen(text)) {
        /* Call consume with length == 2 byte each (like 1byte chunks) */
        rc = ib_ac_consume(&ac_mctx,
                           text + ac_mctx.processed,
                           2,
                           IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
                           pool);

        ASSERT_TRUE(rc == IB_OK || rc == IB_ENOENT) << "ib_ac_consume() "
                               "failed - rc != IB_OK && rc != IB_ENOENT" ;
    }
    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 4)<<"ib_ac_consume()"
               "failed - The number of elements matched should be 4";

    ib_ac_reset_ctx(&ac_mctx, ac_tree);
    ASSERT_TRUE(ib_list_elements(ac_mctx.match_list) == 0)
                << "ib_ac_consume() failed - The number of elements matched"
                   " should be reset to 0";

    while (ac_mctx.processed < strlen(text)) {
        /* Call consume with length == 3 byte each (like 1byte chunks) */
        rc = ib_ac_consume(&ac_mctx,
                           text + ac_mctx.processed,
                           3,
                           IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
                           pool);

        ASSERT_TRUE(rc == IB_OK || rc == IB_ENOENT) << "ib_ac_consume() "
                               "failed - rc != IB_OK && rc != IB_ENOENT" ;
    }
    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 4)<<"ib_ac_consume()"
               "failed - The number of elements matched should be 4";

    ib_mpool_destroy(pool);
}

/// @test Check case insensitive search
TEST(TestIBUtilAhoCorasick, ib_ac_consume_case_sensitive)
{
    ib_mpool_t *pool = NULL;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_mpool_create(&pool, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    /* Change some letters to capital */
    const char *text = "sHeRsHiS";

    ib_ac_t *ac_tree = NULL;

    rc = ib_ac_create(&ac_tree, 0, pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_create() failed "
                                "- rc != IB_OK";

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *) "he", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *) "she", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *) "his", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *) "hers", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";

    /* Create links and init the matching context */
    ib_ac_context_t ac_mctx;
    rc = ib_ac_build_links(ac_tree);

    ib_ac_init_ctx(&ac_mctx, ac_tree);

    /* At this point we reached 'she' */

    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(&ac_mctx,
                       text,
                       strlen(text),
                       IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
                       pool);

    ASSERT_TRUE(rc == IB_ENOENT) << "ib_ac_consume() "
                           "failed - rc != IB_ENOENT" ;

    ASSERT_TRUE(ac_mctx.match_list == NULL ||
               ib_list_elements(ac_mctx.match_list) != 0)<<"ib_ac_consume()"
               "failed - The number of elements should be 0 as is sensitive"
               " case";


    ib_mpool_destroy(pool);
}

/// @test Check nocase search
TEST(TestIBUtilAhoCorasick, ib_ac_consume_nocase)
{
    ib_mpool_t *pool = NULL;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_mpool_create(&pool, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    /* Change some letters to capital */
    const char *text = "sHeRsHiS";

    ib_ac_t *ac_tree = NULL;

    rc = ib_ac_create(&ac_tree, IB_AC_FLAG_PARSER_NOCASE, pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_create() failed "
                                "- rc != IB_OK";

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *) "he", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *) "she", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *) "his", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *) "hers", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";

    /* Create links and init the matching context */
    ib_ac_context_t ac_mctx;
    rc = ib_ac_build_links(ac_tree);
    ASSERT_EQ(IB_OK, rc);

    ib_ac_init_ctx(&ac_mctx, ac_tree);


    /* Check direct links */
    ASSERT_TRUE(ac_tree->root != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid root" ;
    ASSERT_TRUE(ac_tree->root->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->letter == 'h')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->fail == ac_tree->root)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->letter == 'e')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;
    ASSERT_TRUE(ac_tree->root->child->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->letter == 'r')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child->letter == 's')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child->child == NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;

    /* We reached 'hers' */

    ASSERT_TRUE(ac_tree->root->child->child->sibling != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid sibling (expect 'i')";
    ASSERT_TRUE(ac_tree->root->child->child->sibling->letter == 'i')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid sibling letter";
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->letter == 's')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->child == NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;

    /* We reached 'his' */

    ASSERT_TRUE(ac_tree->root->child->sibling != NULL)
                              << "ib_ac_add_pattern() "
                                "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->letter == 's')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->fail == ac_tree->root)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->letter == 'h')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child != NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->letter == 'e')
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child letter" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->child == NULL)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid child" ;
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT)
                              << "ib_ac_add_pattern() "
                                 "failed - invalid flags" ;

    /* At this point we reached 'she' */

    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(&ac_mctx,
                       text,
                       strlen(text),
                       IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
                       pool);

    ASSERT_TRUE(rc == IB_OK) << "ib_ac_consume() "
                           "failed - rc != IB_OK && rc != IB_ENOENT" ;

    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 4)<<"ib_ac_consume()"
               "failed - The number of elements matched is 4";


    ib_mpool_destroy(pool);
}

/// @test Check pattern matches of subpatterns in a pattern
TEST(TestIBUtilAhoCorasick, ib_ac_consume_multiple_common_prefix)
{
    ib_mpool_t *pool = NULL;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_mpool_create(&pool, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    /* Change some letters to capital */
    const char *text = "Aho Corasick is not too expensive for multiple "
                       "pattern matching!";

    ib_ac_t *ac_tree = NULL;

    rc = ib_ac_create(&ac_tree, IB_AC_FLAG_PARSER_NOCASE, pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_create() failed "
                                "- rc != IB_OK";

    rc = ib_ac_add_pattern(ac_tree, "Expensive", callback,
                           (void *) "Expensive", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "Expen", callback, (void *) "Expen", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "pen", callback, (void *) "pen", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "sive", callback, (void *) "sive", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "ve", callback, (void *) "ve", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";

    /* Create links and init the matching context */
    ib_ac_context_t ac_mctx;
    rc = ib_ac_build_links(ac_tree);
    ASSERT_EQ(IB_OK, rc);

    ib_ac_init_ctx(&ac_mctx, ac_tree);


    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(&ac_mctx,
                       text,
                       strlen(text),
                       IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
                       pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_consume() "
                           "failed - rc != IB_OK" ;

    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 4)<<"ib_ac_consume()"
               "failed - The number of elements matched is 4";


    ib_mpool_destroy(pool);
}

/// @test Check the list of matches
TEST(TestIBUtilAhoCorasick, ib_ac_consume_check_list)
{
    ib_mpool_t *pool = NULL;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_mpool_create(&pool, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    const char *text = "shershis";

    ib_ac_t *ac_tree = NULL;

    rc = ib_ac_create(&ac_tree, 0, pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_create() failed "
                                "- rc != IB_OK";

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *) "he", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *) "she", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *) "his", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *) "hers", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";

    /* Create links and init the matching context */
    ib_ac_context_t ac_mctx;
    rc = ib_ac_build_links(ac_tree);

    ib_ac_init_ctx(&ac_mctx, ac_tree);

    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(&ac_mctx,
                       text,
                       strlen(text),
                       IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
                       pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_consume() "
                           "failed - rc != IB_OK" ;

    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 4)<<"ib_ac_consume()"
               "failed - The number of elements matched is 4";

    int miter = 0;
    while (ib_list_elements(ac_mctx.match_list) > 0) {
        ib_ac_match_t *mt = NULL;
        rc = ib_list_dequeue(ac_mctx.match_list, (void *)&mt);
        ASSERT_TRUE(rc == IB_OK) << "ib_list_dequeue() failed - Failed "
                                    "dequeuing ac matches";
#if 0
        printf("From list: Pattern:%s, len:%d, offset:%d "
                 "relative_offset:%d\n", mt->pattern, mt->pattern_len,
                 mt->offset, mt->relative_offset);
#endif
        switch (miter) {
            case 0:
            /* First match should be 'she'*/
            ASSERT_TRUE(strncmp(mt->pattern, "she", 3) == 0)
                        << "ib_ac_consume() failed - unexpected pattern (or"
                           " pattern order) found in the list";
            ASSERT_TRUE(mt->pattern_len == 3)
                        << "ib_ac_consume() failed - unexpected pattern len"
                           " found";
            ASSERT_TRUE(mt->offset == 0)
                        << "ib_ac_consume() failed - unexpected offset";
            ASSERT_TRUE(mt->relative_offset == 0)
                        << "ib_ac_consume() failed - unexpected "
                           "relative offset";
            break;
            case 1:
            /* First match should be 'he'*/
            ASSERT_TRUE(strncmp(mt->pattern, "he", 2) == 0)
                        << "ib_ac_consume() failed - unexpected pattern (or"
                           " pattern order) found in the list";
            ASSERT_TRUE(mt->pattern_len == 2)
                        << "ib_ac_consume() failed - unexpected pattern len"
                           " found";
            ASSERT_TRUE(mt->offset == 1)
                        << "ib_ac_consume() failed - unexpected offset";
            ASSERT_TRUE(mt->relative_offset == 1)
                        << "ib_ac_consume() failed - unexpected "
                           "relative offset";
            break;
            case 2:
            /* First match should be 'hers'*/
            ASSERT_TRUE(strncmp(mt->pattern, "hers", 4) == 0)
                        << "ib_ac_consume() failed - unexpected pattern (or"
                           " pattern order) found in the list";
            ASSERT_TRUE(mt->pattern_len == 4)
                        << "ib_ac_consume() failed - unexpected pattern len"
                           " found";
            ASSERT_TRUE(mt->offset == 1)
                        << "ib_ac_consume() failed - unexpected offset";
            ASSERT_TRUE(mt->relative_offset == 1)
                        << "ib_ac_consume() failed - unexpected relative "
                           "offset";
            break;
            case 3:
            /* First match should be 'his'*/
            ASSERT_TRUE(strncmp(mt->pattern, "his", 3) == 0)
                        << "ib_ac_consume() failed - unexpected pattern (or"
                           " pattern order) found in the list";
            ASSERT_TRUE(mt->pattern_len == 3)
                        << "ib_ac_consume() failed - unexpected pattern len"
                           " found";
            ASSERT_TRUE(mt->offset == 5)
                        << "ib_ac_consume() failed - unexpected offset";
            ASSERT_TRUE(mt->relative_offset == 5)
                        << "ib_ac_consume() failed - unexpected relative "
                           "offset";
            break;
        }
        miter++;
    }

    ib_mpool_destroy(pool);
}

/// @test Check contained patterns
TEST(TestIBUtilAhoCorasick, ib_ac_consume_contained_patterns)
{
    ib_mpool_t *pool = NULL;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";

    rc = ib_mpool_create(&pool, NULL, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";

    const char *text = "abcabcabcabc";

    ib_ac_t *ac_tree = NULL;

    rc = ib_ac_create(&ac_tree, 0, pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_create() failed "
                                "- rc != IB_OK";

    rc = ib_ac_add_pattern(ac_tree, "abcabcabc", callback,
                           (void *) "abcabcabc", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "abcabc", callback, (void *) "abcabc", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";
    rc = ib_ac_add_pattern(ac_tree, "abc", callback, (void *) "abc", 0);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_add_pattern() failed "
                                "- rc != IB_OK";

    /* Create links and init the matching context */
    ib_ac_context_t ac_mctx;
    rc = ib_ac_build_links(ac_tree);

    ib_ac_init_ctx(&ac_mctx, ac_tree);


    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(&ac_mctx,
                       text,
                       strlen(text),
                       IB_AC_FLAG_CONSUME_DOLIST |
                       IB_AC_FLAG_CONSUME_MATCHALL |
                       IB_AC_FLAG_CONSUME_DOCALLBACK,
                       pool);
    ASSERT_TRUE(rc == IB_OK) << "ib_ac_consume() "
                           "failed - rc != IB_OK" ;

    ASSERT_TRUE(ac_mctx.match_list != NULL &&
               ib_list_elements(ac_mctx.match_list) == 9)<<"ib_ac_consume()"
               "failed - The number of elements matched should be 9";


    ib_mpool_destroy(pool);
}
