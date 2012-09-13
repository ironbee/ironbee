//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Types utility tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/types.h>

#include <stdexcept>

TEST(TestTypes, test_status_to_string)
{
    ASSERT_STREQ("OK",        ib_status_to_string(IB_OK)        );
    ASSERT_STREQ("DECLINED",  ib_status_to_string(IB_DECLINED)  );
    ASSERT_STREQ("EUNKNOWN",  ib_status_to_string(IB_EUNKNOWN)  );
    ASSERT_STREQ("ENOTIMPL",  ib_status_to_string(IB_ENOTIMPL)  );
    ASSERT_STREQ("EINCOMPAT", ib_status_to_string(IB_EINCOMPAT) );
    ASSERT_STREQ("EALLOC",    ib_status_to_string(IB_EALLOC)    );
    ASSERT_STREQ("EINVAL",    ib_status_to_string(IB_EINVAL)    );
    ASSERT_STREQ("ENOENT",    ib_status_to_string(IB_ENOENT)    );
    ASSERT_STREQ("ETRUNC",    ib_status_to_string(IB_ETRUNC)    );
    ASSERT_STREQ("ETIMEDOUT", ib_status_to_string(IB_ETIMEDOUT) );
    ASSERT_STREQ("EAGAIN",    ib_status_to_string(IB_EAGAIN)    );
    ASSERT_STREQ("EOTHER",    ib_status_to_string(IB_EOTHER)    );
    ASSERT_STREQ("EBADVAL",   ib_status_to_string(IB_EBADVAL)   );
    ASSERT_STREQ("EEXIST",    ib_status_to_string(IB_EEXIST)    );

    ASSERT_STREQ("Unknown Status Code: 666",
                 ib_status_to_string((ib_status_t)666) );
}
