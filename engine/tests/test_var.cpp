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
 * @brief IronBee --- Var Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/var.h>

#include <ironbeepp/field.hpp>
#include <ironbeepp/memory_pool.hpp>

#include "gtest/gtest.h"

using IronBee::ScopedMemoryPool;
using IronBee::MemoryPool;

using namespace std;

namespace {

ib_var_config_t* make_config(ib_mpool_t *mp)
{
    ib_var_config_t* config = NULL;
    ib_status_t rc;

    rc = ib_var_config_create(&config, mp);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(config);
    if (! config) {return NULL;}
    EXPECT_EQ(mp, ib_var_config_pool(config));

    return config;
}

ib_var_store_t* make_store(ib_var_config_t* config)
{
    ib_var_store_t* store = NULL;
    ib_status_t rc;
    ib_mpool_t* mp = ib_var_config_pool(config);

    rc = ib_var_store_create(&store, mp, config);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(store);
    if (! store) {return NULL;}
    EXPECT_EQ(config, ib_var_store_config(store));
    EXPECT_EQ(mp, ib_var_store_pool(store));

    return store;
}

ib_var_source_t* make_source(
    ib_var_config_t *config,
    const string& name
)
{
    ib_var_source_t* source;
    ib_status_t rc;

    rc = ib_var_source_register(
        &source,
        config,
        name.data(),
        name.length(),
        IB_PHASE_NONE,
        IB_PHASE_NONE
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_TRUE(source);

    return source;
}

}

TEST(TestVar, Config)
{
    ScopedMemoryPool smp;
    ib_mpool_t* mp = MemoryPool(smp).ib();

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);
}

TEST(TestVar, Store)
{
    ScopedMemoryPool smp;
    ib_mpool_t* mp = MemoryPool(smp).ib();

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);
    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);
}

TEST(TestVar, SourceBasic)
{
    ScopedMemoryPool smp;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t *source = NULL;
    ib_status_t rc;

    rc = ib_var_source_register(
        &source,
        config,
        "test",
        sizeof("test") - 1,
        IB_PHASE_REQUEST_HEADER,
        IB_PHASE_REQUEST_BODY
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(source);

    EXPECT_EQ(config, ib_var_source_config(source));
    {
        const char* n;
        size_t nlen;

        ib_var_source_name(source, &n, &nlen);
        EXPECT_EQ(string("test"), string(n, nlen));
    }
    EXPECT_EQ(IB_PHASE_REQUEST_HEADER, ib_var_source_initial_phase(source));
    EXPECT_EQ(IB_PHASE_REQUEST_BODY, ib_var_source_final_phase(source));
    EXPECT_TRUE(ib_var_source_is_indexed(source));
}

TEST(TestVar, SourceRegisterInvalid)
{
    ScopedMemoryPool smp;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    make_source(config, "a");

    ib_status_t rc;
    ib_var_source_t* source = NULL;

    rc = ib_var_source_register(
        &source,
        config,
        "a", 1,
        IB_PHASE_REQUEST_HEADER,
        IB_PHASE_REQUEST_BODY
    );
    ASSERT_EQ(IB_EEXIST, rc);
    ASSERT_FALSE(source);

    source = NULL;
    rc = ib_var_source_register(
        &source,
        config,
        "b", 1,
        IB_PHASE_REQUEST_BODY,
        IB_PHASE_REQUEST_HEADER
    );
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_FALSE(source);
}

TEST(TestVar, SourceSetGet)
{
    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ib_var_source_t* b = make_source(config, "b");

    ASSERT_TRUE(a);
    ASSERT_TRUE(b);

    ib_var_store_t* store = make_store(config);

    ib_field_t fa;
    fa.name = "a";
    fa.nlen = 1;
    ib_field_t fb;
    fb.name = "b";
    fb.nlen = 1;

    rc = ib_var_source_set(a, store, &fa);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(b, store, &fb);
    ASSERT_EQ(IB_OK, rc);

    ib_field_t* f2;
    rc = ib_var_source_get(b, &f2, store);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(&fb, f2);
    rc = ib_var_source_get(a, &f2, store);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(&fa, f2);

    ib_var_source_t* source;
    rc = ib_var_source_lookup(&source, mp, config, "a", 1);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(a, source);
    rc = ib_var_source_lookup(&source, mp, config, "b", 1);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(b, source);
}

TEST(TestVar, SourceSetAndGetInvalid)
{
    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ib_var_source_t* b = make_source(config, "b");

    ASSERT_TRUE(a);
    ASSERT_TRUE(b);

    ib_var_store_t* store = make_store(config);

    ib_field_t fb;
    fb.name = "b";
    fb.nlen = 1;

    rc = ib_var_source_set(b, store, &fb);
    ASSERT_EQ(IB_OK, rc);

    ib_field_t* f2 = NULL;
    ib_var_source_t* unindexed;
    rc = ib_var_source_lookup(&unindexed, mp, config, "c", 1);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_get(unindexed, &f2, store);
    ASSERT_EQ(IB_ENOENT, rc);
    ASSERT_FALSE(f2);

    ib_var_config_t* other_config = make_config(mp);
    ib_var_source_t* b2 = make_source(other_config, "b");

    f2 = NULL;
    rc = ib_var_source_get(b2, &f2, store);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_FALSE(f2);

    rc = ib_var_source_set(b2, store, &fb);
    ASSERT_EQ(IB_EINVAL, rc);

    rc = ib_var_source_set(a, store, &fb);
    ASSERT_EQ(IB_EINVAL, rc);
}

TEST(TestVar, SourceUnindexed)
{
    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_store_t* store = make_store(config);

    ib_var_source_t* source = NULL;
    rc = ib_var_source_lookup(
        &source,
        mp,
        config,
        "a", 1
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(source);
    EXPECT_FALSE(ib_var_source_is_indexed(source));

    ib_field_t fa;
    fa.name = "a";
    fa.nlen = 1;

    rc = ib_var_source_set(source, store, &fa);
    ASSERT_EQ(IB_OK, rc);

    ib_field_t* f2;
    rc = ib_var_source_get(source, &f2, store);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(&fa, f2);
}

TEST(TestVar, LookupWithoutPool)
{
    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ASSERT_TRUE(a);

    ib_var_source_t* source;
    rc = ib_var_source_lookup(&source, NULL, config, "a", 1);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(a, source);

    source = NULL;
    rc = ib_var_source_lookup(&source, NULL, config, "b", 1);
    EXPECT_EQ(IB_ENOENT, rc);
    EXPECT_FALSE(source);
}

TEST(TestVar, SourceInitialize)
{
    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ASSERT_TRUE(a);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    ib_field_t* f = NULL;
    rc = ib_var_source_initialize(a, &f, store, IB_FTYPE_NUM);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(f);

    ib_field_t* f2 = NULL;
    rc = ib_var_source_get(a, &f2, store);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(f, f2);

    EXPECT_EQ(string("a"), string(f->name, f->nlen));
    EXPECT_EQ(IB_FTYPE_NUM, f->type);
    EXPECT_EQ(0, IronBee::Field(f).value_as_number());

    ib_var_config_t* other_config = make_config(mp);
    ib_var_source_t* bad_source = make_source(other_config, "a");
    f = NULL;
    rc = ib_var_source_initialize(bad_source, &f, store, IB_FTYPE_NUM);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_FALSE(f);

    f = NULL;
    rc = ib_var_source_initialize(a, &f, store, IB_FTYPE_GENERIC);
    ASSERT_EQ(IB_EINVAL, rc);
    ASSERT_FALSE(f);
}
