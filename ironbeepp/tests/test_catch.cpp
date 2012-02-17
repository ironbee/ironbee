#include <ironbeepp/internal/catch.hpp>

#include <ironbee/types.h>

#include "gtest/gtest.h"

#include <ironbee/debug.h>

struct other_boost_exception : public boost::exception, std::exception {};

TEST( TestCatch, ironbeepp_exception )
{
    using namespace IronBee;
    EXPECT_EQ( IB_DECLINED,  IBPP_TRY_CATCH( NULL, throw declined()  ) );
    EXPECT_EQ( IB_EUNKNOWN,  IBPP_TRY_CATCH( NULL, throw eunknown()  ) );
    EXPECT_EQ( IB_ENOTIMPL,  IBPP_TRY_CATCH( NULL, throw enotimpl()  ) );
    EXPECT_EQ( IB_EINCOMPAT, IBPP_TRY_CATCH( NULL, throw eincompat() ) );
    EXPECT_EQ( IB_EALLOC,    IBPP_TRY_CATCH( NULL, throw ealloc()    ) );
    EXPECT_EQ( IB_EINVAL,    IBPP_TRY_CATCH( NULL, throw einval()    ) );
    EXPECT_EQ( IB_ENOENT,    IBPP_TRY_CATCH( NULL, throw enoent()    ) );
    EXPECT_EQ( IB_ETRUNC,    IBPP_TRY_CATCH( NULL, throw etrunc()    ) );
    EXPECT_EQ( IB_ETIMEDOUT, IBPP_TRY_CATCH( NULL, throw etimedout() ) );
    EXPECT_EQ( IB_EAGAIN,    IBPP_TRY_CATCH( NULL, throw eagain()    ) );
    EXPECT_EQ( IB_EOTHER,    IBPP_TRY_CATCH( NULL, throw eother()    ) );
}

TEST( TestCatch, boost_exception )
{
    EXPECT_EQ(
        IB_EUNKNOWN,
        IBPP_TRY_CATCH( NULL, throw other_boost_exception() )
    );
}

TEST( TestCatch, std_exception )
{
    using namespace std;
    EXPECT_EQ(
        IB_EUNKNOWN,
        IBPP_TRY_CATCH( NULL, throw runtime_error( "" ) )
    );
    EXPECT_EQ(
        IB_EINVAL,
        IBPP_TRY_CATCH( NULL, throw invalid_argument( "" ) )
    );
}

TEST( TestCatch, bad_alloc )
{
    using namespace std;
    EXPECT_EQ(
        IB_EALLOC,
        IBPP_TRY_CATCH( NULL, throw std::bad_alloc() )
    );
}
