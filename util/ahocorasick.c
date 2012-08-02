/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.    See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.    You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 * @brief IronBee &mdash; Utility Aho Corasick Pattern Matcher
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

/**
 * Implementation of Aho Corasick
 */

#include "ironbee_config_auto.h"

#include <ironbee/ahocorasick.h>

#include "ahocorasick_private.h"

#include <ironbee/debug.h>

#include <ctype.h>

/*------ Aho - Corasick ------*/

/**
 * Creates an aho corasick automata with states in trie form
 *
 * @param ac_tree pointer to store the matcher
 * @param flags options for the matcher
 * @param pool memory pool to use
 *
 * @returns Status code
 */
ib_status_t ib_ac_create(ib_ac_t **ac_tree,
                         uint8_t flags,
                         ib_mpool_t *pool)
{
    IB_FTRACE_INIT();

    if (ac_tree == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *ac_tree = (ib_ac_t *)ib_mpool_calloc(pool, 1,
                                          sizeof(ib_ac_t));
    if (*ac_tree == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    (*ac_tree)->mp = pool;
    (*ac_tree)->flags = flags;
    (*ac_tree)->root = (ib_ac_state_t *)ib_mpool_calloc(pool, 1,
                                                 sizeof(ib_ac_state_t));

    if ( (*ac_tree)->root == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Returns the state that would result of applying the
 * aho corasick goto() function to a given state with the given letter
 *
 * @param parent_state the state from which the transition would be done
 * @param char_state the letter used for the transition
 *
 * @returns the state in result
 */
static ib_ac_state_t*
ib_ac_child_for_code(ib_ac_state_t *parent_state,
                     ib_ac_char_t char_state)
{
    IB_FTRACE_INIT();

    ib_ac_state_t *state = NULL;

    if (parent_state == NULL || parent_state->child == NULL)
    {
        IB_FTRACE_RET_PTR(ib_ac_state_t, NULL);
    }

    for (state = parent_state->child;
         state != NULL;
         state = state->sibling)
    {
        if (state->letter == char_state) {
            IB_FTRACE_RET_PTR(ib_ac_state_t, state);
        }
    }

    IB_FTRACE_RET_PTR(ib_ac_state_t, NULL);
}

/**
 * Adds state to parent state, if it is not already there
 *
 * @param parent the state where we want to link the new state
 * @param child the new state
 *
 */
static void ib_ac_add_child(ib_ac_state_t *parent,
                            ib_ac_state_t *child)
{
    IB_FTRACE_INIT();

    ib_ac_state_t *state = NULL;

    child->parent = parent;

    if (parent->child == NULL) {
        parent->child = child;
        IB_FTRACE_RET_VOID();
    }

    for (state = parent->child;
         state->sibling != NULL;
         state = state->sibling)
    {
        if (state == child) {
            IB_FTRACE_RET_VOID();
        }
    }

    if (state != child) {
        /* We have found the right place */
        state->sibling = child;
    }

    IB_FTRACE_RET_VOID();
}

/**
 * Adds a pattern into the trie
 *
 * @param ac_tree pointer to the matcher
 * @param pattern to add
 * @param callback function pointer to call if pattern is found
 * @param data pointer to pass to the callback if pattern is found
 * @param len the length of the pattern
 *
 * @returns Status code
 */
ib_status_t ib_ac_add_pattern(ib_ac_t *ac_tree,
                              const char *pattern,
                              ib_ac_callback_t callback,
                              void *data,
                              size_t len)
{
    IB_FTRACE_INIT();

    ib_ac_state_t *parent = NULL;
    ib_ac_state_t *child = NULL;

    size_t length = 0;

    size_t i = 0;
    size_t j = 0;

    if (ac_tree->flags & IB_AC_FLAG_PARSER_READY) {
        IB_FTRACE_RET_STATUS(IB_DECLINED);
    }

    length = (len == 0) ? strlen(pattern) : len;
    parent = ac_tree->root;

    for (i = 0; i < length; i++) {
        ib_ac_char_t letter = pattern[i];

        if (ac_tree->flags & IB_AC_FLAG_PARSER_NOCASE)
        {
            letter = tolower(letter);
        }

        child = ib_ac_child_for_code(parent, letter);
        if (child == NULL) {
            child = (ib_ac_state_t *)ib_mpool_calloc(ac_tree->mp, 1,
                                                 sizeof(ib_ac_state_t));
            if (child== NULL) {
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }

            child->letter = letter;
            child->level = i;

            child->pattern = (char *)ib_mpool_calloc(ac_tree->mp, 1,
                                                  i + 1);
            if (child->pattern == NULL) {
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }

            /* Copy the content it should match to reach this state.
             * If the state produces an output, it will be the pattern
             * it self */
            for (j = 0; j <= i; j++) {
                child->pattern[j] = pattern[j];
            }
            child->pattern[i + 1] = '\0';
        }

        if (i == length - 1) {
            if ((child->flags & IB_AC_FLAG_STATE_OUTPUT) == 0)
            {
                ac_tree->pattern_cnt++;
                child->flags |= IB_AC_FLAG_STATE_OUTPUT;
            }

            child->callback = (ib_ac_callback_t) callback;
            child->data = data;
        }

        ib_ac_add_child(parent, child);
        parent = child;
    }

    /* It needs to be compiled */
    ac_tree->flags &= ~IB_AC_FLAG_PARSER_COMPILED;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Link branches that are subpatterns of other to produce it's output
 *
 * @param ac_tree the matcher that holds the patterns
 * @param state the state where it should start (it's used recursively)
 *
 */
static void ib_ac_link_outputs(ib_ac_t *ac_tree,
                               ib_ac_state_t *state)
{
    IB_FTRACE_INIT();
    ib_ac_state_t *child = NULL;
    ib_ac_state_t *outs = NULL;

    for (child = state->child;
         child != NULL;
         child = child->sibling)
    {
        if (child->fail == NULL) {
            continue;
        }

        for (outs = child->fail;
             outs != ac_tree->root && outs != NULL;
             outs = outs->fail)
        {
            if (outs->flags & IB_AC_FLAG_STATE_OUTPUT) {
                child->outputs = outs;
                break;
            }
        }
    }

    for (child = state->child;
         child != NULL;
         child = child->sibling)
    {
        if (child->child != NULL) {
            ib_ac_link_outputs(ac_tree, child);
        }
    }
    IB_FTRACE_RET_VOID();
}

/**
 * Remove unuseful failure links to skip using an invalid transition
 *
 * @param ac_tree the matcher that holds the patterns
 * @param state the state where it should start (it's used recursively)
 *
 */
static void ib_ac_unlink_unuseful(ib_ac_t *ac_tree,
                               ib_ac_state_t *state)
{
    IB_FTRACE_INIT();
    ib_ac_state_t *child = NULL;
    ib_ac_state_t *fail_state = NULL;
    ib_ac_state_t *found = NULL;

    for (child = state->child;
         child != NULL;
         child = child->sibling)
    {
        if (child->fail == NULL ||
            child->fail->child == NULL ||
            child->child == NULL)
        {
            continue;
        }

        for (fail_state = child->fail->child;
             fail_state != ac_tree->root && fail_state != NULL;
             fail_state = fail_state->sibling)
        {
            found = ib_ac_child_for_code(child, fail_state->letter);
            if (found == NULL) {
                break;
            }
        }

        if (found != NULL) {
            /* There's no transition in the fail state that will
             * success, since the fail state doesn't have any letter not
             * present at the goto() of the main state. So let's
             * change the fail state to parent. Consider that this is
             * different to the output links (they'll still valid) */
             child->fail = ac_tree->root;

             /* printf("Removing invalid fails\n"); */
        }
    }

    for (child = state->child;
         child != NULL;
         child = child->sibling)
    {
        if (child->child != NULL) {
            ib_ac_unlink_unuseful(ac_tree, child);
        }
    }

    IB_FTRACE_RET_VOID();
}

/**
 * Add items to the bintree for fast goto() transitions. Recursive calls
 *
 * @param state State to add
 * @param states states array sorted by it's letter
 * @param lb left branch index
 * @param rb right branch index
 * @param pos current position
 * @param pool the memory pool to use
 *
 * @return ib_status_t status of the operation
 */
static ib_status_t ib_ac_add_bintree_sorted(ib_ac_bintree_t *state,
                                            ib_ac_state_t *states[],
                                            int pos,
                                            int lb,
                                            int rb,
                                            ib_mpool_t *pool)
{
    IB_FTRACE_INIT();
    ib_status_t st;
    int left = 0;
    int right = 0;

    if ((pos - lb) > 1) {
        left = lb + (pos - lb) / 2;
        state->left = (ib_ac_bintree_t *)
            ib_mpool_calloc(pool, 1, sizeof(ib_ac_bintree_t));
        if (state->left == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        state->left->state = states[left];
        state->left->letter = states[left]->letter;
    }

    if ((rb - pos) > 1) {
        right = pos + (rb - pos) / 2;
        state->right = (ib_ac_bintree_t *)
            ib_mpool_calloc(pool, 1, sizeof(ib_ac_bintree_t));
        if (state->right == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        state->right->state = states[right];
        state->right->letter = states[right]->letter;
    }

    if (state->right != NULL) {
        st = ib_ac_add_bintree_sorted(state->right, states, right,
                                      pos, rb, pool);
        if (st != IB_OK) {
            IB_FTRACE_RET_STATUS(st);
        }

    }

    if (state->left != NULL) {
        st = ib_ac_add_bintree_sorted(state->left, states, left, lb,
                                      pos, pool);
        if (st != IB_OK) {
            IB_FTRACE_RET_STATUS(st);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Builds balanced binary tree of the children states of the given state
 *
 * @param ac_tree the ac tree matcher
 * @param state the parent state
 *
 * @return ib_status_t status of the operation
 */
static ib_status_t ib_ac_build_bintree(ib_ac_t *ac_tree,
                                       ib_ac_state_t *state)
{
    IB_FTRACE_INIT();

    ib_ac_state_t *child = state->child;
    ib_ac_state_t **states = NULL;

    size_t count = 0;
    size_t pos = 0;

    size_t i = 0;
    size_t j = 0;

    for (count = 0;
         child != NULL;
         child = child->sibling)
    {
        count++;
    }

    states = (ib_ac_state_t **)ib_mpool_calloc(ac_tree->mp, count,
                                             sizeof(ib_ac_state_t *));

    if (states == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    child = state->child;
    for (i = 0; i < count; i++) {
        states[i] = child;
        child = child->sibling;
    }

    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            ib_ac_state_t *tmp;

            if (states[i]->letter < states[j]->letter) {
                continue;
            }

            tmp = states[i];
            states[i] = states[j];
            states[j] = tmp;
        }
    }

    state->bintree = (ib_ac_bintree_t *)ib_mpool_calloc(ac_tree->mp,
                                                1, sizeof(ib_ac_bintree_t));

    if (state->bintree == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    pos = count / 2;
    state->bintree->state = states[pos];
    state->bintree->letter = states[pos]->letter;
    ib_ac_add_bintree_sorted(state->bintree, states, pos, -1, count,
                                      ac_tree->mp);

    for (i = 0; i < count; i++) {
        if (states[i]->child != NULL) {
            ib_ac_build_bintree(ac_tree, states[i]);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Constructs fail links of branches (the failure transition function)
 *
 * @param ac_tree the ac tree matcher
 *
 * @return ib_status_t status of the operation
 */
static ib_status_t ib_ac_link_fail_states(ib_ac_t *ac_tree)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    ib_ac_state_t *child = NULL;
    ib_ac_state_t *state = NULL;
    ib_ac_state_t *goto_state = NULL;

    ib_list_t *iter_queue = NULL;

    if (ac_tree->flags & IB_AC_FLAG_PARSER_COMPILED) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ac_tree->root->pattern = 0;

    rc = ib_list_create(&iter_queue, ac_tree->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ac_tree->root->fail = ac_tree->root;

    /* All first-level children will fail back to root state */
    for (child = ac_tree->root->child;
         child != NULL;
         child = child->sibling)
    {
        child->fail = ac_tree->root;
        rc = ib_list_enqueue(iter_queue, (void *) child);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    while (ib_list_elements(iter_queue) > 0) {
        rc = ib_list_dequeue(iter_queue, (void *) &state);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        state->fail = ac_tree->root;

        if (state->parent != ac_tree->root) {
            goto_state = ib_ac_child_for_code(state->parent->fail,
                                             state->letter);
            if (goto_state != NULL) {
                state->fail = goto_state;
            }
        }

        for (child = state->child;
             child != NULL;
             child = child->sibling)
        {
            rc = ib_list_enqueue(iter_queue, (void *) child);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    /* Link common outputs of subpatterns present in the branch*/
    ib_ac_link_outputs(ac_tree, ac_tree->root);

    /* Unlink invalid fail transitions. This guarantees that there will
     * be at least one letter with transition in each fail state*/
    ib_ac_unlink_unuseful(ac_tree, ac_tree->root);

    if (ac_tree->root->child != NULL) {
        ib_ac_build_bintree(ac_tree, ac_tree->root);
    }

    ac_tree->flags |= IB_AC_FLAG_PARSER_COMPILED;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Search the state to go to for the given state and letter. It represents
 * the goto() function of aho corasick using a balanced binary tree for
 * fast searching
 *
 * @param state the parent state
 * @param letter the ascii code to search
 *
 * @return ib_ac_state_t pointer to the state if found or NULL
 */
static inline ib_ac_state_t *ib_ac_bintree_goto(ib_ac_state_t *state,
                                                ib_ac_char_t letter)
{
    IB_FTRACE_INIT();

    ib_ac_bintree_t *bin_state = NULL;

    if (state == NULL) {
        IB_FTRACE_RET_PTR(ib_ac_state_t, NULL);
    }

    for (bin_state = state->bintree;
        bin_state != NULL;
        bin_state = (bin_state->letter > letter) ?
                    bin_state->left : bin_state->right )
    {
        if (bin_state->letter == letter) {
            IB_FTRACE_RET_PTR(ib_ac_state_t, bin_state->state);
        }
    }

    IB_FTRACE_RET_PTR(ib_ac_state_t, NULL);
}

/**
 * Builds links between states (the AC failure function)
 * It also link outputs of subpatterns found between branches,
 * and removes unuseful transitions. It MUST be called after patterns
 * are added
 *
 * @param ac_tree pointer to store the matcher
 *
 * @returns Status code
 */
ib_status_t ib_ac_build_links(ib_ac_t *ac_tree)
{
    IB_FTRACE_INIT();

    ib_status_t st;

    st = ib_ac_link_fail_states(ac_tree);

    if (st != IB_OK) {
        IB_FTRACE_RET_STATUS(st);
    }

    ac_tree->flags |= IB_AC_FLAG_PARSER_READY;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Wrapper for the callback call
 *
 * @param ac_ctx the matching context
 * @param state the state where a pattern match (output) is found
 *
 */
static void ib_ac_do_callback(ib_ac_context_t *ac_ctx,
                              ib_ac_state_t *state)
{
    IB_FTRACE_INIT();

    ib_ac_t *ac_tree = ac_ctx->ac_tree;

    if (state->callback != NULL) {
        state->callback(ac_tree, state->pattern, state->level + 1,
                      state->data,
                      ac_ctx->processed - (state->level + 1),
                      ac_ctx->current_offset - (state->level + 1));
    }

    state->match_cnt++;

    IB_FTRACE_RET_VOID();
}

/**
 * Search patterns of the ac_tree matcher in the given buffer using a
 * matching context. The matching context stores offsets used to process
 * a search over multiple data segments. The function has option flags to
 * specify to return where the first pattern is found, or after all the
 * data is consumed, using a user specified callback and/or building a
 * list of patterns matched
 *
 * @param ac_ctx pointer to the matching context
 * @param data pointer to the buffer to search in
 * @param len the length of the data
 * @param flags options to use while matching
 * @param mp memory pool to use
 *
 * @returns Status code
 */
ib_status_t ib_ac_consume(ib_ac_context_t *ac_ctx,
                          const char *data,
                          size_t len,
                          uint8_t flags,
                          ib_mpool_t *mp)
{
    IB_FTRACE_INIT();

    const char *end;

    ib_ac_state_t *state = NULL;
    ib_ac_state_t *fgoto = NULL;

    int flag_match = 0;

    ib_ac_t *ac_tree = ac_ctx->ac_tree;
    ac_ctx->current_offset = 0;

    if ((ac_ctx->ac_tree->flags & IB_AC_FLAG_PARSER_COMPILED) == 0)
    {
        ib_ac_build_links(ac_tree);
    }

    ac_tree = ac_ctx->ac_tree;
    if (ac_ctx->current == NULL) {
        ac_ctx->current = ac_tree->root;
    }

    state = ac_ctx->current;
    end = data + len;

    while (data < end) {
        ib_ac_char_t letter = (unsigned char)*data++;
        ac_ctx->processed++;
        ac_ctx->current_offset++;

        if (ac_tree->flags & IB_AC_FLAG_PARSER_NOCASE) {
            letter = tolower(letter);
        }

        fgoto = NULL;
        while (fgoto == NULL) {
            fgoto = ib_ac_bintree_goto(state, letter);

            if (fgoto != NULL) {

                if (fgoto->flags & IB_AC_FLAG_STATE_OUTPUT) {
                    flag_match = 1;

                    fgoto->match_cnt++;
                    ac_ctx->match_cnt++;

                    ac_ctx->current = fgoto;
                    state = fgoto;

                    if (flags & IB_AC_FLAG_CONSUME_DOCALLBACK)
                    {
                        ib_ac_do_callback(ac_ctx, state);
                    }

                    if (flags & IB_AC_FLAG_CONSUME_DOLIST)
                    {
                        /* If list is not created yet, create it */
                        if (ac_ctx->match_list == NULL)
                        {
                            ib_status_t rc;
                            rc = ib_list_create(&ac_ctx->match_list, mp);
                            if (rc != IB_OK) {
                                IB_FTRACE_RET_STATUS(rc);
                            }
                        }

                        ib_ac_match_t *mt = NULL;
                        mt = (ib_ac_match_t *)ib_mpool_calloc(mp,
                                          1, sizeof(ib_ac_match_t));
                        if (mt == NULL) {
                            IB_FTRACE_RET_STATUS(IB_EALLOC);
                        }

                        mt->pattern = state->pattern;
                        mt->data = state->data;
                        mt->pattern_len = state->level + 1;
                        mt->offset = ac_ctx->processed - (fgoto->level + 1);
                        mt->relative_offset = ac_ctx->current_offset -
                                                         (fgoto->level + 1);

                        ib_list_enqueue(ac_ctx->match_list, (void *) mt);
                    }

                    if ( !(flags & IB_AC_FLAG_CONSUME_MATCHALL))
                    {
                        IB_FTRACE_RET_STATUS(IB_OK);
                    }

                    ib_ac_state_t *outs = NULL;

                    for (outs = state->outputs;
                         outs != NULL;
                         outs = outs->outputs)
                    {
                        /* This are subpatterns of the current walked branch
                         * that are present as independent patterns as well
                         * in the tree */

                        outs->match_cnt++;
                        ac_ctx->match_cnt++;

                        if (flags & IB_AC_FLAG_CONSUME_DOCALLBACK)
                        {
                            ib_ac_do_callback(ac_ctx, outs);
                        }

                        if (flags & IB_AC_FLAG_CONSUME_DOLIST)
                        {
                            /* If list is not created yet, create it */
                            if (ac_ctx->match_list == NULL)
                            {
                                ib_status_t rc;
                                rc = ib_list_create(&ac_ctx->match_list, mp);
                                if (rc != IB_OK) {
                                    IB_FTRACE_RET_STATUS(rc);
                                }
                            }

                            ib_ac_match_t *mt = NULL;
                            mt = (ib_ac_match_t *)ib_mpool_calloc(mp,
                                              1, sizeof(ib_ac_match_t));
                            if (mt == NULL) {
                                IB_FTRACE_RET_STATUS(IB_EALLOC);
                            }

                            mt->pattern = outs->pattern;
                            mt->data = state->data;
                            mt->pattern_len = outs->level + 1;
                            mt->offset =
                                ac_ctx->processed - (outs->level + 1);
                            mt->relative_offset = ac_ctx->current_offset -
                                                         (outs->level + 1);

                            ib_list_enqueue(ac_ctx->match_list,
                                            (void *) mt);
                        }
                    }
                }
            }
            else {
                /* if goto() failed, look at the fail states */
                if (state != NULL && state->fail != NULL) {
                    if (state == ac_tree->root) {
                        break;
                    }
                    state = state->fail;
                }
                else {
                    state = ac_tree->root;
                }
            }
        }

        if (fgoto != NULL) {
            state = fgoto;
        }

        ac_ctx->current = state;
    }

    ac_ctx->current = state;

    /* If we have a match, return ok. Otherwise return IB_ENOENT */
    if (flag_match == 1) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(IB_ENOENT);
}


