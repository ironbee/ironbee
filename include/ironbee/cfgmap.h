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

#ifndef _IB_CFGMAP_H_
#define _IB_CFGMAP_H_

/**
 * @file
 * @brief IronBee --- Configuration Map Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <stddef.h>
#include <sys/types.h>

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
typedef struct ib_cfgmap_t ib_cfgmap_t;
struct ib_cfgmap_t {
    ib_mm_t    mm;   /**< Memory manager */
    ib_hash_t *hash; /**< The underlying hash */
    void      *base; /**< Pointer to base of config data. */
};

/**
 * Type of a getter for a config map entry.
 *
 * @sa IB_CFGMAP_INIT_DYNAMIC_ENTRY()
 * @sa ib_field_get_fn_t
 *
 * @param[in]  base Pointer to base of config data.
 * @param[out] pval Where to store value.
 * @param[in]  field Field storing value.
 * @param[in]  data Callback data.
 * @return Status code.
 */
typedef ib_status_t (*ib_cfgmap_get_fn_t)(
    const void       *base,
    void             *pval,
    const ib_field_t *field,
    void             *data
);

/**
 * Type of a setter for a config map entry.
 *
 * @sa IB_CFGMAP_INIT_DYNAMIC_ENTRY()
 * @sa ib_field_set_fn_t
 *
 * @param[in] base  Pointer to base of config data.
 * @param[in] field Field storing value.
 * @param[in] value Value to set to.
 * @param[in] data  Callback data.
 * @returns Status code.
 */
typedef ib_status_t (*ib_cfgmap_set_fn_t)(
    void       *base,
    ib_field_t *field,
    void       *value,
    void       *data
);

/** Config map initialization structure. */
typedef struct ib_cfgmap_init_t ib_cfgmap_init_t;
struct ib_cfgmap_init_t {
    const char          *name; /**< Field name */
    ib_ftype_t           type; /**< Field type */

    /* Either this... */
    ib_cfgmap_get_fn_t   fn_get;     /**< Getter */
    void                *cbdata_get; /**< Getter data */
    ib_cfgmap_set_fn_t   fn_set;     /**< Setter */
    void                *cbdata_set; /**< Setter data */

    /* .. Or this.  Used if fn_get and fn_set are NULL */
    off_t                offset; /**< Field data offset within base */
    size_t               dlen;   /**< Field data length (<= uintptr_t) */
};

/**
 * Create a configuration map.
 *
 * @param pcm Address which new map is written
 * @param mm Memory manager to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_create(ib_cfgmap_t **pcm,
                                        ib_mm_t mm);

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
 */
#define IB_CFGMAP_INIT_ENTRY(name, type, basetype, field) \
    { \
        (name), \
        (type), \
        NULL, NULL, NULL, NULL, \
        offsetof(basetype, field), \
        sizeof(((basetype *)(0))->field) \
    }

/**
 * Defines a dynamic configuration map entry.
 *
 * @param name     Configuration entry name
 * @param type     Configuration entry data type
 * @param set      Setter function.
 * @param set_data Setter data.
 * @param get      Getter function.
 * @param get_data Getter data.
 */
#define IB_CFGMAP_INIT_DYNAMIC_ENTRY(name, type, set, set_data, get, get_data) \
    { \
        (name), \
        (type), \
        (set), (set_data), \
        (get), (get_data), \
        0, 0 \
    }

/**
 * Required as the last entry.
 */
#define IB_CFGMAP_INIT_LAST { NULL, IB_FTYPE_GENERIC, NULL, NULL, NULL, NULL, 0, 0 }

/**
 * Initialize a configuration map with entries.
 *
 * @param cm          Configuration map
 * @param base        Base address of the structure holding the values.
 * @param init        Configuration map initialization structure
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_init(ib_cfgmap_t *cm,
                                      void *base,
                                      const ib_cfgmap_init_t *init);

/**
 * Set a configuration value.
 *
 * @param cm Configuration map
 * @param name Configuration entry name
 * @param in_val Value to assign to entry
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_set(ib_cfgmap_t *cm,
                                     const char *name,
                                     void *in_val);

/**
 * Get a configuration value.
 *
 * @param cm Configuration map
 * @param name Configuration entry name
 * @param out_val Address which the value is written
 * @param ptype Address which the value type is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_get(const ib_cfgmap_t *cm,
                                     const char *name,
                                     void *out_val, ib_ftype_t *ptype);


/** @} IronBeeUtilCfgMap */


#ifdef __cplusplus
}
#endif

#endif /* _IB_CFGMAP_H_ */
