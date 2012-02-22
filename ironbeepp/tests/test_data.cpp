#include "data.hpp"

#include <ironbee/types.h>

#include "gtest/gtest.h"

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

struct destruction_registerer
{
    explicit destruction_registerer( bool* flag ) : m_flag( flag ) {}
    ~destruction_registerer() { *m_flag = true; }

    bool* m_flag;
};

typedef boost::shared_ptr<destruction_registerer> destruction_registerer_p;

TEST( TestCatch, ironbeepp_exception )
{
    using namespace IronBee::Internal;

    ib_mpool_t* mp;

    ib_status_t rc;
    rc = ib_mpool_create( &mp, NULL, NULL );
    ASSERT_EQ( IB_OK, rc );

    bool flag = false;
    destruction_registerer_p it =
        boost::make_shared<destruction_registerer>( &flag );

    void* data;

    data = value_to_data( it, mp );
    ASSERT_TRUE( data );

    destruction_registerer_p other;

    ASSERT_NO_THROW(
        other = data_to_value<destruction_registerer_p>( data )
    );

    ASSERT_EQ( it, other );
    ASSERT_EQ( 3, other.use_count() );
    ASSERT_EQ( 3, it.use_count() );
    other.reset();
    ASSERT_EQ( 2, it.use_count() );
    it.reset();

    ASSERT_THROW( data_to_value<int>( data ), IronBee::einval );

    ASSERT_FALSE( flag );
    ib_mpool_destroy( mp );

    ASSERT_TRUE( flag );
}
