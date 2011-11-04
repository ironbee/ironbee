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
 * @brief IronBee - Utility Radix functions
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

/**
 * This is a radix tree bitwise implementaion initiallly designed for
 * IP / CIDR addresses.
 */
#include "ironbee_config_auto.h"

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ironbee/util.h>
#include <ironbee/engine.h>

#include "ironbee_util_private.h"

/**
 * Creates a new prefix instance
 *
 * @param prefix reference to a pointer that will link to the allocated prefix
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_new(ib_radix_prefix_t **prefix,
                                ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_radix_prefix_new);
    *prefix = (ib_radix_prefix_t *) ib_mpool_calloc(pool, 1,
                                                    sizeof(ib_radix_prefix_t));

    if (*prefix == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    memset(*prefix, 0, sizeof(ib_radix_prefix_t));

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Creates a new prefix instance
 *
 * @param prefix reference to a pointer that will link to the allocated prefix
 * @param rawbits sequence of bytes representing the key prefix
 * @param prefixlen size in bits with the len of the prefix
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_create(ib_radix_prefix_t **prefix,
                                   uint8_t *rawbits,
                                   uint8_t prefixlen,
                                   ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_radix_prefix_create);
    ib_status_t status = ib_radix_prefix_new(prefix, pool);

    if (status != IB_OK) {
        IB_FTRACE_RET_STATUS(status);
    }

    if (*prefix == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    (*prefix)->rawbits = rawbits;
    (*prefix)->prefixlen = prefixlen;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * creates a clone of the prefix instance
 *
 * @param orig pointer to the original prefix
 * @param new_prefix reference to a pointer to the allocated prefix
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_prefix(ib_radix_prefix_t *orig,
                                  ib_radix_prefix_t **new_prefix,
                                  ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_clone_prefix);
    ib_status_t ret = ib_radix_prefix_new(new_prefix, mp);
    if (ret != IB_OK) {
        IB_FTRACE_RET_STATUS(ret);
    }

    (*new_prefix)->prefixlen = orig->prefixlen;

    int i = 0;
    int limit = IB_BITS_TO_BYTES(orig->prefixlen);

    if ((*new_prefix)->prefixlen == 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    (*new_prefix)->rawbits = (uint8_t *) ib_mpool_calloc(mp, 1, sizeof(uint8_t)*
                                                         limit);
    if ((*new_prefix)->rawbits == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    for (; i < limit; i++) {
        (*new_prefix)->rawbits[i] = orig->rawbits[i];
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * destroy a prefix
 *
 * @param prefix to destroy
 * @param pool memory of the prefix
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_destroy(ib_radix_prefix_t **prefix,
                                    ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_radix_prefix_destroy);
    /* @todo: reimplement ib_mpool_t to allow individual deallocations */
    memset(*prefix, 0, sizeof(ib_radix_prefix_t));
    *prefix = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Creates a new node instance
 *
 * @param node reference to a pointer that will link to the allocated node
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_node_new(ib_radix_node_t **node,
                              ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_radix_node_new);
    *node = (ib_radix_node_t *)ib_mpool_calloc(pool, 1,
                                               sizeof(ib_radix_node_t));

    if (*node == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    memset(*node, 0, sizeof(ib_radix_node_t));

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * creates a clone of the node instance
 *
 * @param orig pointer to the original node
 * @param new_node reference to a pointer that will link to the allocated node
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_node(ib_radix_node_t *orig,
                                ib_radix_node_t **new_node,
                                ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_clone_node);
    if (orig == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    ib_status_t ret = ib_radix_node_new(new_node, mp);
    if (ret != IB_OK) {
        IB_FTRACE_RET_STATUS(ret);
    }

    /* Copy the prefix */
    ret = ib_radix_clone_prefix(orig->prefix, &(*new_node)->prefix, mp);
    if (ret != IB_OK) {
        IB_FTRACE_RET_STATUS(ret);
    }

    /* Copy the reference to any data or NULL */
    (*new_node)->data = orig->data;

    /* left branch (next prefix starting with 0) */
    ret = ib_radix_clone_node(orig->zero, &(*new_node)->zero, mp);
    if (ret != IB_OK && ret != IB_ENOENT) {
        IB_FTRACE_RET_STATUS(ret);
    }

    /* right branch (next prefix starting with 1) */
    ret = ib_radix_clone_node(orig->one, &(*new_node)->one, mp);
    if (ret != IB_OK && ret != IB_ENOENT) {
        IB_FTRACE_RET_STATUS(ret);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Destroy a node and its childs (this includes prefix and userdatas)
 *
 * @param radix the radix of the node
 * @param node the node to destroy
 * @param pool memory pool of the node
 *
 * @returns Status code
 */
ib_status_t ib_radix_node_destroy(ib_radix_t *radix,
                                  ib_radix_node_t **node,
                                  ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_radix_node_destroy);
    if (*node == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    if ((*node)->zero != NULL) {
        ib_radix_node_destroy(radix, &(*node)->zero, pool);
    }

    if ((*node)->one != NULL) {
        ib_radix_node_destroy(radix, &(*node)->one, pool);
    }

    if ((*node)->prefix != NULL) {
        ib_radix_prefix_destroy(&(*node)->prefix, pool);
    }

    if ((*node)->data != NULL && radix->free_data != NULL) {
        radix->free_data((void*)(*node)->data);
        radix->data_cnt--;
    }

    /* @todo: reimplement ib_mpool_t and this will change */
    memset(*node, 0, sizeof(ib_radix_node_t));
    *node = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Creates a new radix tree registering functions to update, free and print
 * associated to each prefix it also register the memory pool it should use
 * for new allocations
 *
 * @param radix pointer to link the instance of the new radix
 * @param free_data pointer to the function that will be used to
 * free the userdata entries
 * @param update_data pointer to the function that knows how to update a node
 * with new user data
 * @param print_data pointer to a helper function that print_datas a userdata
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_new(ib_radix_t **radix,
                         ib_radix_free_fn_t free_data,
                         ib_radix_print_fn_t print_data,
                         ib_radix_update_fn_t update_data,
                         ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_radix_new);
    *radix = (ib_radix_t *) ib_mpool_calloc(pool, 1, sizeof(ib_radix_t));

    if (*radix == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    memset(*radix, 0, sizeof(ib_radix_t));

    (*radix)->mp = pool;
    (*radix)->update_data = update_data;
    (*radix)->print_data = print_data;
    (*radix)->free_data = free_data;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * creates a clone of the tree, allocating memory from mp
 *
 * @param orig pointer to the original pool
 * @param new_pool reference to a pointer that will link to the allocated pool
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_radix(ib_radix_t *orig,
                                 ib_radix_t **new_radix,
                                 ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_clone_radix);
    ib_status_t ret = ib_radix_new(new_radix, orig->free_data,
                                   orig->print_data, orig->update_data, mp);
    if (ret != IB_OK) {
        IB_FTRACE_RET_STATUS(ret);
    }

    /* data cnt */
    (*new_radix)->data_cnt = orig->data_cnt;

    /* copy the root branch */
    ret = ib_radix_clone_node(orig->start, &(*new_radix)->start, mp);
    if (ret != IB_OK) {
        IB_FTRACE_RET_STATUS(ret);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * return the number of datas linked from the radix
 *
 * @param radix the radix of the node
 *
 * @returns Status code
 */
size_t ib_radix_elements(ib_radix_t *radix)
{
    IB_FTRACE_INIT(ib_radix_elements);
    IB_FTRACE_RET_SIZET(radix->data_cnt);
}

/*
 * Inserts a new user data associated to the prefix passed. The prefix is not
 * used, so developers are responsible to free that prefixs
 * Keys can be of "any size" but this will be probably used for
 * CIDR data prefixes only (from 0 to 32 ~ 128 depending on IPv4
 * or IPv6 respectively)
 *
 * @param radix the radix of the node
 * @param prefix the prefix to use as index
 * @param prefix_data, the data to store under that prefix
 *
 * @returns Status code
 */
ib_status_t ib_radix_insert_data(ib_radix_t *radix,
                                 ib_radix_prefix_t *prefix,
                                 void *prefix_data)
{
    IB_FTRACE_INIT(ib_radix_insert_data);
    ib_status_t st;

    if (prefix == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (radix->start == NULL) {
        st = ib_radix_node_new(&radix->start, radix->mp);
        if (st != IB_OK) {
            IB_FTRACE_RET_STATUS(st);
        }

        /* The start node should have an empty prefix always */
        st = ib_radix_prefix_create(&radix->start->prefix, NULL, 0, radix->mp);
        if (st != IB_OK) {
            IB_FTRACE_RET_STATUS(st);
        }
    }

    if (prefix->rawbits == NULL || prefix->prefixlen == 0) {
        if (radix->start->data != NULL) {
            if (radix->update_data != NULL) {
                radix->update_data(radix->start, prefix_data);
            }
            else if (radix->free_data != NULL) {
                radix->free_data((void*)radix->start->data);
                radix->start->data = prefix_data;
            }
            else {
                /* @todo: warn the user */
                radix->start->data = prefix_data;
            }
        }
        else {
            radix->start->data = prefix_data;
        }

        radix->data_cnt++;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_radix_node_t *cur_node = NULL;
    if (IB_GET_DIR(prefix->rawbits[0]) == 0) {
        cur_node = radix->start->zero;
    }
    else {
        cur_node = radix->start->one;
    }

    /* store the reference to parent node just in case we
     * need to update something */
    ib_radix_node_t *prev_cur_node = radix->start;

    uint8_t cnt = 0;
    uint8_t cur_prefix_offset = 0;

    /* We are going to select each node as "cur_node" walking
     * the tree checking each prefix data and bit length */
    while (cnt < prefix->prefixlen && cur_node != NULL) {
        cur_prefix_offset = 0;

        for (; cur_prefix_offset < cur_node->prefix->prefixlen &&
               cnt < prefix->prefixlen; cur_prefix_offset++, cnt++)
        {
            if (IB_READ_BIT(cur_node->prefix->rawbits[cur_prefix_offset / 8],
                            cur_prefix_offset % 8) !=
                            IB_READ_BIT(prefix->rawbits[cnt / 8], cnt % 8))
            {
                /* prefix chunks different. Split here the cur_node node into
                 * common prefix (of the cur_node and the new prefix)
                 * and different suffix. */

                /* Create the new sufix from the end of the cur_node rawbits */
                uint8_t *rawbits = NULL;
                int size = 0;
                size = IB_BITS_TO_BYTES(
                               cur_node->prefix->prefixlen - cur_prefix_offset);
                rawbits = (uint8_t *) ib_mpool_calloc(radix->mp, 1,
                                                      sizeof(uint8_t) * size);
                memset(rawbits, 0, sizeof(uint8_t) * size);

                /* Copy the new sufix bits starting from the cur_prefix_offset
                 * bit offset (where the difference begins) */
                int i = cur_prefix_offset;
                int ni = 0;
                for (; i < cur_node->prefix->prefixlen; i++, ni++) {
                    if (IB_READ_BIT(cur_node->prefix->rawbits[i / 8],
                                    i % 8) == 0x01)
                    {
                        IB_SET_BIT_ARRAY(rawbits, ni);
                    }
                }

                /* Create a prefix for the new sufix */
                ib_radix_prefix_t *k = NULL;
                st = ib_radix_prefix_create(&k, rawbits,
                               cur_node->prefix->prefixlen -(cur_prefix_offset),
                               radix->mp);

                if (st != IB_OK) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }

                /* Create a node for the new sufix */
                ib_radix_node_t *n = NULL;
                st = ib_radix_node_new(&n, radix->mp);
                if (st != IB_OK) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }

                /* Assign the prefix to the new node */
                n->prefix = k;

                /* Copy reference to user data and remove the old reference */
                n->data = cur_node->data;
                cur_node->data = NULL;

                /* Update pointers */
                n->zero = cur_node->zero;
                n->one = cur_node->one;

                if ( IB_READ_BIT(rawbits[0], 0) == 0) {
                    cur_node->zero = n;
                    cur_node->one = NULL;
                }
                else {
                    cur_node->one = n;
                    cur_node->zero = NULL;
                }

                /* Update the old prefix of the cur_node */
                size = IB_BITS_TO_BYTES(cur_prefix_offset);
                rawbits = (uint8_t *) ib_mpool_calloc(radix->mp, 1,
                                                      sizeof(uint8_t) * size);
                memset(rawbits, 0, sizeof(uint8_t) * size);

                /* Copy cur_prefix_offset bits of the old prefix starting from 0
                 * offset */
                i = 0;
                for (; i < cur_prefix_offset; i++) {
                    if (IB_READ_BIT(cur_node->prefix->rawbits[i / 8],
                                    i % 8) == 0x01)
                    {
                        IB_SET_BIT_ARRAY(rawbits, i);
                    }
                }

                /* Update the len of the cur_node with cur_prefix_offset */
                cur_node->prefix->prefixlen = cur_prefix_offset;

                /* @todo: (mpool) Here we should free the old prefix */

                /* update the pointer to new rawbits with the common prefix */
                cur_node->prefix->rawbits = rawbits;

                size = IB_BITS_TO_BYTES(prefix->prefixlen - cnt);
                rawbits = (uint8_t *) ib_mpool_calloc(radix->mp, 1,
                                                      sizeof(uint8_t) * size);
                memset(rawbits, 0, sizeof(uint8_t) * size);

                /* Copy the new sufix bits from offset cnt
                 * (where the difference begins) to the end of the prefix */
                i = cnt;
                ni = 0;
                for (; i < prefix->prefixlen; i++, ni++) {
                    if (IB_READ_BIT(prefix->rawbits[i / 8], i % 8) == 0x01) {
                        IB_SET_BIT_ARRAY(rawbits, ni);
                    }
                }

                /* Create the prefix for the new rawbits sufix */
                st = ib_radix_prefix_create(&k, rawbits,
                                            prefix->prefixlen - cnt, radix->mp);

                if (st != IB_OK) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }

                /* Create the node for the new prefix */
                st = ib_radix_node_new(&n, radix->mp);
                if (st != IB_OK) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }

                n->prefix = k;

                /* Update the cur_node to point to this sufix aswell */
                if (IB_READ_BIT(prefix->rawbits[cnt / 8], cnt % 8) == 0) {
                    cur_node->zero = n;
                }
                else {
                    cur_node->one = n;
                }

                /* Set the user data */
                n->data = prefix_data;

                radix->data_cnt++;
                IB_FTRACE_RET_STATUS(IB_OK);
            }
        }

        /* If we are here then we didn't find the palce yet. Check if
         * 1. We are just on a node with the same prefix
         * 2. We don't need to split any old prefix, just to append
         * 3. We just need to append because we are at an ending node
         * 4. We need to walk more */

        if (cur_prefix_offset >= cur_node->prefix->prefixlen &&
            cnt >= prefix->prefixlen)
        {
            /* we are exactly on the node */
            if (cur_node->data == NULL) {
                cur_node->data = prefix_data;
            }
            else {
                if (radix->update_data != NULL) {
                    radix->update_data(cur_node, prefix_data);
                }
                else if (radix->free_data != NULL) {
                    radix->free_data((void*)prefix_data);
                    cur_node->data = prefix_data;
                }
                else {
                    cur_node->data = prefix_data;
                }
            }

            radix->data_cnt++;
            IB_FTRACE_RET_STATUS(IB_OK);

        }
        else if (cur_prefix_offset >= cur_node->prefix->prefixlen &&
                   cnt < prefix->prefixlen)
        {
            /* If we matched all the cur_node prefix, look if we have to jump to
            the next cur_node (otherwise break the loop to append the
            next prefix chunk) */

            prev_cur_node = cur_node;
            if (IB_READ_BIT(prefix->rawbits[cnt / 8], cnt % 8) == 0x00) {
                if (cur_node->zero) {
                    cur_node = cur_node->zero;
                }
                else {
                    /*printf("NO PATH TO LEFT, Need to append!\n"); */
                    break;
                }
            }
            else if (IB_READ_BIT(prefix->rawbits[cnt / 8], cnt % 8) == 0x01) {
                if (cur_node->one) {
                    cur_node = cur_node->one;
                }
                else {
                    /*printf("NO PATH TO RIGHT, Need to append!\n"); */
                    break;
                }
            }
            /* Continue walking the cur_node */
            continue;

        }
        else if (cnt >= prefix->prefixlen &&
                   cur_prefix_offset < cur_node->prefix->prefixlen)
        {
            /* Look at the remaining bits in the cur_node prefix and split it */

            uint8_t *rawbits = NULL;
            uint8_t size = 0;
            size = IB_BITS_TO_BYTES(cur_node->prefix->prefixlen -
                                    cur_prefix_offset);
            rawbits = (uint8_t *) ib_mpool_calloc(radix->mp, 1,
                                                  sizeof(uint8_t) * size);
            memset(rawbits, 0, sizeof(uint8_t) * size);

            /* Copy the new sufix */
            int i = cur_prefix_offset;
            int ni = 0;

            for (; i < cur_node->prefix->prefixlen; i++, ni++) {
                if (IB_READ_BIT(cur_node->prefix->rawbits[i / 8],
                                i % 8) ==0x01)
                {
                    IB_SET_BIT_ARRAY(rawbits, ni);
                }
            }

            /* Create prefix for the new sufix */
            ib_radix_prefix_t *k = NULL;
            st = ib_radix_prefix_create(&k, rawbits,
                      cur_node->prefix->prefixlen-cur_prefix_offset, radix->mp);

            if (st != IB_OK) {
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }

            /* Create node for the new prefix */
            ib_radix_node_t *n = NULL;
            st = ib_radix_node_new(&n, radix->mp);
            if (st != IB_OK) {
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }

            /* Update pointers */
            n->zero = cur_node->zero;
            n->one = cur_node->one;
            n->prefix = k;

            if (IB_GET_DIR(rawbits[0]) == 0) {
                cur_node->zero = n;
                cur_node->one = NULL;
            }
            else {
                cur_node->one = n;
                cur_node->zero = NULL;
            }

            /* Copy reference to user data to the new sufix */
            n->data = cur_node->data;
            cur_node->data = prefix_data;

            /* OK, now update the old cur_node prefix len
             * Update / cut the old prefix */
            size = IB_BITS_TO_BYTES(cur_prefix_offset);
            rawbits = (uint8_t *) ib_mpool_calloc(radix->mp, 1,
                                                  sizeof(uint8_t) * size);
            memset(rawbits, 0, sizeof(uint8_t) * size);

            /* Copy cur_prefix_offset bits of the old prefix */
            i = 0;
            for (; i < cur_prefix_offset; i++) {
                if (IB_READ_BIT(cur_node->prefix->rawbits[i / 8],
                                i % 8) ==0x01)
                {
                    IB_SET_BIT_ARRAY(rawbits, i);
                }
            }

            /* @todo: (mpool) Here we should free the old prefix */

            /* Update the pointer to new rawbits */
            cur_node->prefix->rawbits = rawbits;
            /* Update len of the cur_prefix_offset */
            cur_node->prefix->prefixlen = cur_prefix_offset;

            radix->data_cnt++;
            IB_FTRACE_RET_STATUS(IB_OK);
        }
        else {
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }

        /* Else try to jump to the next branch and continue,
         * or break to append */
        if (IB_READ_BIT(prefix->rawbits[cnt / 8], cnt % 8) == 0) {
            if (cur_node->zero == NULL) {
                break;
            }
            else {
                prev_cur_node = cur_node;
                cur_node = cur_node->zero;
            }
        }
        else {
            if (cur_node->one == NULL) {
                break;
            }
            else {
                prev_cur_node = cur_node;
                cur_node = cur_node->one;
            }
        }
    }

    /* Here we just need to append a new node without splitting anything */
    if (cnt < prefix->prefixlen) {
        cur_node = prev_cur_node;

        uint8_t size = IB_BITS_TO_BYTES(prefix->prefixlen - cnt);
        uint8_t *rawbits = (uint8_t *) ib_mpool_calloc(radix->mp, 1,
                                                       sizeof(uint8_t) * size);

        memset(rawbits, 0, sizeof(uint8_t) * size);

        /* Copy the new sufix bits from offset cnt (where the difference begins)
        to the end of the prefix */
        int i = cnt;
        int ni = 0;

        for (; i < prefix->prefixlen; i++, ni++) {
            if (IB_READ_BIT(prefix->rawbits[i / 8], i % 8) == 0x01) {
                IB_SET_BIT_ARRAY(rawbits, ni);
            }
        }

        ib_radix_node_t *node = NULL;
        st = ib_radix_node_new(&node, radix->mp);

        if (st != IB_OK) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        st = ib_radix_prefix_create(&node->prefix, rawbits,
                                    prefix->prefixlen - cnt, radix->mp);

        if (st != IB_OK) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        node->data = prefix_data;

        if (IB_READ_BIT(prefix->rawbits[cnt / 8], cnt % 8) == 0 &&
            cur_node->zero == NULL)
        {
            cur_node->zero = node;
        }
        else if (IB_READ_BIT(prefix->rawbits[cnt / 8], cnt % 8) == 1 &&
                   cur_node->one == NULL)
        {
            cur_node->one = node;
        }
        else {
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }

        radix->data_cnt++;
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
}


/*
 * Destroy the memory pool of a radix (warning: this usually includes itself)
 *
 * @param radix the radix to destroy
 *
 * @returns Status code
 */
ib_status_t ib_radix_destroy(ib_radix_t **radix)
{
    IB_FTRACE_INIT(ib_radix_destroy);

    if (!radix) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_radix_node_destroy(*radix, &(*radix)->start, (*radix)->mp);
    ib_mpool_destroy((*radix)->mp);
    *radix = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 *
 * Recursive function that return the data associated to prefix
 * This function should not be called directly
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param type can be IB_RADIX_PREFIX or IB_RADIX_CLOSEST
 * @param result reference to the pointer that will be linked to the data if any
 *
 * @returns Status code
 */
static ib_status_t ib_radix_match_prefix(ib_radix_node_t *node,
                                                ib_radix_prefix_t *prefix,
                                                int offset,
                                                uint8_t type,
                                                void *result)
{
    IB_FTRACE_INIT(ib_radix_match_prefix);
    int i = 0;

    if (node == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    for (; i < node->prefix->prefixlen && offset < prefix->prefixlen;
          i++, offset++)
    {
        if (IB_READ_BIT(node->prefix->rawbits[i / 8], i % 8) !=
            IB_READ_BIT(prefix->rawbits[offset / 8], offset % 8))
        {
            IB_FTRACE_RET_STATUS(IB_ENOENT);
        }
    }

    if (offset == prefix->prefixlen) {
        *(void **)result = node->data;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_status_t ret;
    if (IB_READ_BIT(prefix->rawbits[offset / 8], offset % 8) == 0) {
        ret = ib_radix_match_prefix(node->zero, prefix, offset, type, result);
    }
    else {
        ret = ib_radix_match_prefix(node->one, prefix, offset, type, result);
    }

    /* If we are returning NULL from recursion and we are matching
     * the value of a prefix or it's parent prefix, return the
     * first not empty data otherwise return NULL */

    /* With this we are creating the ability of "inheritance" between common
     * prefixes and prefix. For example for inserting a net /16 with data that
     * will be returned in case we match a host of that net that's not
     * present as a complete prefix. */
    if (ret == IB_ENOENT && type == IB_RADIX_CLOSEST &&
        node->data != NULL)
    {
        *(void **)result = node->data;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(ret);
}

/*
 * Recursive function that return a list of all the datas found under this cidr
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param rlist reference to a pointer where the list will be allocated
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
static ib_status_t ib_radix_match_all(ib_radix_node_t *node,
                                                ib_radix_prefix_t *prefix,
                                                int offset,
                                                ib_list_t **rlist,
                                                ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_match_all);
    ib_status_t ret = IB_OK;
    uint8_t inserted = 0;

    if (node == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    if (offset >= prefix->prefixlen && node->data != NULL) {
        if (*rlist == NULL) {
            ret = ib_list_create(rlist, mp);
            if (ret != IB_OK) {
                IB_FTRACE_RET_STATUS(ret);
            }
        }

        ret = ib_list_push(*rlist, node->data);
        if (ret != IB_OK) {
            IB_FTRACE_RET_STATUS(ret);
        }

        inserted = 1;
    }
    else {
        int i = 0;

        for (; i < node->prefix->prefixlen && offset < prefix->prefixlen;
              i++, offset++)
        {
            if (IB_READ_BIT(node->prefix->rawbits[i / 8], i % 8) !=
                IB_READ_BIT(prefix->rawbits[offset / 8], offset % 8))
            {
                IB_FTRACE_RET_STATUS(IB_ENOENT);
            }
    
            if (offset >= prefix->prefixlen && node->data != NULL) {
                if (*rlist == NULL) {
                    ret = ib_list_create(rlist, mp);
                    if (ret != IB_OK) {
                        IB_FTRACE_RET_STATUS(ret);
                    }
                }

                ret = ib_list_push(*rlist, node->data);
                if (ret != IB_OK) {
                    IB_FTRACE_RET_STATUS(ret);
                }

                inserted = 1;
                break;
            }
        }
    }

    if (inserted == 0 && offset >= prefix->prefixlen && node->data != NULL) {
        if (*rlist == NULL) {
            ret = ib_list_create(rlist, mp);
            if (ret != IB_OK) {
                IB_FTRACE_RET_STATUS(ret);
            }
        }
        ret = ib_list_push(*rlist, node->data);
        if (ret != IB_OK) {
            IB_FTRACE_RET_STATUS(ret);
        }
    }

    if (offset >= prefix->prefixlen) {
        ret = ib_radix_match_all(node->zero, prefix, offset, rlist, mp);
        if (ret != IB_OK && ret != IB_ENOENT) {
            IB_FTRACE_RET_STATUS(ret);
        }
            
        ret = ib_radix_match_all(node->one, prefix, offset, rlist, mp);
        if (ret != IB_OK && ret != IB_ENOENT) {
            IB_FTRACE_RET_STATUS(ret);
        }
    }
    else {
        if (IB_READ_BIT(prefix->rawbits[offset / 8], offset % 8) == 0) {
            ret = ib_radix_match_all(node->zero, prefix, offset, rlist, mp);
        }
        else {
            ret = ib_radix_match_all(node->one, prefix, offset, rlist, mp);
        }
    }

    IB_FTRACE_RET_STATUS(ret);
}

/*
 * Function that return the data allocated to an exact prefix
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param result reference to the pointer that will be linked to the data if any
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_exact(ib_radix_t *radix,
                                 ib_radix_prefix_t *prefix,
                                 void *result)
{
    IB_FTRACE_INIT(ib_radix_match_exact);
    if (radix->start == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    if (prefix->prefixlen == 0) {
        *(void **)result = radix->start->data;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_status_t ret;
    if (IB_GET_DIR(prefix->rawbits[0]) == 0) {
        ret = ib_radix_match_prefix(radix->start->zero, prefix, 0,
                                    IB_RADIX_PREFIX, result);
    }
    else {
        ret = ib_radix_match_prefix(radix->start->one, prefix, 0,
                                    IB_RADIX_PREFIX, result);
    }

    IB_FTRACE_RET_STATUS(ret);
}

/*
 * Function that return the data linked to an exact prefix if any. Otherwise
 * it will start falling backwars until it reach a immediate shorter prefix with
 * any data returning it. If no data is found on it's path it will return null.
 *
 * Example: insert data in 192.168.1.0/24
 * search with this function the data of 192.168.1.27
 * it will not have an exact match ending with .27, but walking backwards the
 * recursion, it will find data associated to 192.168.1.0/24
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param result reference to the pointer that will be linked to the data if any
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_closest(ib_radix_t *radix,
                                   ib_radix_prefix_t *prefix,
                                   void *result)
{
    IB_FTRACE_INIT(ib_radix_match_closest);

    if (radix->start == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    if (prefix->prefixlen == 0) {
        *(void **)result = radix->start->data;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_status_t ret;
    if (IB_GET_DIR(prefix->rawbits[0]) == 0) {
        ret = ib_radix_match_prefix(radix->start->zero, prefix, 0,
                                    IB_RADIX_CLOSEST, result);
    }
    else {
        ret = ib_radix_match_prefix(radix->start->one, prefix, 0,
                                    IB_RADIX_CLOSEST, result);
    }

    if (ret == IB_ENOENT && radix->start->data != NULL) {
        *(void **)result = radix->start->data;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(ret);
}

/*
 * Function that return a list of all the datas with a prefix that start like
 * the provided prefix arg
 *
 * Example: insert data in 192.168.1.27, as well as 192.168.1.28,
 * as well as 192.168.10.0/24 and 10.0.0.0/8 and then search 192.168.0.0/16
 * it should return a list containing all the datas except the associated to
 * 10.0.0.0/8
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param rlist reference to the pointer that will be linked to the list, if any
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_all_data(ib_radix_t *radix,
                                     ib_radix_prefix_t *prefix,
                                     ib_list_t **rlist,
                                     ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_match_all_data);
    ib_status_t ret;

    if (rlist == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (radix->start == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    if (radix->start->data != NULL  &&
        prefix->prefixlen == 0)
    {
        if (*rlist == NULL) {
            ret = ib_list_create(rlist, mp);
            if (ret != IB_OK) {
                IB_FTRACE_RET_STATUS(ret);
            }
        }
        ret = ib_list_push(*rlist, radix->start->data);
        if (ret != IB_OK) {
            IB_FTRACE_RET_STATUS(ret);
        }
    }

    if (prefix->prefixlen == 0) {
            // Append data of both branches
            ret = ib_radix_match_all(radix->start->zero, prefix, 0,
                                        rlist, mp);
            if (ret != IB_OK && ret != IB_ENOENT) {
                IB_FTRACE_RET_STATUS(ret);
            }
            ret = ib_radix_match_all(radix->start->one, prefix, 0,
                                        rlist, mp);
            if (ret != IB_OK && ret != IB_ENOENT) {
                IB_FTRACE_RET_STATUS(ret);
            }
    }
    else {
        // Follow it's path until we reach the correct offset
        if (IB_GET_DIR(prefix->rawbits[0]) == 0) {
            ret = ib_radix_match_all(radix->start->zero, prefix, 0,
                                        rlist, mp);
        }
        else {
            ret = ib_radix_match_all(radix->start->one, prefix, 0,
                                        rlist, mp);
        }
    }

    if (ret == IB_ENOENT && *rlist != NULL &&
        ib_list_elements(*rlist) > 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(ret);
}

/*
 * @internal
 *
 * Create a binary representation (in_addr) of IP, allocating mem from mp
 *
 * @param ip ascii representation
 * @param mp pool where we should allocate the in_addr
 *
 * @returns struct in_addr*
 */
static inline struct in_addr *ib_radix_get_IPV4_addr(const char *ip,
                                                     ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_get_IPV4_addr);
    struct in_addr *rawbytes = NULL;

    if ((rawbytes = (struct in_addr *) ib_mpool_calloc(mp, 1,
                                               sizeof(struct in_addr))) == NULL)
    {
        IB_FTRACE_RET_PTR(struct in_addr, NULL);
    }

    if (inet_pton(AF_INET, ip, rawbytes) <= 0) {
        IB_FTRACE_RET_PTR(struct in_addr, NULL);
    }

    IB_FTRACE_RET_PTR(struct in_addr, rawbytes);
}

/*
 * @internal
 *
 * Create a binary representation (in6_addr) of IP, allocating mem from mp
 *
 * @param ip ascii representation
 * @param mp pool where we should allocate in6_addr
 *
 * @returns struct in6_addr*
 */
static inline struct in6_addr *ib_radix_get_IPV6_addr(const char *ip,
                                                      ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_get_IPV6_addr);
    struct in6_addr *rawbytes = NULL;

    if ((rawbytes = (struct in6_addr *) ib_mpool_calloc(mp, 1,
                                              sizeof(struct in6_addr))) == NULL)
    {
        IB_FTRACE_RET_PTR(struct in6_addr, NULL);
    }

    if (inet_pton(AF_INET6, ip, rawbytes) <= 0) {
        IB_FTRACE_RET_PTR(struct in6_addr, NULL);
    }

    IB_FTRACE_RET_PTR(struct in6_addr, rawbytes);
}

/*
 * Create a prefix of type ib_radix_prefix_t given the cidr ascii representation
 * Valid for ipv4 and ipv6.
 * warning:
 *  the criteria to determine if ipv6 or ipv4 is the presence of ':' (ipv6)
 *  so the functions using this API should implement their own checks for valid
 *  formats, with regex, or functions, thought
 *
 * @param cidr ascii representation
 * @param mp pool where we should allocate the prefix
 *
 * @returns struct in6_addr*
 */
ib_status_t ib_radix_ip_to_prefix(const char *cidr,
                                  ib_radix_prefix_t **prefix,
                                  ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_radix_ip_to_prefix);

    /* If we got a mask, we will need to copy the IP to separate it from
     the mask, and the max length should be the length of a IPv6 in ascii,
     so 39 plus \0 */
    char ip_tmp[40];

    const char *mask = NULL;
    uint64_t nmask = 0;

    if (IB_RADIX_IS_IPV4(cidr)) {
        mask = strstr(cidr, "/");

        if (mask != NULL) {
            nmask = strtoull(mask+1, NULL, 10);

            if (nmask > 32) {
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }

            /* Don't modify the orign cidr, instead of that, create a local copy
             in stack memory to avoid allocations */
            memcpy(ip_tmp, cidr, mask - cidr);
            ip_tmp[mask - cidr] = '\0';
            cidr = ip_tmp;
        }
        else {
            nmask = 32;
        }

        struct in_addr *cidrv4 = ib_radix_get_IPV4_addr(cidr, mp);
        if (cidrv4 == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Return a prefix for IPV4 */
        IB_FTRACE_RET_STATUS(ib_radix_prefix_create(prefix,
                                                    (uint8_t *) cidrv4,
                                                    (uint8_t) nmask, mp));
    }
    else if (IB_RADIX_IS_IPV6(cidr)) {
        mask = strstr(cidr, "/");

        if (mask != NULL) {
            nmask = strtoull(mask+1, NULL, 10);

            if (nmask > 128) {
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }

            /* Don't modify the orign cidr, instead of that, create a local copy
             in stack memory to avoid allocations */
            memcpy(ip_tmp, cidr, mask - cidr);
            ip_tmp[mask - cidr] = '\0';
            cidr = ip_tmp;
        }
        else {
            nmask = 128;
        }

        struct in6_addr *cidrv6 = ib_radix_get_IPV6_addr(cidr, mp);
        if (cidrv6 == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Return a prefix for IPV6 */
        IB_FTRACE_RET_STATUS(ib_radix_prefix_create(prefix,
                                                    (uint8_t *) cidrv6,
                                                    (uint8_t) nmask, mp));
    }
    IB_FTRACE_RET_STATUS(IB_EINVAL);
}


