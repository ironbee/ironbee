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
/// @brief IronBee &mdash; Aho Corasick Pattern Matcher provider tests
/// 
/// @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include "ironbee_util_private.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

class TestIBUtilAhoCorasick : public ::testing::Test
{
public:
    TestIBUtilAhoCorasick()
    {
        ib_status_t rc;
        
        ib_initialize();
        
        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create mpool.");
        }
    }
    
    ~TestIBUtilAhoCorasick()
    {
        ib_mpool_destroy(m_pool);
        ib_shutdown();
    }
    
protected:
    ib_mpool_t* m_pool;
};

/* -- Helper Function -- */
void callback(
    ib_ac_t*,
    ib_ac_char_t* pattern,
    size_t        pattern_len,
    void*         data,
    size_t        offset,
    size_t        relative_offset
) {
#ifdef ENABLE_VERBOSE_DEBUG_AHOCORASICK
    const char *d = reinterpret_cast<const char*>(data);
    printf("Call: Matched '%s', len:%d offset:%d rel_offset:%d data %x\n",
           pattern, pattern_len, offset, relative_offset, data);
#endif
}


/* -- Tests -- */

/// @test Parse patterns of the original paper, build_links, match all
TEST_F(TestIBUtilAhoCorasick, generic_ac_test)
{
    ib_status_t rc;
    const char *text = "shershis";
    ib_ac_t *ac_tree = NULL;
    ib_ac_context_t ac_mctx;

    rc = ib_ac_create(&ac_tree, 0, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *)"he", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *)"she", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *)"his", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *)"hers", 0);
    ASSERT_EQ(IB_OK, rc);

    /* Create links and init the matching context */
    rc = ib_ac_build_links(ac_tree);
    ib_ac_init_ctx(&ac_mctx, ac_tree);

    /* Check direct links */
    ASSERT_TRUE(ac_tree->root);
    ASSERT_TRUE(ac_tree->root->child);
    ASSERT_EQ('h', ac_tree->root->child->letter);
    ASSERT_EQ(ac_tree->root, ac_tree->root->child->fail);
    ASSERT_TRUE(ac_tree->root->child->child);
    ASSERT_EQ('e', ac_tree->root->child->child->letter);
    ASSERT_TRUE(ac_tree->root->child->child->flags & 
        IB_AC_FLAG_STATE_OUTPUT);
    ASSERT_TRUE(ac_tree->root->child->child->child);
    ASSERT_EQ('r', ac_tree->root->child->child->child->letter);
    ASSERT_TRUE(ac_tree->root->child->child->child->child);
    ASSERT_EQ('s', ac_tree->root->child->child->child->child->letter);
    ASSERT_FALSE(ac_tree->root->child->child->child->child->child);
    ASSERT_TRUE(ac_tree->root->child->child->child->child->flags &
        IB_AC_FLAG_STATE_OUTPUT);

    /* We reached 'hers' */
    ASSERT_TRUE(ac_tree->root->child->child->sibling);
    ASSERT_EQ('i', ac_tree->root->child->child->sibling->letter);
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child);
    ASSERT_EQ('s', ac_tree->root->child->child->sibling->child->letter);
    ASSERT_FALSE(ac_tree->root->child->child->sibling->child->child);
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->flags &
                IB_AC_FLAG_STATE_OUTPUT);

    /* We reached 'his' */
    ASSERT_TRUE(ac_tree->root->child->sibling);
    ASSERT_EQ('s', ac_tree->root->child->sibling->letter);
    ASSERT_EQ(ac_tree->root, ac_tree->root->child->sibling->fail);
    ASSERT_TRUE(ac_tree->root->child->sibling->child);
    ASSERT_EQ('h', ac_tree->root->child->sibling->child->letter);
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child);
    ASSERT_EQ('e', ac_tree->root->child->sibling->child->child->letter);
    ASSERT_FALSE(ac_tree->root->child->sibling->child->child->child);
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT);

    /* At this point we reached 'she' */
    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(
        &ac_mctx,
        text,
        strlen(text),
        IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
        m_pool
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(ac_mctx.match_list);
    ASSERT_EQ(4UL, ib_list_elements(ac_mctx.match_list));
}

/// @test check that it match a pattern over multiple chunks
TEST_F(TestIBUtilAhoCorasick, test_ib_ac_consume)
{
    ib_status_t rc;
    const char *text = "shershis";
    ib_ac_t *ac_tree = NULL;
    ib_ac_context_t ac_mctx;

    rc = ib_ac_create(&ac_tree, 0, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *)"he", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *)"she", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *)"his", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *)"hers", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_build_links(ac_tree);
    ASSERT_EQ(IB_OK, rc);

    ib_ac_init_ctx(&ac_mctx, ac_tree);

    while (ac_mctx.processed < strlen(text)) {
        /* Call consume with length == 1 byte each (like 1byte chunks) */
        rc = ib_ac_consume(
            &ac_mctx,
            text + ac_mctx.processed,
            1,
            IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
            m_pool
        );
        ASSERT_TRUE(rc == IB_OK || rc == IB_ENOENT);
    }
    
    ASSERT_TRUE(ac_mctx.match_list);
    ASSERT_EQ(4UL, ib_list_elements(ac_mctx.match_list));

    ib_ac_reset_ctx(&ac_mctx, ac_tree);
    ASSERT_EQ(0UL, ib_list_elements(ac_mctx.match_list));

    while (ac_mctx.processed < strlen(text)) {
        /* Call consume with length == 2 byte each (like 1byte chunks) */
        rc = ib_ac_consume(
            &ac_mctx,
            text + ac_mctx.processed,
            2,
            IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
            m_pool
        );
        ASSERT_TRUE(rc == IB_OK || rc == IB_ENOENT);
    }
    
    ASSERT_TRUE(ac_mctx.match_list);
    ASSERT_EQ(4UL, ib_list_elements(ac_mctx.match_list));

    ib_ac_reset_ctx(&ac_mctx, ac_tree);
    ASSERT_EQ(0UL, ib_list_elements(ac_mctx.match_list));

    while (ac_mctx.processed < strlen(text)) {
        /* Call consume with length == 3 byte each (like 1byte chunks) */
        rc = ib_ac_consume(
            &ac_mctx,
            text + ac_mctx.processed,
            3,
            IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
            m_pool
        );

        ASSERT_TRUE(rc == IB_OK || rc == IB_ENOENT);
    }
    
    ASSERT_TRUE(ac_mctx.match_list);
    ASSERT_EQ(4UL, ib_list_elements(ac_mctx.match_list));
}

/// @test Check case insensitive search
TEST_F(TestIBUtilAhoCorasick, ib_ac_consume_case_sensitive)
{
    ib_status_t rc;
    /* Change some letters to capital */
    const char *text = "sHeRsHiS";
    ib_ac_t *ac_tree = NULL;
    ib_ac_context_t ac_mctx;
    
    rc = ib_ac_create(&ac_tree, 0, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *)"he", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *)"she", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *)"his", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *)"hers", 0);
    ASSERT_EQ(IB_OK, rc);

    /* Create links and init the matching context */
    rc = ib_ac_build_links(ac_tree);
    ib_ac_init_ctx(&ac_mctx, ac_tree);

    /* At this point we reached 'she' */
    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(
        &ac_mctx,
        text,
        strlen(text),
        IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
        m_pool
    );

    ASSERT_EQ(IB_ENOENT, rc);
    
    if (ac_mctx.match_list != NULL) {
        ASSERT_NE(0UL, ib_list_elements(ac_mctx.match_list));
    }
}

/// @test Check nocase search
TEST_F(TestIBUtilAhoCorasick, ib_ac_consume_nocase)
{
    ib_status_t rc;
    /* Change some letters to capital */
    const char *text = "sHeRsHiS";
    ib_ac_t *ac_tree = NULL;
    ib_ac_context_t ac_mctx;

    rc = ib_ac_create(&ac_tree, IB_AC_FLAG_PARSER_NOCASE, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *)"he", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *)"she", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *)"his", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *)"hers", 0);
    ASSERT_EQ(IB_OK, rc);

    /* Create links and init the matching context */
    rc = ib_ac_build_links(ac_tree);
    ASSERT_EQ(IB_OK, rc);

    ib_ac_init_ctx(&ac_mctx, ac_tree);


    /* Check direct links */
    ASSERT_TRUE(ac_tree->root);
    ASSERT_TRUE(ac_tree->root->child);
    ASSERT_EQ('h', ac_tree->root->child->letter);
    ASSERT_EQ(ac_tree->root, ac_tree->root->child->fail);
    ASSERT_TRUE(ac_tree->root->child->child);
    ASSERT_EQ('e', ac_tree->root->child->child->letter);
    ASSERT_TRUE(ac_tree->root->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT);
    ASSERT_TRUE(ac_tree->root->child->child->child);
    ASSERT_EQ('r', ac_tree->root->child->child->child->letter);
    ASSERT_TRUE(ac_tree->root->child->child->child->child);
    ASSERT_EQ('s', ac_tree->root->child->child->child->child->letter);
    ASSERT_FALSE(ac_tree->root->child->child->child->child->child);
    ASSERT_TRUE(ac_tree->root->child->child->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT);

    /* We reached 'hers' */
    ASSERT_TRUE(ac_tree->root->child->child->sibling);
    ASSERT_EQ('i', ac_tree->root->child->child->sibling->letter);
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child);
    ASSERT_EQ('s', ac_tree->root->child->child->sibling->child->letter);
    ASSERT_FALSE(ac_tree->root->child->child->sibling->child->child);
    ASSERT_TRUE(ac_tree->root->child->child->sibling->child->flags &
                IB_AC_FLAG_STATE_OUTPUT);

    /* We reached 'his' */
    ASSERT_TRUE(ac_tree->root->child->sibling);
    ASSERT_EQ('s', ac_tree->root->child->sibling->letter);
    ASSERT_EQ(ac_tree->root, ac_tree->root->child->sibling->fail);
    ASSERT_TRUE(ac_tree->root->child->sibling->child);
    ASSERT_EQ('h', ac_tree->root->child->sibling->child->letter);
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child);
    ASSERT_EQ('e', ac_tree->root->child->sibling->child->child->letter);
    ASSERT_FALSE(ac_tree->root->child->sibling->child->child->child);
    ASSERT_TRUE(ac_tree->root->child->sibling->child->child->flags &
                IB_AC_FLAG_STATE_OUTPUT);

    /* At this point we reached 'she' */
    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(
        &ac_mctx,
        text,
        strlen(text),
        IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
        m_pool
    );
    ASSERT_EQ(IB_OK, rc);

    ASSERT_TRUE(ac_mctx.match_list);
    ASSERT_EQ(4UL, ib_list_elements(ac_mctx.match_list));
}

/// @test Check pattern matches of subpatterns in a pattern
TEST_F(TestIBUtilAhoCorasick, ib_ac_consume_multiple_common_prefix)
{
    ib_status_t rc;
    /* Change some letters to capital */
    const char *text = "Aho Corasick is not too expensive for multiple "
                       "pattern matching!";
    ib_ac_t *ac_tree = NULL;
    ib_ac_context_t ac_mctx;

    rc = ib_ac_create(&ac_tree, IB_AC_FLAG_PARSER_NOCASE, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "Expensive", callback,
                           (void *)"Expensive", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "Expen", callback, (void *)"Expen", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "pen", callback, (void *)"pen", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "sive", callback, (void *)"sive", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "ve", callback, (void *)"ve", 0);
    ASSERT_EQ(IB_OK, rc);


    /* Create links and init the matching context */
    rc = ib_ac_build_links(ac_tree);
    ASSERT_EQ(IB_OK, rc);
    ib_ac_init_ctx(&ac_mctx, ac_tree);

    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(
        &ac_mctx,
        text,
        strlen(text),
        IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
        m_pool
    );
    ASSERT_EQ(IB_OK, rc);

    ASSERT_TRUE(ac_mctx.match_list);
    ASSERT_EQ(4UL, ib_list_elements(ac_mctx.match_list));
}

/// @test Check the list of matches
TEST_F(TestIBUtilAhoCorasick, ib_ac_consume_check_list)
{
    ib_status_t rc;
    const char *text = "shershis";
    ib_ac_t *ac_tree = NULL;
    ib_ac_context_t ac_mctx;
    
    rc = ib_ac_create(&ac_tree, 0, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "he", callback, (void *)"he", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "she", callback, (void *)"she", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "his", callback, (void *)"his", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "hers", callback, (void *)"hers", 0);
    ASSERT_EQ(IB_OK, rc);

    /* Create links and init the matching context */
    rc = ib_ac_build_links(ac_tree);
    ib_ac_init_ctx(&ac_mctx, ac_tree);

    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(
        &ac_mctx,
        text,
        strlen(text),
        IB_AC_FLAG_CONSUME_DOLIST | IB_AC_FLAG_CONSUME_MATCHALL,
        m_pool
    );
    ASSERT_EQ(IB_OK, rc);

    ASSERT_TRUE(ac_mctx.match_list);
    ASSERT_EQ(4UL, ib_list_elements(ac_mctx.match_list));

    int miter = 0;
    while (ib_list_elements(ac_mctx.match_list) > 0) {
        ib_ac_match_t *mt = NULL;
        rc = ib_list_dequeue(ac_mctx.match_list, (void *)&mt);
        ASSERT_EQ(IB_OK, rc);

#ifdef ENABLE_VERBOSE_DEBUG_AHOCORASICK
        printf(
            "From list: Pattern:%s, len:%d, offset:%d relative_offset:%d\n",
            mt->pattern, 
            mt->pattern_len,
            mt->offset, 
            mt->relative_offset
        );
#endif
        switch (miter) {
            case 0:
                /* First match should be 'she'*/
                ASSERT_EQ(0, strncmp(mt->pattern, "she", 3));
                ASSERT_EQ(3UL, mt->pattern_len);
                ASSERT_EQ(0UL, mt->offset);
                ASSERT_EQ(0UL, mt->relative_offset);
                break;
            case 1:
                /* First match should be 'he'*/
                ASSERT_EQ(0, strncmp(mt->pattern, "he", 2));
                ASSERT_EQ(2UL, mt->pattern_len);
                ASSERT_EQ(1UL, mt->offset);
                ASSERT_EQ(1UL, mt->relative_offset);
                break;
            case 2:
                /* First match should be 'hers'*/
                ASSERT_EQ(0, strncmp(mt->pattern, "hers", 4));
                ASSERT_EQ(4UL, mt->pattern_len);
                ASSERT_EQ(1UL, mt->offset);
                ASSERT_EQ(1UL, mt->relative_offset);
                break;
            case 3:
                /* First match should be 'his'*/
                ASSERT_EQ(0, strncmp(mt->pattern, "his", 3));
                ASSERT_EQ(3UL, mt->pattern_len);
                ASSERT_EQ(5UL, mt->offset);
                ASSERT_EQ(5UL, mt->relative_offset);
                break;
            default:
                FAIL();
        }
        ++miter;
    }
}

/// @test Check contained patterns
TEST_F(TestIBUtilAhoCorasick, ib_ac_consume_contained_patterns)
{
    ib_status_t rc;
    const char *text = "abcabcabcabc";
    ib_ac_t *ac_tree = NULL;
    ib_ac_context_t ac_mctx;
    
    rc = ib_ac_create(&ac_tree, 0, m_pool);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "abcabcabc", callback,
                           (void *)"abcabcabc", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "abcabc", callback, (void *)"abcabc", 0);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_ac_add_pattern(ac_tree, "abc", callback, (void *)"abc", 0);
    ASSERT_EQ(IB_OK, rc);


    /* Create links and init the matching context */
    rc = ib_ac_build_links(ac_tree);
    ib_ac_init_ctx(&ac_mctx, ac_tree);

    /* Let's test the search. Content is consumed in just one call */
    rc = ib_ac_consume(
        &ac_mctx,
        text,
        strlen(text),
        IB_AC_FLAG_CONSUME_DOLIST |
            IB_AC_FLAG_CONSUME_MATCHALL |
            IB_AC_FLAG_CONSUME_DOCALLBACK,
        m_pool
    );
    ASSERT_EQ(IB_OK, rc);

    ASSERT_TRUE(ac_mctx.match_list != NULL);
    ASSERT_EQ(9UL, ib_list_elements(ac_mctx.match_list));
}
