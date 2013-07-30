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

#ifndef _IB_VAR_H_
#define _IB_VAR_H_

#include <ironbee/field.h>
#include <ironbee/rule_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief IronBee --- Var Support
 *
 * @author Christopher Alfeld <calfeld@calfeld.net>
 */

/**
 * @defgroup IronBeeEngineVar Var Support
 * @ingroup IronBeeEngine
 *
 * This API covers var sources, var filters, and var targets, known as
 * vars, filters, and targets, respectively, in the rule language.
 *
 * **Key Concepts and Types:**
 *
 * - @ref ib_var_config_t -- Configuration.  A set of var sources.
 * - @ref ib_var_store_t  -- Store.  A mapping of sources to values.
 * - @ref ib_var_source_t -- Source.  A name and associated metadata.
 * - @ref ib_var_filter_t -- Filter.  A description of which subkey to fetch
 *   from a field.
 * - @ref ib_var_target_t -- Target.  A var source and filter (possibly
 *   empty).
 *
 * **APIs:**
 *
 * The API is divided into six sections:
 *
 * - @c ib_var_config -- Create a configuration.
 * - @c ib_var_store  -- Create a store.
 * - @c ib_var_source -- Register, lookup, get, or set sources.  This API is
 *   the fundamental service provided by the var code.  All later APIs are
 *   defined in terms of it as the field API.
 * - @c ib_var_filter -- Create and apply filters to fields; parse filter
 *   specification strings.
 * - @c ib_var_target -- Create and apply targets; parse target specification
 *   strings.
 * - @c ib_var_expand -- Expand strings containing embeded target references.
 *
 * **Pre-Computation:**
 *
 * A theme of the APIs here are the separation into pre-computation and
 * execution, pushing as much work as possible to configuration time.  For
 * example, when the source name is known at configuration time, it can be
 * converted into an @ref ib_var_source_t allowing gets (but not sets) to
 * be executed at evaluation time in constant time.  Similar behavior is
 * available for filters and targets.
 *
 * **Performance:**
 *
 * A *slow* function has runtime that grows with the length of the source name
 * and/or the number of registered var sources.
 *
 * A *fast* function has constant runtime.
 *
 * @{
 */

/**
 * @defgroup IronBeeEngineVarConfiguration Data Configuration
 *
 * Set of sources.
 *
 * @{
 **/

/**
 * A set of var sources.
 *
 * A var store will be defined in terms of a var configuration.
 **/
typedef struct ib_var_config_t ib_var_config_t;

/**
 * Create new var configuration.
 *
 * @param[out] config The new var configuration.
 * @param[in]  mp     Memory pool to use.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_var_config_create(
    ib_var_config_t **config,
    ib_mpool_t       *mp
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Access memory pool of @a config.
 **/
ib_mpool_t DLL_PUBLIC *ib_var_config_pool(
    const ib_var_config_t *config
)
NONNULL_ATTRIBUTE(1);

/**@}*/

/**
 * @defgroup IronBeeEngineVarStore Data Store
 *
 * Map source to values.
 *
 * @{
 **/

/**
 * A map of var source to value.
 *
 * A var store is associated with a var configuration and holds the values
 * for the sources in that configuration.  These values are held such that
 * indexe sources can get their value in constant time.
 *
 * A store has an assocaited memory pool which may be used to creating fields
 * to use as values that have the same lifetime as the store.
 */
typedef struct ib_var_store_t ib_var_store_t;

/**
 * Create new var store.
 *
 * @param[out] store  The new var store.
 * @param[in]  mp     Memory pool to use.
 * @param[in]  config Data configuration.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_var_store_create(
    ib_var_store_t        **store,
    ib_mpool_t             *mp,
    const ib_var_config_t  *config
)
NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Access configuration of @a store.
 **/
const ib_var_config_t DLL_PUBLIC *ib_var_store_config(
    const ib_var_store_t *store
)
NONNULL_ATTRIBUTE(1);

/**
 * Access memory pool of @a store.
 **/
ib_mpool_t DLL_PUBLIC *ib_var_store_pool(
    const ib_var_store_t *store
)
NONNULL_ATTRIBUTE(1);

/**@}*/

/**
 * @defgroup IronBeeEngineVarSource Data Source
 *
 * Create, lookup, get, and set sources.
 *
 * @{
 **/

/**
 * A source of var in an @ref ib_var_store_t.
 *
 * From the purposes of getting and setting values, a var source is
 * semantically equivalent to its name.  The main advantage gained from the
 * strucutre is the ability to *get* in constant time (sets remain slow).
 *
 * The other purpose of a var source is to associate metavar with a var
 * source.  At present, the metavar is the initial phase the source takes a
 * value and the last phase it may be change that value in.
 **/
typedef struct ib_var_source_t ib_var_source_t;

/**
 * Register a var source.
 *
 * This function registers a var source and establishes it as indexed.  It
 * should only be called at configuration time.  It should never be called
 * at evaluation time.  To get a var source at evaluation time, use
 * ib_var_source_lookup().
 *
 * The phase argument specify the first phase the source will be meaningful
 * at and the last phase its value will change at.  Either or both may be
 * IB_RULE_PHASE_NONE which should be interpreted as any phase and never,
 * respectively.  Specifying these phases accurately can improve performance,
 * especially in more advance rule systems such as Predicate.
 *
 * Is is strongly recommended that sources either never change their value
 * (and @a initial_phase == final_phase) or that they only change their value
 * by being list fields and appending additional values to the end of the
 * list.  Source that do not follow this advise will not work properly in
 * Predicate.
 *
 * This function is *slow*.
 *
 * @param[out] source        Source created.  Will have lifetime equal to
 *                           @a config.  May be NULL.
 * @param[in]  config        Data configuration to create source for.
 * @param[in]  name          Name of source.
 * @param[in]  name_length   Length of @a name.
 * @param[in]  initial_phase First phase source has meaningful value in.
 * @param[in]  final_phase   Last phase source may change value in.  Must be
 *                           IB_RULE_PHASE_NONE or after @a initial_phase.
 * @return
 * - IB_OK on success.
 * - IB_EEXISTS if a source named @a name already exists.
 * - IB_EINVAL if @a initial_phase is after @a final_phase.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_var_source_register(
    ib_var_source_t     **source,
    ib_var_config_t      *config,
    const char           *name,
    size_t                name_length,
    ib_rule_phase_num_t   initial_phase,
    ib_rule_phase_num_t   final_phase
)
NONNULL_ATTRIBUTE(2, 3);

/** Access config of @a source. */
const ib_var_config_t DLL_PUBLIC *ib_var_source_config(
    const ib_var_source_t *source
)
NONNULL_ATTRIBUTE(1);

/** Access name of @a source. */
void DLL_PUBLIC ib_var_source_name(
    const ib_var_source_t  *source,
    const char            **name,
    size_t                 *name_length
)
NONNULL_ATTRIBUTE(1, 2, 3);

/** Access initial phase of @a source. **/
ib_rule_phase_num_t DLL_PUBLIC ib_var_source_initial_phase(
    const ib_var_source_t *source
)
NONNULL_ATTRIBUTE(1);

/** Access final phase of @a source. **/
ib_rule_phase_num_t DLL_PUBLIC ib_var_source_final_phase(
    const ib_var_source_t *source
)
NONNULL_ATTRIBUTE(1);

/** True iff @a source is indexed. */
bool DLL_PUBLIC ib_var_source_is_indexed(
    const ib_var_source_t *source
)
NONNULL_ATTRIBUTE(1);

/**
 * Fetch the value of a source in a store.
 *
 * This function is *fast* if @a source is indexed and *slow* otherwise.  This
 * paragraph is the most important performance paragraph in this API.
 *
 * @param[in]  source Source to fetch value for.
 * @param[out] field  Value fetched.  May be NULL.
 * @param[in]  store  Store to fetch value from.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if @a source is from a different var configuration than
 *   @a store.
 * - IB_ENOENT if @a source is not set in @a store.
 **/
ib_status_t DLL_PUBLIC ib_var_source_get(
    ib_var_source_t  *source,
    ib_field_t      **field,
    ib_var_store_t   *store
)
NONNULL_ATTRIBUTE(1, 3);

/** Const version of ib_var_source_get(). **/
ib_status_t DLL_PUBLIC ib_var_source_get_const(
    const ib_var_source_t  *source,
    const ib_field_t      **field,
    const ib_var_store_t   *store
);

/**
 * Set the value of a source in a store.
 *
 * It is recommended that this function only be used to initially set the
 * field for a source.  Subsequent changes should be done by modifying the
 * field.  That is, call this function at transaction start to set the
 * source to an appropriate field and then change the field value as needed.
 *
 * If @a source has already been set, this will overwrite the previous
 * value.
 *
 * The name of @a field must match the name of @a store.
 *
 * @sa ib_var_source_initialize() for an easier way to create a field and
 * set a source to it.
 *
 * This function is *slow*.
 *
 * @param[in]  source Source to set value for.
 * @param[in]  store  Store to set value in.
 * @param[out] field  Value to set.  May be NULL.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a source is from a different var configuration than
 *   @a store.
 * - IB_EINVAL if @a field is non-NULL and has a different name than
 *   @a source.
 **/
ib_status_t DLL_PUBLIC ib_var_source_set(
    ib_var_source_t *source,
    ib_var_store_t  *store,
    ib_field_t      *field
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Lookup a var source at config time.
 *
 * If the name of the var source is known at configuration time, call this
 * then and store the var source somewhere.  For indexed var sources, this
 * will allow for get/set operations to execute in O(1) time.  If the name of
 * the var source is only known at evaluation time, then call this then.
 *
 * This function is *slow*.
 *
 * @param[out] source      Looked up var source.  If @a indexed is false,
 *                         then this source has lifetime equal to @a mp.
 *                         Otherwise, has lifetime equal to @a config.  May
 *                         be NULL.
 * @param[in]  mp          Memory pool to use for unindexed sources.  If NULL,
 *                         an unindexed lookup will result in IB_ENOENT.
 * @param[in]  config      Data configuration to look up in.
 * @param[in]  name        Name of source to lookup.  If result is unindexed,
 *                         this must outlive result.
 * @param[in]  name_length Length of @a name.
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if source if unindexed and @a mp is NULL.
 * - IB_EALLOC if source is unindexed and allocation fails.
 **/
ib_status_t DLL_PUBLIC ib_var_source_lookup(
    ib_var_source_t       **source,
    ib_mpool_t             *mp,
    const ib_var_config_t  *config,
    const char             *name,
    size_t                  name_length
)
NONNULL_ATTRIBUTE(3, 4);

/**
 * Initialize a source to a given field type.
 *
 * This is a help function which creates a field of the appropriate type and
 * sets it as the value of @a source.
 *
 * Num and float fields are initialized to 0, time is initialize to epoch,
 * nul string and byte string are initialized to empty strings, lists to
 * empty lists and sbuffers to result in an error.
 *
 * This function is *slow*.
 *
 * @param[in]  source Source to initialize.
 * @param[out] field  Field created; may be NULL.  Lifetime will be that of
 *                    @a store.
 * @param[in]  store  Store to initialize @a source in.
 * @param[in]  ftype  Field type.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a source is from a different var configuration than
 *   @a store.
 * - IB_EINVAL if @a ftype is unsupported, e.g., GENERIC or SBUFFER.
 **/
ib_status_t DLL_PUBLIC ib_var_source_initialize(
    ib_var_source_t  *source,
    ib_field_t      **field,
    ib_var_store_t   *store,
    ib_ftype_t        ftype
)
NONNULL_ATTRIBUTE(1, 3);

/** @} */

/**
 * @} IronBeeEngineVar
 */

#ifdef __cplusplus
}
#endif

#endif
