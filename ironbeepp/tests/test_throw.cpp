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

/**
 * @file
 * @brief IronBee++ Internals -- Throw Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "throw.hpp"
#include <ironbeepp/exception.hpp>

#include <ironbee/types.h>

#include "gtest/gtest.h"

using namespace IronBee;
using IronBee::Internal::throw_if_error;

TEST( TestThrow, basic )
{
    EXPECT_NO_THROW( throw_if_error( IB_OK ) );
    EXPECT_THROW( throw_if_error( IB_DECLINED  ), declined  );
    EXPECT_THROW( throw_if_error( IB_EUNKNOWN  ), eunknown  );
    EXPECT_THROW( throw_if_error( IB_ENOTIMPL  ), enotimpl  );
    EXPECT_THROW( throw_if_error( IB_EINCOMPAT ), eincompat );
    EXPECT_THROW( throw_if_error( IB_EALLOC    ), ealloc    );
    EXPECT_THROW( throw_if_error( IB_EINVAL    ), einval    );
    EXPECT_THROW( throw_if_error( IB_ENOENT    ), enoent    );
    EXPECT_THROW( throw_if_error( IB_ETRUNC    ), etrunc    );
    EXPECT_THROW( throw_if_error( IB_ETIMEDOUT ), etimedout );
    EXPECT_THROW( throw_if_error( IB_EAGAIN    ), eagain    );
    EXPECT_THROW( throw_if_error( IB_EOTHER    ), eother    );
}
