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

#ifndef _IB_CFGMAP_H_
#define _IB_CFGMAP_H_

/**
 * @file
 * @brief IronBee - Configuration Map Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/types.h>
#include <ironbee/array.h>
#include <ironbee/list.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilCfgMap Configuration Map
 * @ingroup IronBeeUtil
 *
 * Configuration map routines.
 *
 * This is a wrapper around a hash specifically targeted
 * at configuration data, making it easier to deal with storing specific
 * types of data and accessing the data faster.  CfgMap data can be
 * accessed directly (and simultaneously) via a structure and hash
 * like syntax. It is meant to expose the hash syntax externally while
 * being able to access the structure internally without the overhead
 * of the hash functions.
 *
 * To accomplish this, the creator is responsible for allocating
 * a fixed length base data structure (C struct) and supply a mapping as to
 * the locations (offsets within the struct) of each configuration field.
 *
 * @{
 */


/**
 * Configuration Map
 */
struct ib_cfgmap_t {
    ib_mpool_t         *mp;           /**< Memory pool */
    ib_hash_t          *hash;         /**< The underlying hash */
    ib_cfgmap_init_t   *data;         /**< Initialization mapping */
};

/** Config map initialization structure. */
struct ib_cfgmap_init_t {
    const char          *name;     /**< Field name */
    ib_ftype_t           type;     /**< Field type */
    off_t                offset;   /**< Field data offset within base */
    size_t               dlen;     /**< Field data length (<= uintptr_t) */
    uintptr_t            defval;   /**< Default value */
};

/**
 * Create a configuration map.
 *
 * @param pcm Address which new map is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_create(ib_cfgmap_t **pcm,
                                        ib_mpool_t *pool);

/**
 * Defines a configuration map initialization structure.
 *
 * @param name Name of structure
 */
#define IB_CFGMAP_INIT_STRUCTURE(name) const ib_cfgmap_init_t name[]

/**
 * Defines a configuration map entry.
 *
 * @param name Configuration entry name
 * @param type Configuration entry data type
 * @param basetype Type of structure holding values
 * @param field Field name in structure holding values
 * @param defval Default value of entry
 */
#define IB_CFGMAP_INIT_ENTRY(name,type,basetype,field,defval) \
    { \
        (name), \
        (type), \
        offsetof(basetype, field), \
        sizeof(((basetype*)(0))->field), \
        (const uintptr_t)(defval) \
    }

/**
 * Required as the last entry.
 */
#define IB_CFGMAP_INIT_LAST { NULL }


/**
 * Initialize a configuration map with entries.
 *
 * @param cm Configuration map
 * @param base Base address of the structure holding the values
 * @param init Configuration map initialization structure
 * @param usedefaults If true, use the map default values as base
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_init(ib_cfgmap_t *cm,
                                      const void *base,
                                      const ib_cfgmap_init_t *init,
                                      int usedefaults);

/**
 * Set a configuration value.
 *
 * @param cm Configuration map
 * @param name Configuration entry name
 * @param data Data to assign to entry
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_set(ib_cfgmap_t *cm,
                                     const char *name,
                                     void *data);

/**
 * Get a configuration value.
 *
 * @param cm Configuration map
 * @param name Configuration entry name
 * @param pval Address which the value is written
 * @param ptype Address which the value type is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_get(ib_cfgmap_t *cm,
                                     const char *name,
                                     void *pval, ib_ftype_t *ptype);


/** @} IronBeeUtilCfgMap */


#ifdef __cplusplus
}
#endif

#endif /* _IB_CFGMAP_H_ */
