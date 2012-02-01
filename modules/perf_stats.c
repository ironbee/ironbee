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

/* Event info structure */
typedef struct {
    int          number;
    int          cbdata_type;
    const char  *name;
} event_info_t;

static event_info_t event_info[IB_STATE_EVENT_NUM];

/* Perf info structure */
typedef struct {
    int           number;
    int           cbdata_type;
    const char   *name;
    uint64_t      call_cnt;
    uint64_t      total_msec;
    uint64_t      max_msec;
    uint64_t      start_msec;
    uint64_t      stop_msec;
} perf_info_t;

uint64_t get_time_stamp_us(void);

static perf_info_t *get_perf_info(ib_engine_t *,void *, int);

/**
 * @internal
 * Get a microsecond ts  
 *
 * Returns a timestamp as uint64_t from CLOCK_MONOTONIC_RAW or CLOCK_MONOTINIC.
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
        
        /* Ticks seem to be an undesireable due for many reasons.
         * IB_CLOCK is set to CLOCK_MONOTONIC which is vulnerable to slew or
         * if avaliable set to CLOCK_MONOTONIC_RAW which does not suffer from slew.
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
 * Perf Event Start Event Callback.
 *
 * On a connection started event we register connection 
 * counters for the connection.
 *
 * @param[in] ib IronBee object
 * @param[in] connp Connection object
 * @param[in] cbdata Callback data: acutally an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_reg_conn_counter(ib_engine_t *ib,
                                                   ib_conn_t *connp,
                                                   void *cbdata)
{
    IB_FTRACE_INIT(mod_perf_stats_reg_conn_counter);

    perf_info_t *perf_info;
    event_info_t *eventp = (event_info_t *)cbdata;
    int cevent = eventp->number;
    int event;
    int rc;

    perf_info = ib_mpool_alloc(connp->mp, sizeof(*perf_info) * IB_STATE_EVENT_NUM);

    for (event = 0; event < IB_STATE_EVENT_NUM; ++event) {
        if ((eventp->cbdata_type == IB_CBDATA_NONE) ||
            (eventp->cbdata_type == IB_CBDATA_CONN_DATA_T)) {
            ib_log_error(ib, 4, "Cannot collect stats for:%d name:%s cbdata_type: %d",
                         eventp->number, eventp->name, eventp->cbdata_type);
        }
        else 
        {
            perf_info_t *perfp = &perf_info[event];

            /* Does this event match conn_started_event? 
             * If so we should init counters for this event.
             */
            if (event == cevent) {
                perfp->call_cnt = 1;
                perfp->start_msec = get_time_stamp_us();
            }
            else {
                perfp->call_cnt = 0;
                perfp->start_msec = 0;
            } 
          
            /* Setup other defaults */
            perfp->number = event;
            perfp->name = ib_state_event_name((ib_state_event_type_t)event);
            perfp->cbdata_type = ib_state_event_cbdata_type(event); 
            perfp->max_msec = 0;
            perfp->total_msec = 0;
            perfp->stop_msec = 0;

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
 * @internal
 * Perf Stats Start Event Callback.
 *
 * Handles all callback events other than conn_start_event. Here we set start times 
 * and incriment the call counter.
 *
 * @param[in] ib IronBee object
 * @param[in] param Callback data which differs based on hook type.
 * @param[in] cbdata Callback data: acutally an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_start_callback(ib_engine_t *ib,
                                                       void *param,
                                                       void *cbdata)
{

    IB_FTRACE_INIT(mod_perf_stats_event_start_callback);

    event_info_t *eventp = (event_info_t *)cbdata; /* Handler is generic lets tell it about the event. */
    int cevent = eventp->number;                   /* Current event number. */
    int cbdata_type = eventp->cbdata_type;         /* Current callback data type. */
    perf_info_t *perf_info;                        /* Storage for perf data from connection hash. */
    perf_info_t *perfp;                            /* Perf data on current event. */

    /* Get data back from the connection hash. */
    perf_info = get_perf_info(ib,param,cbdata_type);

    if (perf_info != NULL) {
        /* Set perfp to current event type. */
        perfp = &perf_info[cevent];

        /* Set the start time for event */
        perfp->start_msec = get_time_stamp_us();

        /* Incriment the call counter */
        perfp->call_cnt++;

        ib_log_debug(ib, 4, "Start Callback: %s (%llu) (%llu) ",
                     perfp->name, perfp->call_cnt, perfp->start_msec);
    }
    else
    {
        ib_log_debug(ib, 4, "Connection based perf_info is NULL");
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Perf Stats Stop Event Callback.
 *
 * Called at the end of an event. Counters are updaated and displayed.
 *
 * @param[in] ib IronBee object
 * @param[in] param Callback data which differs based on hook type.
 * @param[in] cbdata Callback data: acutally an perf_info_t describing the
 * event.
 */
static ib_status_t mod_perf_stats_event_stop_callback(ib_engine_t *ib,
                                                      void *param,
                                                      void *cbdata)
{
    
    IB_FTRACE_INIT(mod_perf_stats_event_stop_callback);

    event_info_t *eventp = (event_info_t *)cbdata; /* Handler is generic lets tell it about the event. */
    int cevent = eventp->number;                   /* Current event number. */
    int cbdata_type = eventp->cbdata_type;         /* Current callback data type. */
    perf_info_t *perf_info;                        /* Storage for perf data from connection hash. */
    perf_info_t *perfp;                            /* Perf data on current event. */
    uint64_t time_taken;                           /* Temp storage for time the event took */

    /* Get data back from the connection hash */
    perf_info = get_perf_info(ib,param,cbdata_type);
   
    if (perf_info != NULL) {
        perfp = &perf_info[cevent];

        /* Set the stop time for the event. */
        perfp->stop_msec = get_time_stamp_us();

        /* Get the msec the event took. */
        time_taken = (perfp->stop_msec - perfp->start_msec);

        /* Update total time spent on event. */
        perfp->total_msec += time_taken;

        /* Update max time taken for event if needed. */
        if (time_taken > perfp->max_msec) {
            perfp->max_msec = time_taken;
        }

        ib_log_debug(ib, 4, "Stop Callback: %s call_cnt:(%llu) start:(%llu) "
                     "stop:(%llu) took:(%llu) conn total:(%llu) max:(%llu)",
                     perfp->name, perfp->call_cnt, perfp->start_msec, 
                     perfp->stop_msec, time_taken, perfp->total_msec, 
                     perfp->max_msec);
    }
    else {
        ib_log_debug(ib, 4, "Connection based perf_info is NULL");
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Given @ib_txdata_t, @ib_tx_t, @ib_conn_t, or @ib_conndata_t
 * return the performance info associated with it.
 *
 * @param[in] hook callback data 
 * @param[in] call back data type
 */
static perf_info_t *get_perf_info(ib_engine_t *ib, void *param, int cbdata_type){
    /* storage from hash */
    perf_info_t *perf_info;

    int rc;

    /* Fetch data from connection hash depending on callback data type */
    if (cbdata_type == IB_CBDATA_CONN_T) {
        ib_conn_t *connp = (ib_conn_t *)param;;
        rc = ib_hash_get(connp->data, "MOD_PERF_STATS",
                         (void *)&perf_info);
    } 
    else if (cbdata_type == IB_CBDATA_CONN_DATA_T) {
        ib_conndata_t *connd = (ib_conndata_t *)param;
        rc = ib_hash_get(connd->conn->data, "MOD_PERF_STATS",
                         (void *)&perf_info);
    } 
    else if (cbdata_type == IB_CBDATA_TX_T) {
        ib_tx_t *tx = (ib_tx_t *)param;
        rc = ib_hash_get(tx->conn->data, "MOD_PERF_STATS",
                         (void *)&perf_info);
    } 
    else if (cbdata_type == IB_CBDATA_TX_DATA_T) {
        ib_txdata_t *txd = (ib_txdata_t *)param;
        rc = ib_hash_get(txd->tx->conn->data, "MOD_PERF_STATS",
                         (void *)&perf_info);
    }
    else {
        ib_log_error(ib,3,"Unknown Callback data type (%d)", cbdata_type);
        return NULL;
    }

    if (rc != IB_OK) {
        return NULL;
    }

    return perf_info;
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

    IB_FTRACE_INIT(perf_stats_init);
    ib_log_debug(ib, 4, "Perf stats module loaded.");
    ib_status_t rc;
    int event;
    
    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    for (event = 0; event < IB_STATE_EVENT_NUM; ++event) {
        ib_void_fn_t  handler = NULL;
         
        event_info_t *eventp = &event_info[event];

        /* Record event info */
        eventp->number = event;
        eventp->name = ib_state_event_name((ib_state_event_type_t)event);
        eventp->cbdata_type = ib_state_event_cbdata_type(event);

        /* init the per connection counters here. 
         * Otherwise use callback data type.
         */ 
        if (event == conn_started_event) { 
            handler = (ib_void_fn_t)mod_perf_stats_reg_conn_counter;
        } 
        else if ((eventp->cbdata_type == IB_CBDATA_NONE) || 
                 (eventp->cbdata_type == IB_CBDATA_CONN_DATA_T)) {
            ib_log_error(ib, 4, "Cannot register handler "
                         "for:%d name:%s cbdata_type: %d",
                         eventp->number, eventp->name, eventp->cbdata_type);
        }
        else {
            handler = (ib_void_fn_t)mod_perf_stats_event_start_callback; 
        }

        /* Might have a NONE call back data type for cfg parser.
         * If this is the case don't set a handler 
         */
        if (handler != NULL) {
            rc = ib_hook_register(ib, (ib_state_event_type_t)event,
                                  handler,(void*)eventp);
            if (rc != IB_OK) {
                ib_log_error(ib, 4, "Hook register for"
                             "event:%d name:%s cbdata_type: %d returned %d",
                             eventp->number, eventp->name, 
                             eventp->cbdata_type, rc);
            }
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
    IB_FTRACE_INIT(perf_stats_context_init);
    ib_status_t rc;
    int event;

    /* Check that we are in the main ctx otherwise return */
    if (ctx == ib_context_main(ib))
    {
        for (event = 0; event < IB_STATE_EVENT_NUM; ++event) {
            ib_void_fn_t  handler = NULL;
            event_info_t *eventp = &event_info[event];

            if ((eventp->cbdata_type == IB_CBDATA_NONE) ||
                (eventp->cbdata_type == IB_CBDATA_CONN_DATA_T)) {
                ib_log_error(ib, 4, "Cannot register handler "
                             "for:%d name:%s cbdata_type: %d",
                             eventp->number, eventp->name, eventp->cbdata_type);
            }
            else {
                handler = (ib_void_fn_t)mod_perf_stats_event_stop_callback;
            }

            if (handler != NULL){
                rc = ib_hook_register(ib, (ib_state_event_type_t)event,
                                      handler,(void*)eventp);
                if (rc != IB_OK) {
                    ib_log_error(ib, 4, "Hook register for "
                                 "event:%d name:%s cbdata_type: %d returned %d",
                                 eventp->number, eventp->name, 
                                 eventp->cbdata_type, rc);
                }
            }

        } 
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Called when module is unloaded. */
static ib_status_t perf_stats_fini(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(perf_stats_fini);
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

