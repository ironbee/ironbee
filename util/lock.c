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
 * @brief IronBee - Lock Utiities
 * @author Sam Baskinger <sbaskinger@qualys.com>
 *
 * @internal
 */


#include "ironbee/lock.h"

#include "ironbee/core.h"
#include "ironbee/debug.h"
#include "ironbee/types.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

ib_status_t ib_lock_init(ib_lock_t *lock)
{
    IB_FTRACE_INIT(ib_lock_init);

    int rc;

    /* Snipped from the Linux man page semctl(2). */
    union semun {
        int              val;    /* Value for SETVAL */
        struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
        unsigned short  *array;  /* Array for GETALL, SETALL */
        struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
    } sem_val;

    /* Initialize semaphore */
    sem_val.val=0;

    *lock = semget(IPC_PRIVATE, 1, S_IRUSR|S_IWUSR);

    if (*lock == IB_LOCK_UNINITIALIZED) {
        fprintf(stderr,
          "Failed to initialize runtime lock - %s\n", strerror(errno));
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = semctl(*lock, 0, SETVAL, sem_val);

    if (rc == IB_LOCK_UNINITIALIZED) {
        fprintf(stderr,
          "Failed to initialize runtime lock - %s\n", strerror(errno));
        semctl(*lock, 0, IPC_RMID);
        *lock = IB_LOCK_UNINITIALIZED;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }


    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lock_lock(ib_lock_t *lock)
{
    IB_FTRACE_INIT(ib_lock_lock);

    /* Return code from system calls. */
    int sys_rc;

    struct sembuf lock_sops[2];

    /* Wait for 0. */
    lock_sops[0].sem_num = 0;
    lock_sops[0].sem_op = 0;
    lock_sops[0].sem_flg = 0;

    /* Increment, taking ownership of the semaphore. */
    lock_sops[1].sem_num = 0;
    lock_sops[1].sem_op = 1;
    lock_sops[1].sem_flg = 0;

    if (*lock==IB_LOCK_UNINITIALIZED) {
        fprintf(stderr, "Attempt to lock uninitialized lock.\n");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    sys_rc = semop(*lock, lock_sops, 2);

    /* Report semop error and return. */
    if (sys_rc == IB_LOCK_UNINITIALIZED) {
        fprintf(stderr, "Failed to lock sem %d - %s.\n", *lock,
                     strerror(errno));
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lock_unlock(ib_lock_t *lock)
{
    IB_FTRACE_INIT(ib_lock_unlock);

    /* Return code from system calls. */
    int sys_rc;

    struct sembuf unlock_sop;

    /* To unlock, decrement to 0. */
    unlock_sop.sem_num = 0;
    unlock_sop.sem_op = -1;
    unlock_sop.sem_flg = 0;

    if (*lock==IB_LOCK_UNINITIALIZED) {
        fprintf(stderr, "Attempt to unlock uninitialized lock.\n");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    sys_rc = semop(*lock, &unlock_sop, 1);

    /* Report semop error and return. */
    if (sys_rc == -1) {
        fprintf(stderr, "Failed to unlock sem %d - %s.\n", *lock, strerror(errno));
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lock_destroy(ib_lock_t *lock)
{
    IB_FTRACE_INIT(ib_lock_destroy);

    int rc;

    if (*lock != IB_LOCK_UNINITIALIZED) {
        rc = semctl(*lock, 0, IPC_RMID);

        if (rc != -1) {
            *lock = IB_LOCK_UNINITIALIZED;
        }
        else {
            fprintf(stderr, "Failed to clean up semaphore %d.\n"
                            "Please remove it manually with ipcrm or similar.",
                            *lock);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

