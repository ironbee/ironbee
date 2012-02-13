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
 * @brief IronBee - Performance Measurement Module
 *
 * This module records performance stats
 *
 * @author William Metcalf <wmetcalf@qualys.com>
 */

/* We need this before time.h, so that we get the right prototypes. */
#include "ironbee_config_auto.h"

#include <time.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#include <ironbee/engine.h>
#include <ironbee/debug.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME               perf_stats
#define MODULE_NAME_STR           IB_XSTRINGIFY(MODULE_NAME)

#ifdef CLOCK_MONOTONIC_RAW
#define IB_CLOCK                  CLOCK_MONOTONIC_RAW
#else
#define IB_CLOCK                  CLOCK_MONOTONIC
#endif /* CLOCK_MONOTONIC_RAW */


/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/** Event info structure */
typedef struct {
    int          number;
    int          cbdata_type;
    const char  *name;
} event_info_t;

static event_info_t event_info[IB_STATE_EVENT_NUM];

/** Perf info structure */
typedef struct {
    int           number;
    int           cbdata_type;
    const char   *name;
    uint64_t      call_cnt;
    uint64_t      total_usec;
    uint64_t      max_usec;
    uint64_t      start_usec;
    uint64_t      stop_usec;
} perf_info_t;

/** Callback Data Type */
enum cb_data_type {
    IB_CBDATA_CONN_T,
    IB_CBDATA_CONN_DATA_T,
    IB_CBDATA_TX_T,
    IB_CBDATA_TX_DATA_T,
    IB_CBDATA_NONE,
};

uint64_t get_time_stamp_us(void);
int ib_state_event_cbdata_type(ib_state_event_type_t);

/**
 * @internal
 * Get a microsecond ts
 *
 * Returns a timestamp as uint64_t from CLOCK_MONOTONIC_RAW or CLOCK_MONOTONIC.
 *
 */
uint64_t get_time_stamp_us(void){
    uint64_t us;

#ifdef _DARWIN_C_SOURCE
    {
      struct timeval t;

      gettimeofday(&t, NULL);

      us = t.tv_sec*1e6 + t.tv_usec;
    }
#else
    {
        struct timespec t;

        /* Ticks seem to be an undesirable due for many reasons.
         * IB_CLOCK is set to CLOCK_MONOTONIC which is vulnerable to slew or
         * if available set to CLOCK_MONOTONIC_RAW which does not suffer from slew.
         *
         * timespec provides sec and nsec resolution so we have to convert to msec.
         */
        clock_gettime(IB_CLOCK,&t);

        /* There are 1 million microsecs in a sec.
         * There are 1000 nanosecs in a microsec
         */
        us = (uint64_t)((t.tv_sec * 1000000) + (t.tv_nsec / 1000));
    }
#endif
    return us;
}

/**
 * @internal
 * List of callback data types for event id to type lookups.
 */
static int ib_state_event_name_cbdata_type_list[] = {
    /* Engine States */
    IB_CBDATA_CONN_T,      /**< handle_conn_started_event */
    IB_CBDATA_CONN_T,      /**< handle_conn_finished_event */
    IB_CBDATA_TX_T,        /**< handle_context_tx_started_event */
    IB_CBDATA_TX_T,        /**< handle_context_tx_process_event */
    IB_CBDATA_TX_T,        /**< handle_context_tx_finished_event */

    /* Handler States */
    IB_CBDATA_CONN_T,      /**< handle_context_conn_event */
    IB_CBDATA_CONN_T,      /**< handle_connect_event */
    IB_CBDATA_TX_T,        /**< handle_context_tx_event */
    IB_CBDATA_TX_T,        /**< handle_request_headers_event */
    IB_CBDATA_TX_T,        /**< handle_request_event */
    IB_CBDATA_TX_T,        /**< handle_response_headers_event */
    IB_CBDATA_TX_T,        /**< handle_response_event */
    IB_CBDATA_CONN_T,      /**< handle_disconnect_event */
    IB_CBDATA_TX_T,        /**< handle_postprocess_event */

    /* Plugin States */
    IB_CBDATA_NONE,        /**< cfg_started_event */
    IB_CBDATA_NONE,        /**< cfg_finished_event */
    IB_CBDATA_CONN_T,      /**< conn_opened_event */
    IB_CBDATA_CONN_DATA_T, /**< conn_data_in_event */
    IB_CBDATA_CONN_DATA_T, /**< conn_data_out_event */
    IB_CBDATA_CONN_T,      /**< conn_closed_event */

    /* Parser States */
    IB_CBDATA_TX_DATA_T,   /**< tx_data_in_event */
    IB_CBDATA_TX_DATA_T,   /**< tx_data_out_event */
    IB_CBDATA_TX_T,        /**< request_started_event */
    IB_CBDATA_TX_T,        /**< request_headers_event */
    IB_CBDATA_TX_T,        /**< request_body_event */
    IB_CBDATA_TX_T,        /**< request_finished_event */
    IB_CBDATA_TX_T,        /**< response_started_event */
    IB_CBDATA_TX_T,        /**< response_headers_event */
    IB_CBDATA_TX_T,        /**< response_body_event */
    IB_CBDATA_TX_T,        /**< response_finished_event */

    IB_CBDATA_NONE
};
int ib_state_event_cbdata_type(ib_state_event_type_t event)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_INT(ib_state_event_name_cbdata_type_list[event]);
}

/**
 * @internal
 * Perf Event Start Event Callback.
 *
 * On a connection started event we register connection
 * counters for the connection.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] connp Connection object
 * @param[in] cbdata Callback data: actually an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_reg_conn_counter(
     ib_engine_t *ib,
     ib_state_event_type_t event_type,
     ib_conn_t *connp,
     void *cbdata
)
{
    IB_FTRACE_INIT();

    perf_info_t *perf_info;
    event_info_t *eventp = (event_info_t *)cbdata;
    int cevent = eventp->number;
    int rc;
    int event;

    perf_info = ib_mpool_alloc(connp->mp, sizeof(*perf_info) * IB_STATE_EVENT_NUM);

    for (event = 0; event < IB_STATE_EVENT_NUM; ++event) {
        if ((eventp->cbdata_type == IB_CBDATA_NONE) ||
            (eventp->cbdata_type == IB_CBDATA_CONN_DATA_T)) {
            ib_log_error(ib, 4, "Cannot collect stats for:%d name:%s cbdata_type: %d",
                         eventp->number, eventp->name, eventp->cbdata_type);
        }
        else {
            perf_info_t *perfp = &perf_info[event];

            /* Does this event match conn_started_event?
             * If so we should init counters for this event.
             */
            if (event == cevent) {
                perfp->call_cnt = 1;
                perfp->start_usec = get_time_stamp_us();
            }
            else {
                perfp->call_cnt = 0;
                perfp->start_usec = 0;
            }

            /* Setup other defaults */
            perfp->number = event;
            perfp->name = ib_state_event_name((ib_state_event_type_t)event);
            perfp->cbdata_type = ib_state_event_cbdata_type(event);
            perfp->max_usec = 0;
            perfp->total_usec = 0;
            perfp->stop_usec = 0;

            ib_log_debug(ib, 4, "Perf callback registered %s (%d) (%d)",
                         perfp->name, perfp->number, perfp->cbdata_type);
        }
    }

    rc = ib_hash_set(connp->data, "MOD_PERF_STATS" ,perf_info);
    if (rc != IB_OK) {
        ib_log_debug(ib, 3, "Failed to store perf stats in connection data: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Handle event starts.
 *
 * Here we set start times and increment the call counter.
 *
 * \param[in] ib IronBee object.
 * \param[in] eventp Event info.
 * \param[in] perf_info Perf info.
 **/
static void mod_perf_stats_event_start(
    ib_engine_t *ib,
    event_info_t *eventp,
    perf_info_t *perf_info
)
{
    IB_FTRACE_INIT();

    int cevent = eventp->number;                   /* Current event number. */
    perf_info_t *perfp;                            /* Perf data on current event. */

    if (perf_info != NULL) {
        /* Set perfp to current event type. */
        perfp = &perf_info[cevent];

        /* Set the start time for event */
        perfp->start_usec = get_time_stamp_us();

        /* Increment the call counter */
        perfp->call_cnt++;

        ib_log_debug(ib, 4, "Start Callback: %s (%llu) (%llu) ",
                     perfp->name, perfp->call_cnt, perfp->start_usec);
    }
    else {
        ib_log_debug(ib, 4, "Connection based perf_info is NULL");
    }

    IB_FTRACE_RET_VOID();
}

/**
 * @internal
 * Handle event stops.
 *
 * Counters are updated and displayed.
 *
 * @param[in] ib IronBee object
 * \param[in] eventp Event info.
 * \param[in] perf_info Perf info.
 * event.
 */
static ib_status_t mod_perf_stats_event_stop(
     ib_engine_t *ib,
     event_info_t *eventp,
     perf_info_t *perf_info
)
{

    IB_FTRACE_INIT();

    int cevent = eventp->number;                   /* Current event number. */
    perf_info_t *perfp;                            /* Perf data on current event. */
    uint64_t time_taken;                           /* Temp storage for time the event took */

    if (perf_info != NULL) {
        perfp = &perf_info[cevent];

        /* Set the stop time for the event. */
        perfp->stop_usec = get_time_stamp_us();

        /* Get the msec the event took. */
        time_taken = (perfp->stop_usec - perfp->start_usec);

        /* Update total time spent on event. */
        perfp->total_usec += time_taken;

        /* Update max time taken for event if needed. */
        if (time_taken > perfp->max_usec) {
            perfp->max_usec = time_taken;
        }

        ib_log_debug(ib, 4, "Stop Callback: %s call_cnt:(%llu) start:(%llu) "
                     "stop:(%llu) took:(%llu) conn total:(%llu) max:(%llu)",
                     perfp->name, perfp->call_cnt, perfp->start_usec,
                     perfp->stop_usec, time_taken, perfp->total_usec,
                     perfp->max_usec);
    }
    else {
        ib_log_debug(ib, 4, "Connection based perf_info is NULL");
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Start Event conn Callback.
 *
 * Handles all conn callback events other than conn_start_event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] conn Connection.
 * @param[in] cbdata Callback data: actually @ref perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_start_conn_callback(
     ib_engine_t *ib,
     ib_state_event_type_t event,
     ib_conn_t* conn,
     void *cbdata
)
{
    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_start(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Start Event conndata Callback.
 *
 * Handles all conndata callback events other than conndata_start_event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] conndata Connection.
 * @param[in] cbdata Callback data: actually @ref perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_start_conndata_callback(
     ib_engine_t *ib,
     ib_state_event_type_t event,
     ib_conndata_t* conndata,
     void *cbdata
)
{
    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        conndata->conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_start(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Start Event tx Callback.
 *
 * Handles all tx callback events other than tx_start_event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] tx Connection.
 * @param[in] cbdata Callback data: actually @ref perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_start_tx_callback(
     ib_engine_t *ib,
     ib_state_event_type_t event,
     ib_tx_t* tx,
     void *cbdata
)
{
    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        tx->conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_start(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}
/**
 * @internal
 * Perf Stats Start Event txdata Callback.
 *
 * Handles all txdata callback events other than txdata_start_event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] txdata Connection.
 * @param[in] cbdata Callback data: actually @ref perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_start_txdata_callback(
     ib_engine_t *ib,
     ib_state_event_type_t event,
     ib_txdata_t* txdata,
     void *cbdata
)
{
    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        txdata->tx->conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_start(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Stop Event conn Callback.
 *
 * Called at the end of an event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] param Callback data which differs based on hook type.
 * @param[in] cbdata Callback data: actually an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_stop_conn_callback(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_conn_t* conn,
    void *cbdata
)
{

    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_stop(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Stop Event conndata Callback.
 *
 * Called at the end of an event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] param Callback data which differs based on hook type.
 * @param[in] cbdata Callback data: actually an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_stop_conndata_callback(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_conndata_t* conndata,
    void *cbdata
)
{

    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        conndata->conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_stop(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Stop Event tx Callback.
 *
 * Called at the end of an event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] param Callback data which differs based on hook type.
 * @param[in] cbdata Callback data: actually an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_stop_tx_callback(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_tx_t* tx,
    void *cbdata
)
{

    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        tx->conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_stop(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Stop Event txdata Callback.
 *
 * Called at the end of an event.
 *
 * @param[in] ib IronBee object
 * @param[in] event Event type
 * @param[in] param Callback data which differs based on hook type.
 * @param[in] cbdata Callback data: actually an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_stop_txdata_callback(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_txdata_t* txdata,
    void *cbdata
)
{

    IB_FTRACE_INIT();

    event_info_t *eventp = (event_info_t *)cbdata;
    perf_info_t *perf_info;

    ib_status_t rc = ib_hash_get(
        txdata->tx->conn->data,
        &perf_info,
        "MOD_PERF_STATS"
    );
    if (rc != IB_OK) {
        perf_info = NULL;
    }

    mod_perf_stats_event_stop(ib, eventp, perf_info);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Called when module is loaded
 * Start event hooks are registered here.
 *
 * @param[in] ib IronBee object
 * @param[in] m Module object
 */
static ib_status_t perf_stats_init(ib_engine_t *ib, ib_module_t *m)
{
    /*Detect main context otherwise return IB_ENGINE_CONTEXT_MAIN. */

    IB_FTRACE_INIT();
    ib_log_debug(ib, 4, "Perf stats module loaded.");
    ib_status_t rc;
    int event;

    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    for (event = 0; event < IB_STATE_EVENT_NUM; ++event) {
        event_info_t *eventp = &event_info[event];

        /* Record event info */
        eventp->number = event;
        eventp->name = ib_state_event_name((ib_state_event_type_t)event);
        eventp->cbdata_type = ib_state_event_cbdata_type(event);

        /* init the per connection counters here.
         * Otherwise use callback data type.
         */
        if (event == conn_started_event) {
            rc = ib_hook_conn_register(
                ib, (ib_state_event_type_t)event,
                mod_perf_stats_reg_conn_counter,
                (void*)eventp
            );
        }
        else if ((eventp->cbdata_type == IB_CBDATA_NONE) ||
                 (eventp->cbdata_type == IB_CBDATA_CONN_DATA_T))
        {
            rc = IB_EINVAL;
            ib_log_error(ib, 4, "Cannot register handler "
                         "for:%d name:%s cbdata_type: %d",
                         eventp->number, eventp->name, eventp->cbdata_type);
        }
        else
        {
            switch( ib_state_hook_type( (ib_state_event_type_t)event ) ) {
                case IB_STATE_HOOK_CONN:
                    rc = ib_hook_conn_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_start_conn_callback,
                        (void*)eventp
                    );
                    break;
                case IB_STATE_HOOK_CONNDATA:
                    rc = ib_hook_conndata_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_start_conndata_callback,
                        (void*)eventp
                    );
                    break;
                case IB_STATE_HOOK_TX:
                   rc = ib_hook_tx_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_start_tx_callback,
                        (void*)eventp
                    );
                    break;
                case IB_STATE_HOOK_TXDATA:
                    rc = ib_hook_txdata_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_start_txdata_callback,
                        (void*)eventp
                    );
                    break;
                default:
                    rc = IB_EINVAL;
                    ib_log_error(ib, 4, "Event with unknown hook type: %d/%s",
                                 eventp->number, eventp->name);

            }
        }

        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Hook register for"
                         "event:%d name:%s cbdata_type: %d returned %d",
                         eventp->number, eventp->name,
                         eventp->cbdata_type, rc);
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Initialize a context for the perf_stats module.
 *
 * This is a hack.
 * Hook callbacks set here to get end times for hooks.
 * Currently no other modules register hook call backs.
 * Because of this it should
 * ensure that we are the last thing called per event.
 *
 * @param[in] ib IronBee object
 * @param[in] m Module object
 * @param[in] ctx Context object
 */
static ib_status_t perf_stats_context_init(ib_engine_t *ib,
                                           ib_module_t *m,
                                           ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    int event;

    /* Check that we are in the main ctx otherwise return */
    if (ctx != ib_context_main(ib)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    for (event = 0; event < IB_STATE_EVENT_NUM; ++event) {
        event_info_t *eventp = &event_info[event];

        if ((eventp->cbdata_type == IB_CBDATA_NONE) ||
            (eventp->cbdata_type == IB_CBDATA_CONN_DATA_T))
        {
            rc = IB_EINVAL;
            ib_log_error(ib, 4, "Cannot register handler "
                         "for:%d name:%s cbdata_type: %d",
                         eventp->number, eventp->name, eventp->cbdata_type);
        }
        else {
            switch( ib_state_hook_type( (ib_state_event_type_t)event ) ) {
                case IB_STATE_HOOK_CONN:
                    rc = ib_hook_conn_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_stop_conn_callback,
                        (void*)eventp
                                               );
                    break;
                case IB_STATE_HOOK_CONNDATA:
                    rc = ib_hook_conndata_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_stop_conndata_callback,
                        (void*)eventp
                                                   );
                    break;
                case IB_STATE_HOOK_TX:
                    rc = ib_hook_tx_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_stop_tx_callback,
                        (void*)eventp
                                             );
                    break;
                case IB_STATE_HOOK_TXDATA:
                    rc = ib_hook_txdata_register(
                        ib,
                        (ib_state_event_type_t)event,
                        mod_perf_stats_event_stop_txdata_callback,
                        (void*)eventp
                                                 );
                    break;
                default:
                    rc = IB_EINVAL;
                    ib_log_error(ib, 4, "Event with unknown hook type: %d/%s",
                                 eventp->number, eventp->name);

            }
        }

        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Hook register for "
                         "event:%d name:%s cbdata_type: %d returned %d",
                         eventp->number, eventp->name,
                         eventp->cbdata_type, rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Called when module is unloaded. */
static ib_status_t perf_stats_fini(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT();
    ib_log_debug(ib, 4, "Perf stats module unloaded.");
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,      /* Default metadata */
    MODULE_NAME_STR,                /* Module name */
    IB_MODULE_CONFIG_NULL,          /* Global config data */
    NULL,                           /* Configuration field map */
    NULL,                           /* Config directive map */
    perf_stats_init,                /* Initialize function */
    perf_stats_fini,                /* Finish function */
    perf_stats_context_init,        /* Context init function */
    NULL                            /* Context fini function */
);

