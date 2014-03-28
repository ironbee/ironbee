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
#include <ironbee/mm.h>
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
 * Var sources, filters, and targets.
 *
 * This API covers var sources, var filters, and var targets, known as
 * vars, filters, and targets, respectively, in the rule language.
 *
 * **Key Concepts and Types:**
 *
 * - @ref ib_var_config_t -- Configuration.  A set of var sources.
 * - @ref ib_var_store_t  -- Store.  A mapping of sources to values.
 * - @ref ib_var_source_t -- Source.  A name and associated metadata.
 * - @ref ib_var_filter_t -- Filter.  A description of which subkey to get
 *   from a field.
 * - @ref ib_var_target_t -- Target.  A var source and filter (possibly
 *   empty).
 * - @ref ib_var_expand_t -- Expand.  A description of how to construct a
 *   string out of targets.
 *
 * **APIs:**
 *
 * The API is divided into six sections:
 *
 * - @ref IronBeeEngineVarConfiguration -- Acquire a configuration.
 * - @ref IronBeeEngineVarStore  -- Acquire a store.
 * - @ref IronBeeEngineVarSource -- Register, acquire, get, or set sources.
 *   This API is the fundamental service provided by the var code.  All later
 *   APIs are defined in terms of it and the field API.
 * - @ref IronBeeEngineVarFilter -- Acquire and apply filters to fields;
 *   parse filter specification strings.
 * - @ref IronBeeEngineVarTarget -- Acquire and apply targets; parse target
 *   specification strings.
 * - @ref IronBeeEngineVarExpand -- Expand strings containing embedded target
 *   references.
 *
 * **Pre-Computation:**
 *
 * A theme of the APIs here are the separation into pre-computation and
 * execution, pushing as much work as possible to configuration time.  For
 * example, when the source name is known at configuration time, it can be
 * converted into an @ref ib_var_source_t allowing gets (but not sets) to
 * be executed at evaluation time in constant time.  Similar behavior is
 * available for filters, targets, and expands.  All such pre-computation
 * routines have `acquire` in the name, e.g., ib_var_store_acquire().
 * Whenever possible, acquire at configuration time.
 *
 * **Performance:**
 *
 * Generally, acquisition is slow but use of an acquired object is fast.  The
 * main exception is write access which is as slow as acquisition.
 *
 * @{
 */

/**
 * A set of var sources.
 *
 * A var store will be defined in terms of a var configuration.
 **/
typedef struct ib_var_config_t ib_var_config_t;

/**
 * A source of var in an @ref ib_var_store_t.
 *
 * From the purposes of getting and setting values, a var source is
 * semantically equivalent to its name.  The main advantage gained from a
 * source is the ability to *get* in constant time (sets remain slow).
 *
 * The other purpose of a var source is to associate metavar with a var
 * source.  At present, the metavar is the initial phase the source takes a
 * value and the last phase it may be change that value in.
 **/
typedef struct ib_var_source_t ib_var_source_t;

/**
 * A map of var source to value.
 *
 * A var store is associated with a var configuration and holds the values
 * for the sources in that configuration.  These values are held such that
 * indexed sources can get their value in constant time.
 *
 * A store has an associated memory manager which may be used to creating
 * fields to use as values that have the same lifetime as the store.
 */
typedef struct ib_var_store_t ib_var_store_t;

/**
 * A selection criteria to apply to a list of fields.
 **/
typedef struct ib_var_filter_t ib_var_filter_t;

/**
 * A source and (possibly trivial) filter.
 **/
typedef struct ib_var_target_t ib_var_target_t;

/**
 * A prepared string expansion.
 **/
typedef struct ib_var_expand_t ib_var_expand_t;

/**
 * @defgroup IronBeeEngineVarConfiguration Var Configuration
 *
 * Set of sources.
 *
 * @{
 **/

/**
 * Acquire new var configuration.
 *
 * @param[out] config The new var configuration.
 * @param[in]  mm     Memory manager to use.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_var_config_acquire(
    ib_var_config_t **config,
    ib_mm_t           mm
)
NONNULL_ATTRIBUTE(1);

/**
 * Access memory manager of @a config.
 **/
ib_mm_t DLL_PUBLIC ib_var_config_mm(
    const ib_var_config_t *config
)
NONNULL_ATTRIBUTE(1);

/**@}*/

/**
 * @defgroup IronBeeEngineVarStore Var Store
 *
 * Map source to values.
 *
 * @{
 **/

/**
 * Acquire new var store.
 *
 * @param[out] store  The new var store.
 * @param[in]  mm     Memory manager to use.
 * @param[in]  config Var configuration.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_var_store_acquire(
    ib_var_store_t        **store,
    ib_mm_t                 mm,
    const ib_var_config_t  *config
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Access configuration of @a store.
 **/
const ib_var_config_t DLL_PUBLIC *ib_var_store_config(
    const ib_var_store_t *store
)
NONNULL_ATTRIBUTE(1);

/**
 * Access memory manager of @a store.
 **/
ib_mm_t DLL_PUBLIC ib_var_store_mm(
    const ib_var_store_t *store
)
NONNULL_ATTRIBUTE(1);

/**
 * Export entire contents.
 *
 * @param[in]  store  Store to export.
 * @param[in]  result List to push values to.
 **/
void ib_var_store_export(
    ib_var_store_t *store,
    ib_list_t      *result
)
NONNULL_ATTRIBUTE(1, 2);

/**@}*/

/**
 * @defgroup IronBeeEngineVarSource Var Source
 *
 * Create, lookup, get, and set sources.
 *
 * @{
 **/

/**
 * Register a var source.
 *
 * This function registers a var source and establishes it as indexed.  It
 * should only be called at configuration time.  It should never be called
 * at evaluation time.  To get a var source at evaluation time, use
 * ib_var_source_acquire().
 *
 * The phase argument specify the first phase the source will be meaningful
 * at and the last phase its value will change at.  Either or both may be
 * IB_PHASE_NONE which should be interpreted as any phase and never,
 * respectively.  Specifying these phases accurately can improve performance,
 * especially in more advance rule systems such as Predicate.
 *
 * Is is strongly recommended that sources either never change their value
 * (and @a initial_phase == final_phase) or that they only change their value
 * by being list fields and appending additional values to the end of the
 * list.  Source that do not follow this advise will not work properly in
 * Predicate.
 *
 * This function should *only* be used at configuration time, never
 * afterwards.
 *
 * @param[out] source        Source acquired.  Will have lifetime equal to
 *                           @a config.  May be NULL.
 * @param[in]  config        Var configuration to acquire source for.
 * @param[in]  name          Name of source.
 * @param[in]  name_length   Length of @a name.
 * @param[in]  initial_phase First phase source has meaningful value in.
 * @param[in]  final_phase   Last phase source may change value in.  Must be
 *                           IB_PHASE_NONE or after @a initial_phase.
 * @return
 * - IB_OK on success.
 * - IB_EEXIST if a source named @a name already exists.
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
 * This function is fast (small constant) if @a source is indexed and slow
 * (grows with length of source name and number of sources) otherwise.  This
 * paragraph is the most important performance paragraph in this API.
 *
 * @param[in]  source Source to get value for.
 * @param[out] field  Where to store value.  May be NULL.
 * @param[in]  store  Store to get value from.
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
 * The name of @a field will be changed to match that of @a source.
 *
 * @sa ib_var_source_initialize() for an easier way to create a field and
 * set a source to it.
 *
 * Unlike ib_var_source_get(), this function is slow; as slow as acquiring a
 * source.
 *
 * @param[in] source Source to set value for.
 * @param[in] store  Store to set value in.
 * @param[in] field  Value to set.  May be NULL.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a source is from a different var configuration than
 *   @a store.
 **/
ib_status_t DLL_PUBLIC ib_var_source_set(
    ib_var_source_t *source,
    ib_var_store_t  *store,
    ib_field_t      *field
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Acquire a var source.
 *
 * If the name of the var source is known at configuration time, call this
 * then and store the var source somewhere.  For indexed var sources, this
 * will allow for get operations to execute in O(1) time.  If the name of
 * the var source is only known at evaluation time, then call this then.
 *
 * This function is slow, with time growing with with @a name_length and number of
 * registered sources.
 *
 * @param[out] source      Looked up var source.  If @a indexed is false,
 *                         then this source has lifetime equal to @a mp.
 *                         Otherwise, has lifetime equal to @a config.  May
 *                         be NULL.
 * @param[in]  mm          Memory manager to use for unindexed sources.  If
 *                         IB_MM_NULL an unindexed lookup will result in
 *                         IB_ENOENT.
 * @param[in]  config      Var configuration to look up in.
 * @param[in]  name        Name of source to lookup.
 * @param[in]  name_length Length of @a name.
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if source if unindexed and @a mm is IB_MM_NULL.
 * - IB_EALLOC if source is unindexed and allocation fails.
 **/
ib_status_t DLL_PUBLIC ib_var_source_acquire(
    ib_var_source_t       **source,
    ib_mm_t                 mm,
    const ib_var_config_t  *config,
    const char             *name,
    size_t                  name_length
)
NONNULL_ATTRIBUTE(3, 4);

/**
 * Initialize a source to a given field type.
 *
 * This is a helper function which creates a field of the appropriate type and
 * sets it as the value of @a source.
 *
 * Num and float fields are initialized to 0, time is initialize to epoch,
 * nul string and byte string are initialized to empty strings, lists to
 * empty lists and sbuffers to result in an error.
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

/**
 * Append a field to a list source, initializing if needed.
 *
 * This is a helper function which appends a field to a source of type list,
 * initializing it (ib_var_source_initialize()) if needed.
 *
 * @param[in] source Source to append to.
 * @param[in] field  Field to append.
 * @param[in] store  Store source value is in.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a source is from a different var configuration than
 *   @a store.
 * - IB_EINCOMPAT if @a source has a value in store that is non-list.
 * - IB_EOTHER if there is an unexpected error in field or list handling.
 **/
ib_status_t DLL_PUBLIC ib_var_source_append(
    ib_var_source_t *source,
    ib_var_store_t  *store,
    ib_field_t      *field
)
NONNULL_ATTRIBUTE(1, 2, 3);

/** @} */

/**
 * @defgroup IronBeeEngineVarFilter Var Filter
 *
 * Select subkeys of a field.
 *
 * Filters reduced a list source to a shorter list.  They do this is one of
 * three ways:
 *
 * 1. The filter may be a regexp filter (indicated by enclosing in slashes)
 *    in which case, the result is all elements of the list whose name
 *    matches the regexp.
 * 2. The filter may be a string, in which case, the result is all elements
 *    of the list whose name matches the string, case insensitive.
 * 3. The field may be a dynamic list field, in which case the filter is
 *    passed to it without interpretation.  If the result is a list, it is
 *    provided.  Otherwise, an error results.
 *
 * @{
 **/

/**
 * Acquire a filter
 *
 * This function prepares a filter for later use.  At present, the primary
 * advantage is for regexp filters, but future filter functionality may also
 * take advantage of it.  As such, this should be called at configuration time
 * whenever possible.
 *
 * If @a filter_string begins and ends with a forward slash, the text between
 * the slashes will be  compiled as a regular expression.
 *
 * @param[out] filter               Filter prepared.  Filter will have
 *                                  lifetime equal to  @a mp.
 * @param[in]  mm                   Memory manager to use.
 * @param[in]  filter_string        Filter string to prepare.
 * @param[in]  filter_string_length Length of @a filter_string.
 * @param[out] error_message        Where to store an error message on
 *                                  regexp compile failure.  May be NULL;
 *                                  should not be freed.
 * @param[out] error_offset         Where in regexp, error occurred.  May be
 *                                  NULL.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a filter_string is invalid.  Invalid strings include the
 *   empty string, an invalid regexp specification.
 **/
ib_status_t DLL_PUBLIC ib_var_filter_acquire(
    ib_var_filter_t **filter,
    ib_mm_t           mm,
    const char       *filter_string,
    size_t            filter_string_length,
    const char      **error_message,
    int              *error_offset
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Apply a filter.
 *
 * Apply @a filter to the collection @a field and store results in
 * @a result.
 *
 * @param[in]  filter Filter to apply.
 * @param[out] result Results.  Value is `const ib_field_t *`.  Lifetime is
 *                    equal to @a mp.
 * @param[in]  mm     Memory manager to use.
 * @param[in]  field  Field to apply filter to.  Must be a field with value a
 *                    list of `const ib_field_t *`.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a field is not of type list.
 * - IB_EOTHER if @a field is dynamic and dynamic query results in error.
 **/
ib_status_t DLL_PUBLIC ib_var_filter_apply(
    const ib_var_filter_t  *filter,
    const ib_list_t       **result,
    ib_mm_t                 mm,
    const ib_field_t       *field
)
NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Apply a filter, removing found elements.
 *
 * Apply @a filter to the collection @a field, removing any elements found.
 * Elements removed are stored in @a result if it is non-null.
 *
 * @param[in]  filter Filter to apply.  Cannot be regexp.
 * @param[out] result Results.  Value is `const ib_field_t *`.  Lifetime is
 *                    equal to @a mp.  Can be NULL.
 * @param[in]  mm     Memory manager to use.  Can be NULL if @a result is NULL.
 * @param[in]  field  Field to apply filter to.  Must be a field with value a
 *                    list of `ib_field_t *`.  Cannot be dynamic.
 *
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if nothing to remove.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a field is not of type list.
 * - IB_EINVAL if @a filter is regexp.
 * - IB_EOTHER if @a field is dynamic and dynamic query results in error.
 **/
ib_status_t DLL_PUBLIC ib_var_filter_remove(
    const ib_var_filter_t  *filter,
    ib_list_t             **result,
    ib_mm_t                 mm,
    ib_field_t             *field
)
NONNULL_ATTRIBUTE(1, 4);

/** @} */

/**
 * @defgroup IronBeeEngineVarTarget Var Target
 *
 * Source and filter.
 *
 * A target is a source and a filter, possibly trivia, or an expand to
 * construct a filter from.  It can be acquired from such or prepared from a
 * string.  At evaluation time, it can be be used to get a list of fields.
 * As such, it provides an abstraction, turning a user specification of a
 * target into appropriate fields to act on while hiding details such as the
 * presence of a filter.
 *
 * There are four categories of targets:
 *
 * - Trivial, e.g., `foo`, which evaluates to the var named foo.
 * - Simple, e.g., `foo:bar`, which evaluates to all members of the var `foo`
 *   named `bar`.
 * - Regexp, e.g., `foo:/bar/`, which evaluates to all members of the var
 *   `foo` whose name matches the regexp `bar`.
 * - Expand, e.g., `foo:%{bar}`, which replaces `%{bar}` and then interprets
 *   the result as a simple target.  This form is fundamentally slower than
 *   the others as the target is reevaluated at execution time.  Note
 *   that only simple targets can result, not trivial or regexp.  Finally,
 *   note that the expansion can be nested, e.g., `foo:x-%{bar:%{baz}}`.
 *
 * @{
 **/

/**
 * Acquire a target from a source and an expand or filter.
 *
 * @sa ib_var_target_acquire()
 *
 * @param[out] target Created target.  Lifetime will be equal to @a mp.
 * @param[in]  mm     Memory manager to use for allocations.
 * @param[in]  source Source to get field from.
 * @param[in]  expand Expand to use to lazily generate filter.  May be
 *                    NULL to use @a filter instead.
 * @param[in]  filter Filter to apply to field.  May be NULL to indicate
 *                    a trivial field.  Must be NULL if @a expand is not NULL.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_var_target_acquire(
    ib_var_target_t       **target,
    ib_mm_t                 mm,
    ib_var_source_t        *source,
    const ib_var_expand_t  *expand,
    const ib_var_filter_t  *filter
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Return the name of the @ref ib_var_source_t backing this target.
 *
 * @param[in] target The target whose source will be queried.
 * @param[out] name The name of the @ref ib_var_source_t that backs @a target.
 * @param[out] len The length of @a name.
 */
void DLL_PUBLIC ib_var_target_source_name(
    const ib_var_target_t  *target,
    const char            **name,
    size_t                 *len
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Return the @ref ib_var_source_t used by @a target.
 *
 * @param[in] target The target whose source is returned.
 *
 * @returns the @ref ib_var_source_t used by @a target.
 */
ib_var_source_t DLL_PUBLIC *ib_var_target_source(
    const ib_var_target_t *target
)
NONNULL_ATTRIBUTE(1);

/**
 * Acquire a target from a specification string.
 *
 * A specification string is a source name, optionally followed by a :
 * and a filter string, i.e., `^[^:]+(:.+)?$`.
 *
 * @param[out] target               Created target.  Lifetime will be equal
 *                                  to @a mp.
 * @param[in]  mm                   Memory manager to use for allocations.
 * @param[in]  config               Config to find sources in.
 * @param[in]  target_string        Target string to prepare from.
 * @param[in]  target_string_length Length of @a target_string.
 * @param[out] error_message        Where to store an error message on
 *                                  regexp compile failure.  May be NULL;
 *                                  should not be freed.
 * @param[out] error_offset         Where in regexp, error occurred.  May be
 *                                  NULL.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a target_string is invalid (see ib_var_filter_acquire()).
 **/
ib_status_t DLL_PUBLIC ib_var_target_acquire_from_string(
    ib_var_target_t       **target,
    ib_mm_t                 mm,
    const ib_var_config_t  *config,
    const char             *target_string,
    size_t                  target_string_length,
    const char            **error_message,
    int                    *error_offset
)
NONNULL_ATTRIBUTE(1, 3, 4);

/**
 * Fetch types of target; read-only.
 *
 * @param[in] target Target to get values of.
 * @param[in] store Store holding values.
 * @param[out] type The type that the requested target is.
 *
 * @return
 * - IB_OK On success.
 * - IB_ENOENT If the field is not found.
 */
ib_status_t ib_var_target_type(
    ib_var_target_t  *target,
    ib_var_store_t   *store,
    ib_ftype_t       *type
)
NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Fetch values of target; read-only.
 *
 * This function always outputs to a list stored in @a result.  For list
 * fields, this will either be the filtered list value of the field (possibly
 * via a trivial filter).  For non-list filters, this will either be a list
 * with a single element, the value; or an error if the filter is non-trivial.
 *
 * The lifetime of @a result will depend on the value.  For non-filtered
 * list fields, the underlying value will be reported directly and @a result
 * will have lifetime equal to that field.  For all other results, the
 * lifetime will equal that of @a mp.
 *
 * @param[in]  target Target to get values of.
 * @param[out] result Fetched values.  Lifetime will vary.  See above.
 *                    Value is `ib_field_t *`.
 * @param[in]  mm     Memory manager to use for allocations.
 * @param[in]  store  Store holding values.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if target and store have different configs.
 * - IB_EINVAL if filter is non-trivial and value of source is not a list.
 * - IB_EINVAL if filter is expand and tries to be a regexp.
 * - IB_ENOENT if source does not exist in store.
 * - IB_EOTHER if source value is dynamic and query results in error.
 **/
ib_status_t DLL_PUBLIC ib_var_target_get(
    ib_var_target_t  *target,
    const ib_list_t **result,
    ib_mm_t           mm,
    ib_var_store_t   *store
)
NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Const version of ib_var_target_get().
 *
 * Value of @a result: `const ib_field_t *`.
 **/
ib_status_t DLL_PUBLIC ib_var_target_get_const(
    const ib_var_target_t  *target,
    const ib_list_t       **result,
    ib_mm_t                 mm,
    const ib_var_store_t   *store
)
NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Remove entries described by target.
 *
 * This function behaves differently depending on the target type:
 *
 * - Trivial: Equivalent to using ib_var_source_set() to set the source to
 *   NULL.
 * - Simple: Equivalent to using ib_var_filter_remove().
 * - Expand: As Simple, but with an expansion step.
 * - Regexp: Returns IB_EINVAL.
 *
 * @param[in]  target Target to get values of.
 * @param[out] result Removed values.  Lifetime will be equal to @a mp.  May
 *                    be NULL.
 * @param[in]  mm     Memory manager to use for allocations.  May be NULL if
 *                    @a result is NULL.
 * @param[in]  store  Store holding values.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_ENOENT if nothing to remove.
 * - IB_EINVAL if target and store have different configs.
 * - IB_EINVAL if filter is non-trivial and value of source is not a list.
 * - IB_EINVAL if value of source is dynamic.
 * - IB_EINVAL if filter is expand and tries to be a regexp.
 * - IB_EINVAL if filter is regexp.
 **/
ib_status_t DLL_PUBLIC ib_var_target_remove(
    ib_var_target_t  *target,
    ib_list_t       **result,
    ib_mm_t           mm,
    ib_var_store_t   *store
)
NONNULL_ATTRIBUTE(1, 4);

/**
 * Convert an expand target to a simple target.
 *
 * This function does the expansion of the target.  The result can then be
 * used multiple times without reincurring the cost of expansion.  However,
 * expansion fixes the expanded values.  As such, this routine should only
 * be used at evaluation time for targets that will be reused with no changes
 * to the store in between uses.
 *
 * If called with a non-expand target, will output @a target.
 *
 * @param[in]  target   Target to expand.
 * @param[out] expanded Expanded target.  Lifetime will be that of @a target
 *                      for non-expand targets and that of @a mp for expand
 *                      targets.
 * @param[in]  mm       Memory manager to use.
 * @param[in]  store    Store to use.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_var_target_expand(
    ib_var_target_t       *target,
    ib_var_target_t      **expanded,
    ib_mm_t                mm,
    const ib_var_store_t  *store
)
NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Const version of ib_var_target_expand().
 **/
ib_status_t DLL_PUBLIC ib_var_target_expand_const(
    const ib_var_target_t  *target,
    const ib_var_target_t **expanded,
    ib_mm_t                 mm,
    const ib_var_store_t   *store
)
NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Set target to a value.
 *
 * This function behaves differently depending on the target:
 * - Trivial: Equivalent to ib_var_source_set() except that @a field cannot
 *   be null.
 * - Simple: If source not set, will initialize a list field via
 *   ib_var_source_initialize().  Will then append @a field to source.
 * - Expand: Will expand and then as above.
 * - Regexp: Will return IB_EINVAL.
 *
 * Note that @a field cannot be NULL.  Use ib_var_target_remove() instead.
 *
 * @param[in] target Target to set.
 * @param[in] mm     Memory manager to use.
 * @param[in] store  Store to use.
 * @param[in] field  Field to set value to.  Will have name overridden based on
 *                   filter.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if target is regexp or expands to regexp.
 * - IB_EINVAL if target is simple or expands to simple and source is not a
 *   list or is dynamic.
 * - IB_EINVAL if config of @a store does not match that of @a target.
 **/
ib_status_t DLL_PUBLIC ib_var_target_set(
    ib_var_target_t *target,
    ib_mm_t          mm,
    ib_var_store_t  *store,
    ib_field_t      *field
)
NONNULL_ATTRIBUTE(1, 3, 4);

/**
 * As ib_var_target_set() but removes any existing values first.
 *
 * This helper function is equivalent to calling ib_var_target_remove()
 * followed by ib_var_target_set().
 *
 * @param[in] target Target to remove and set.
 * @param[in] mm     Memory manager to use.
 * @param[in] store  Store to use.
 * @param[in] field  Field to set value to.  Will have name overridden based on
 *                   filter.
 * @return
 * - As described in ib_var_target_remove() and ib_var_target_set().
 **/
ib_status_t DLL_PUBLIC ib_var_target_remove_and_set(
    ib_var_target_t *target,
    ib_mm_t          mm,
    ib_var_store_t  *store,
    ib_field_t      *field
)
NONNULL_ATTRIBUTE(1, 3, 4);

/** @} */

/**
 * @defgroup IronBeeEngineVarExpand Var Expand
 *
 * Expand string by substituting references to vars.
 *
 * An expandable string may containing var references via `%{target}`.  When
 * expanded, all targets are replaced with stringified versions of their
 * values, or the empty string if they do not exist.
 *
 * Expandable strings should be converted, as early as possible, to
 * @ref ib_var_expand_t.  These can then be executed to gain expanded strings
 * when needed.
 *
 * @{
 **/

/**
 * Acquire a string expansion.
 *
 * When executed with ib_var_expand_execute(), any references to targets of
 * the form `%{target}` will be replaced with the result of that target
 * (if possible).
 *
 * @param[out] expand        Resulting string expansion preparation.  Lifetime
 *                           will equal @a mp.
 * @param[in]  mm            Memory manager to use.
 * @param[in]  str           String to expand.
 * @param[in]  str_length    Length of @a str.
 * @param[in]  config        Config to expand from.
 * @param[out] error_message Error message from filter preparation.  May be
 *                           NULL.
 * @param[out] error_offset  Error offset from filter preparation. May be
 *                           NULL.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation error.
 * - IB_EINVAL if @a target_string is invalid (see ib_var_filter_acquire()).
 **/
ib_status_t DLL_PUBLIC ib_var_expand_acquire(
    ib_var_expand_t       **expand,
    ib_mm_t                 mm,
    const char             *str,
    size_t                  str_length,
    const ib_var_config_t  *config,
    const char            **error_message,
    int                    *error_offset
)
NONNULL_ATTRIBUTE(1, 3, 5);

/**
 * Execute prepared string expansion to get expanded string.
 *
 * @note Errors that occur during target expansion do not cause this method
 * to fail.  Instead, they result in expansion of the target at issue into
 * an error message in the string.
 *
 * @param[in]  expand     String expansion to expand.
 * @param[out] dst        Expanded string.  Lifetime will equal @a mp.
 * @param[out] dst_length Length of @a dst.
 * @param[in]  mm         Memory manager to use.
 * @param[in]  store      Store to use.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_var_expand_execute(
    const ib_var_expand_t  *expand,
    const char            **dst,
    size_t                 *dst_length,
    ib_mm_t                 mm,
    const ib_var_store_t   *store
)
NONNULL_ATTRIBUTE(1, 2, 3, 5);

/**
 * Check if @a str has expansions.
 *
 * This looks for a substring of the form `%{...}` for any `...`.
 *
 * @param[in] str String to test.
 * @param[in] str_length Length of @a str.
 * @return true iff string contains target expansion expression.
 **/
bool DLL_PUBLIC ib_var_expand_test(
    const char *str,
    size_t      str_length
)
NONNULL_ATTRIBUTE(1);

/** @} */

/**
 * @} IronBeeEngineVar
 */

#ifdef __cplusplus
}
#endif

#endif
