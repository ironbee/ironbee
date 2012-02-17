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
 * @brief IronBee - Debug Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/debug.h>

#include <stdio.h>
#include <inttypes.h>

#ifdef IB_DEBUG

static FILE *ib_trace_fh;

/* --Tracing -- */

void ib_trace_init(const char *fn)
{
    if (fn != NULL) {
        ib_trace_fh = fopen(fn, "w");
    }
    else {
        ib_trace_fh = stderr;
    }
}

void ib_trace_msg(const char *file,
                  int line,
                  const char *func,
                  const char *msg)
{
    const char *sep = func?"() - ":"";

    fprintf(ib_trace_fh, "IronBee TRACE [%s:%d]: %s%s%s\n",
            file, line,
            (func?func:""), sep, msg);
    fflush(ib_trace_fh);
}

void ib_trace_num(const char *file,
                  int line,
                  const char *func,
                  const char *msg,
                  intmax_t num)

{
    const char *sep = func?"() - ":"";
    const char *sep2 = msg?" ":"";

    fprintf(ib_trace_fh, "IronBee TRACE [%s:%d]: %s%s%s%s%" PRIuMAX " (0x%" PRIxMAX ")\n",
            file, line,
            (func?func:""), sep,
            (msg?msg:""), sep2,
            num, num);
    fflush(ib_trace_fh);
}

void ib_trace_unum(const char *file,
                   int line,
                   const char *func,
                   const char *msg,
                   uintmax_t unum)

{
    const char *sep = func?"() - ":"";
    const char *sep2 = msg?" ":"";

    fprintf(ib_trace_fh, "IronBee TRACE [%s:%d]: %s%s%s%s%" PRIuMAX " (0x%" PRIxMAX ")\n",
            file, line,
            (func?func:""), sep,
            (msg?msg:""), sep2,
            unum, unum);
    fflush(ib_trace_fh);
}

void ib_trace_ptr(const char *file,
                  int line,
                  const char *func,
                  const char *msg,
                  void *ptr)

{
    const char *sep = func?"() - ":"";
    const char *sep2 = msg?" ":"";

    fprintf(ib_trace_fh, "IronBee TRACE [%s:%d]: %s%s%s%s%p\n",
            file, line,
            (func?func:""), sep,
            (msg?msg:""), sep2,
            ptr);
    fflush(ib_trace_fh);
}

void ib_trace_str(const char *file,
                  int line,
                  const char *func,
                  const char *msg,
                  const char *str)

{
    const char *sep = func?"() - ":"";
    const char *sep2 = msg?" ":"";
    const char *real_str = (str != NULL) ? str : "(null)";

    fprintf(ib_trace_fh, "IronBee TRACE [%s:%d]: %s%s%s%s\"%s\"\n",
            file, line,
            (func?func:""), sep,
            (msg?msg:""), sep2,
            real_str);
    fflush(ib_trace_fh);
}

#endif /* IB_DEBUG */

