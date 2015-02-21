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

#ifndef _IB_LOGEVENT_H_
#define _IB_LOGEVENT_H_

/**
 * @file
 * @brief IronBee --- Event Logger
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine.h>
#include <ironbee/log.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineLogEvent Event Logging
 * @ingroup IronBeeEngine
 * @{
 */

/** Log Event Type */
typedef enum {
    IB_LEVENT_TYPE_UNKNOWN,          /**< Unknown type */
    IB_LEVENT_TYPE_OBSERVATION,      /**< Observation event */
    IB_LEVENT_TYPE_ALERT             /**< Alert event */
} ib_logevent_type_t;

/** Log Event Action */
typedef enum {
    IB_LEVENT_ACTION_UNKNOWN,
    IB_LEVENT_ACTION_LOG,
    IB_LEVENT_ACTION_BLOCK,
    IB_LEVENT_ACTION_IGNORE,
    IB_LEVENT_ACTION_ALLOW,
} ib_logevent_action_t;

/**
 * Log Event suppression states.
 *
 * Events may be suppressed for different reasons.
 */
typedef enum {
    IB_LEVENT_SUPPRESS_NONE,         /**< Not suppressed. */
    IB_LEVENT_SUPPRESS_FPOS,         /**< False positive. */
    IB_LEVENT_SUPPRESS_REPLACED,     /**< Replaced by later event. */
    IB_LEVENT_SUPPRESS_INC,          /**< Event is partial/incomplete. */
    IB_LEVENT_SUPPRESS_OTHER         /**< Other reason. */
} ib_logevent_suppress_t;

/** Log Event Structure */
struct ib_logevent_t {
    ib_mm_t                  mm;         /**< Memory manager */
    const char              *rule_id;    /**< Formatted rule ID */
    const char              *msg;        /**< Event message */
    ib_list_t               *tags;       /**< List of tag strings */
    uint32_t                 event_id;   /**< Event ID */
    ib_logevent_type_t       type;       /**< Event type */
    ib_logevent_action_t     rec_action; /**< Recommended action */
    ib_logevent_suppress_t   suppress;   /**< Suppress this event. */
    const void              *data;       /**< Event data */
    size_t                   data_len;   /**< Event data size */
    uint8_t                  confidence; /**< Event confidence (percent) */
    uint8_t                  severity;   /**< Event severity (0-100?) */
};

/**
 * Add an event to be logged.
 *
 * @note This function generates a logevent event.
 * see ib_engine_notify_logevent().
 *
 * @param[in,out] tx Transaction
 * @param[in] le Event
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_add(
    ib_tx_t                *tx,
    ib_logevent_t          *le);

/**
 * Remove an event from the queue before it is logged.
 *
 * @note This function generates a logevent event.
 * see ib_engine_notify_logevent().
 *
 * @param[in,out] tx Transaction
 * @param[in] id Event id
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_remove(
    ib_tx_t                *tx,
    uint32_t                id);

/**
 * Get a list of pending events to be logged.
 *
 * @note The list can be modified directly.
 * @note This function does not generate a logevent event.
 * see ib_engine_notify_logevent().
 *
 * @param[in] tx Transaction
 * @param[out] pevents Address where list of events is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_get_all(
    ib_tx_t                *tx,
    ib_list_t             **pevents);

/**
 * Get the last event from @a tx.
 *
 * @param[in] tx The transaction to fetch the event from.
 * @param[out] event The last event is written here.
 *
 * @returns
 * - IB_OK Success.
 * - IB_ENOENT There are no transactions in tx yet.
 */
ib_status_t ib_logevent_get_last(
    ib_tx_t        *tx,
    ib_logevent_t **event
)
ALL_NONNULL_ATTRIBUTE;

/**
 * Write out any pending events to the log.
 *
 * @param[in] tx Transaction
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_write_all(
    ib_tx_t                *tx);

/**
 * Lookup log event type name.
 *
 * @param[in] num Numeric ID
 *
 * @returns String name
 */
const DLL_PUBLIC char *ib_logevent_type_name(
    ib_logevent_type_t      num);

/**
 * Lookup log event action name.
 *
 * @param[in] num Numeric ID
 *
 * @returns String name
 */
const DLL_PUBLIC char *ib_logevent_action_name(
    ib_logevent_action_t    num);

/**
 * Lookup log suppression name.
 *
 * @param[in] num Numeric ID
 *
 * @returns String name
 */
const char *ib_logevent_suppress_name(
    ib_logevent_suppress_t num);

/**
 * Create a logevent.
 *
 * @param[out] ple Address which new logevent is written
 * @param[in]  mm Memory manager to allocate from
 * @param[in]  rule_id Rule ID string
 * @param[in]  type Event type
 * @param[in]  rec_action Event recommended action
 * @param[in]  confidence Event confidence
 * @param[in]  severity Event severity
 * @param[in]  fmt Event message format string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_create(
    ib_logevent_t         **ple,
    ib_mm_t                 mm,
    const char             *rule_id,
    ib_logevent_type_t      type,
    ib_logevent_action_t    rec_action,
    uint8_t                 confidence,
    uint8_t                 severity,
    const char             *fmt,
    ...
) PRINTF_ATTRIBUTE(8, 9);

/**
 * Add a tag to the event.
 *
 * @note This function does not generate a logevent event.
 * see ib_engine_notify_logevent().
 *
 * @param[in,out] le Log event
 * @param[in] tag Tag to add (string will be copied)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_tag_add(
    ib_logevent_t          *le,
    const char             *tag);

/**
 * Set data for the event.
 *
 * @note This function does not generate a logevent event.
 * see ib_engine_notify_logevent().
 *
 * @param[in,out] le Log event
 * @param[in] data Arbitrary binary data
 * @param[in] dlen Data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_data_set(
    ib_logevent_t          *le,
    const void             *data,
    size_t                  dlen);

/**
 * Set the suppression for the event.
 *
 * @note This function does not generate a logevent event.
 * see ib_engine_notify_logevent().
 *
 * @param[in,out] le Log event
 * @param[in] suppress Suppression setting for the event
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_logevent_suppress_set(
    ib_logevent_t          *le,
    ib_logevent_suppress_t  suppress);

/**
 * @} IronBeeEngineLogEvent
 */

#ifdef __cplusplus
}
#endif

#endif
