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

#include <ironbee/string.h>
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

    rc = ib_var_config_acquire(&config, mp);
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

    rc = ib_var_store_acquire(&store, mp, config);
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
        IB_PHASE_REQUEST
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
    EXPECT_EQ(IB_PHASE_REQUEST, ib_var_source_final_phase(source));
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
        IB_PHASE_REQUEST
    );
    ASSERT_EQ(IB_EEXIST, rc);
    ASSERT_FALSE(source);

    source = NULL;
    rc = ib_var_source_register(
        &source,
        config,
        "b", 1,
        IB_PHASE_REQUEST,
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
    rc = ib_var_source_acquire(&source, mp, config, "a", 1);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(a, source);
    rc = ib_var_source_acquire(&source, mp, config, "b", 1);
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
    rc = ib_var_source_acquire(&unindexed, mp, config, "c", 1);
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
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ("a", string(fb.name, fb.nlen));
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
    rc = ib_var_source_acquire(
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

TEST(TestVar, SourceLookupWithoutPool)
{
    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ASSERT_TRUE(a);

    ib_var_source_t* source;
    rc = ib_var_source_acquire(&source, NULL, config, "a", 1);
    EXPECT_EQ(IB_OK, rc);
    EXPECT_EQ(a, source);

    source = NULL;
    rc = ib_var_source_acquire(&source, NULL, config, "b", 1);
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

TEST(TestVar, SourceAppend)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ASSERT_TRUE(a);
    ib_var_source_t* b = make_source(config, "b");
    ASSERT_TRUE(a);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    rc = ib_var_source_append(a, store,
        Field::create_number(smp, "A", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_append(a, store,
        Field::create_number(smp, "B", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    ib_field_t* list_field;
    rc = ib_var_source_get(a, &list_field, store);
    ASSERT_EQ(IB_OK, rc);
    Field f(list_field);
    ASSERT_EQ(2UL, f.value_as_list<Field>().size());

    rc = ib_var_source_set(b, store,
        Field::create_number(smp, "b", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_source_append(b, store,
        Field::create_number(smp, "A", 1, 1).ib()
    );
    ASSERT_EQ(IB_EINCOMPAT, rc);
}

TEST(TestVar, Filter)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    typedef ConstList<IronBee::Field> field_clist_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));
    data_list.push_back(Field::create_number(smp, "barA", 4, 7));
    data_list.push_back(Field::create_number(smp, "x", 1, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_filter_t *filter;
    rc = ib_var_filter_acquire(&filter, mp, "fooa", 4, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);

    const ib_list_t *result = NULL;
    rc = ib_var_filter_apply(filter, &result, mp, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    field_clist_t result_list(result);
    EXPECT_EQ(1UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());

    rc = ib_var_filter_acquire(&filter, mp, "/foo/", 5, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_filter_apply(filter, &result, mp, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(2UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());
    EXPECT_EQ("fooB", result_list.back().name_as_s());

    rc = ib_var_filter_acquire(&filter, mp, "", 0, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_filter_apply(filter, &result, mp, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(0UL, result_list.size());

    rc = ib_var_filter_acquire(&filter, mp, "x", 1, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_filter_apply(filter, &result, mp, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(1UL, result_list.size());
}

TEST(TestVar, FilterRemove)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    typedef ConstList<IronBee::Field> field_clist_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));
    data_list.push_back(Field::create_number(smp, "barA", 4, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_filter_t *filter;
    rc = ib_var_filter_acquire(&filter, mp, "fooa", 4, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);

    ib_list_t *result = NULL;
    rc = ib_var_filter_remove(filter, &result, mp, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    field_clist_t result_list(result);
    EXPECT_EQ(1UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());
    EXPECT_EQ(2UL, data_field.value_as_list<Field>().size());

    rc = ib_var_filter_acquire(&filter, mp, "/foo/", 5, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_filter_remove(filter, &result, mp, data_field.ib());
    ASSERT_EQ(IB_EINVAL, rc);
}

TEST(TestVar, Target)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    typedef ConstList<IronBee::Field> field_clist_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));
    data_list.push_back(Field::create_number(smp, "barA", 4, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_config_t *config = make_config(mp);
    ASSERT_TRUE(config);
    ib_var_source_t *source = make_source(config, "data");
    ASSERT_TRUE(source);
    ib_var_source_t *source_fooA = make_source(config, "fooA");
    ASSERT_TRUE(source_fooA);
    ib_var_store_t *store = make_store(config);
    rc = ib_var_source_set(source, store, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(source_fooA, store, data_list.front().ib());
    ASSERT_EQ(IB_OK, rc);

    ib_var_filter_t *filter;
    rc = ib_var_filter_acquire(&filter, mp, "fooa", 4, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);

    ib_var_target_t *target;
    const ib_list_t *result = NULL;
    field_clist_t result_list;

    rc = ib_var_target_acquire(&target, mp, source, NULL, filter);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(1UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());

    rc = ib_var_target_acquire_from_string(&target, mp, config, "data:fooa", 9, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(1UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());

    rc = ib_var_target_acquire_from_string(&target, mp, config, "data:/foo/", 10, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(2UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());
    EXPECT_EQ("fooB", result_list.back().name_as_s());

    rc = ib_var_target_acquire_from_string(&target, mp, config, "data", 4, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(3UL, result_list.size());

    rc = ib_var_target_acquire_from_string(&target, mp, config, "fooA", 4, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(1UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());
}

TEST(TestVar, TargetRemoveTrivial)
{
    using namespace IronBee;

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

    rc = ib_var_source_set(a, store,
        Field::create_number(smp, "a", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(b, store,
        Field::create_number(smp, "b", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    ib_list_t* result;
    ib_var_target_t* target;

    rc = ib_var_target_acquire(&target, mp, a, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_remove(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(1UL, ib_list_elements(result));
    ASSERT_EQ(IB_ENOENT, ib_var_source_get(a, NULL, store));
    ASSERT_EQ(IB_OK, ib_var_source_get(b, NULL, store));
}

TEST(TestVar, TargetRemoveSimple)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));
    data_list.push_back(Field::create_number(smp, "barA", 4, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* data = make_source(config, "data");
    ASSERT_TRUE(data);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    rc = ib_var_source_set(data, store, data_field.ib());
    ASSERT_EQ(IB_OK, rc);

    ib_list_t* result;
    ib_var_target_t* target;

    rc = ib_var_target_acquire_from_string(
        &target,
        mp,
        config,
        "data:fooA", sizeof("data:fooA") - 1,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_remove(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(1UL, ib_list_elements(result));

    ib_field_t *field;
    ASSERT_EQ(IB_OK, ib_var_source_get(data, &field, store));
    ASSERT_EQ(2UL, Field(field).value_as_list<Field>().size());
}

TEST(TestVar, TargetRemoveExpand)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));
    data_list.push_back(Field::create_number(smp, "barA", 4, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* data = make_source(config, "data");
    ASSERT_TRUE(data);
    ib_var_source_t* index = make_source(config, "index");
    ASSERT_TRUE(data);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    rc = ib_var_source_set(data, store, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(index, store,
        Field::create_byte_string(
            smp, "index", 5,
            ByteString::create(smp, "fooA")
        ).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    ib_list_t* result;
    ib_var_target_t* target;

    rc = ib_var_target_acquire_from_string(
        &target,
        mp,
        config,
        "data:%{index}", sizeof("data:%{index}") - 1,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_remove(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(1UL, ib_list_elements(result));

    ib_field_t *field;
    ASSERT_EQ(IB_OK, ib_var_source_get(data, &field, store));
    ASSERT_EQ(2UL, Field(field).value_as_list<Field>().size());
}

TEST(TestVar, TargetSetTrivial)
{
    using namespace IronBee;
    typedef ConstList<IronBee::Field> const_field_list_t;

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

    rc = ib_var_source_set(a, store,
        Field::create_number(smp, "a", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(b, store,
        Field::create_number(smp, "b", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    ib_var_target_t *t;

    rc = ib_var_target_acquire_from_string(
        &t, mp, config, "a", 1, NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_target_set(t, mp, store,
        Field::create_number(smp, "", 0, 2).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    const ib_list_t *l;
    rc = ib_var_target_get(t, &l, mp, store);
    ASSERT_EQ(IB_OK, rc);

    const_field_list_t results(l);
    ASSERT_EQ(1UL, results.size());
    ASSERT_EQ(2, results.front().value_as_number());
}

TEST(TestVar, TargetRemoveAndSet)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ASSERT_TRUE(a);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    rc = ib_var_source_append(a, store,
        Field::create_number(smp, "A", 1, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    ib_var_target_t* target;
    rc = ib_var_target_acquire_from_string(
        &target,
        mp,
        config,
        "a:A", sizeof("a:A") - 1,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_remove_and_set(target, mp, store,
        Field::create_number(smp, "a:A", 3, 2).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    const ib_list_t* result;
    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(1UL, ib_list_elements(result));
}

TEST(TestVar, TargetSetSimple)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));
    data_list.push_back(Field::create_number(smp, "barA", 4, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* data = make_source(config, "data");
    ASSERT_TRUE(data);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    rc = ib_var_source_set(data, store, data_field.ib());
    ASSERT_EQ(IB_OK, rc);

    const ib_list_t* result;
    ib_var_target_t* target;

    rc = ib_var_target_acquire_from_string(
        &target,
        mp,
        config,
        "data:another", sizeof("data:another") - 1,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_set(target, mp, store,
        Field::create_number(smp, "", 0, 8).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(1UL, ib_list_elements(result));

    rc = ib_var_target_acquire_from_string(
        &target,
        mp,
        config,
        "a:b", sizeof("a:b") - 1,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_set(target, mp, store,
        Field::create_number(smp, "", 0, 9).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(1UL, ib_list_elements(result));
}

TEST(TestVar, TargetSetExpand)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "barA", 4, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* data = make_source(config, "data");
    ASSERT_TRUE(data);
    ib_var_source_t* index = make_source(config, "index");
    ASSERT_TRUE(data);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    rc = ib_var_source_set(data, store, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(index, store,
        Field::create_byte_string(
            smp, "index", 5,
            ByteString::create(smp, "fooA")
        ).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    const ib_list_t* result;
    ib_var_target_t* target;

    rc = ib_var_target_acquire_from_string(
        &target,
        mp,
        config,
        "data:%{index}", sizeof("data:%{index}") - 1,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_set(target, mp, store,
        Field::create_number(smp, "", 0, 1).ib()
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1UL, ib_list_elements(result));

    ib_field_t *field;
    rc = ib_var_source_get(data, &field, store);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(2UL, Field(field).value_as_list<Field>().size());
}

TEST(TestVar, ExpandFilter)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();
    typedef List<IronBee::Field> field_list_t;
    typedef ConstList<IronBee::Field> field_clist_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));
    data_list.push_back(Field::create_number(smp, "barA", 4, 7));

    Field data_field =
        Field::create_no_copy_list<Field>(smp, "data", 4, data_list);

    ib_var_config_t *config = make_config(mp);
    ASSERT_TRUE(config);
    ib_var_source_t *source = make_source(config, "data");
    ASSERT_TRUE(source);

    Field index = Field::create_byte_string(smp, "index", 5,
        ByteString::create(smp, "fooA")
    );

    ib_var_source_t *source_index = make_source(config, "index");
    ASSERT_TRUE(source_index);
    ib_var_store_t *store = make_store(config);
    rc = ib_var_source_set(source, store, data_field.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(source_index, store, index.ib());
    ASSERT_EQ(IB_OK, rc);

    ib_var_target_t *target;
    const ib_list_t *result = NULL;
    field_clist_t result_list;

    rc = ib_var_target_acquire_from_string(&target, mp, config, "data:%{index}", 13, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    result_list = field_clist_t(result);
    EXPECT_EQ(1UL, result_list.size());
    EXPECT_EQ("fooA", result_list.front().name_as_s());
}

TEST(TestVar, Expand)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    typedef List<IronBee::Field> field_list_t;
    field_list_t data_list = field_list_t::create(smp);

    data_list.push_back(Field::create_number(smp, "fooA", 4, 5));
    data_list.push_back(Field::create_number(smp, "fooB", 4, 6));

    ib_var_source_t* a = make_source(config, "a");
    ib_var_source_t* b = make_source(config, "b");
    ib_var_source_t* c = make_source(config, "c");
    ib_var_source_t* d = make_source(config, "d");

    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    ASSERT_TRUE(c);
    ASSERT_TRUE(d);

    ib_var_store_t* store = make_store(config);

    Field fa = Field::create_number(smp, "a", 1, 17);
    Field fb = Field::create_float(smp, "b", 1, 1.234);
    Field fc = Field::create_byte_string(smp, "c", 1,
        ByteString::create(smp, "foo")
    );
    Field fd = Field::Field::create_no_copy_list<Field>(
        smp,
        "d", 1,
        data_list
    );

    rc = ib_var_source_set(a, store, fa.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(b, store, fb.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(c, store, fc.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_var_source_set(d, store, fd.ib());
    ASSERT_EQ(IB_OK, rc);

    static const string c_expand_string("a = %{a} b = %{b} c = %{c} d1 = %{d} d2 = %{d:fooA}");

    ASSERT_TRUE(
        ib_var_expand_test(c_expand_string.data(), c_expand_string.length())
    );
    ASSERT_FALSE(ib_var_expand_test("foo", 3));

    ib_var_expand_t *expand = NULL;
    rc = ib_var_expand_acquire(
        &expand,
        mp,
        c_expand_string.data(), c_expand_string.length(),
        config,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(expand);

    const char *result = NULL;
    size_t result_length;
    rc = ib_var_expand_execute(
        expand,
        &result, &result_length,
        mp,
        store
    );
    ASSERT_EQ(IB_OK, rc);

    EXPECT_EQ(
        "a = 17 b = 1.234000 c = foo d1 = 5, 6 d2 = 5",
        string(result, result_length)
    );

    expand = NULL;
    rc = ib_var_expand_acquire(&expand, mp, "", 0, config, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(expand);

    result = NULL;
    rc = ib_var_expand_execute(
        expand,
        &result, &result_length,
        mp,
        store
    );
    ASSERT_EQ(IB_OK, rc);

    EXPECT_EQ("", string(result, result_length));
}



extern "C" {
static ib_status_t dyn_get(
    const ib_field_t *f,
    void *out_value,
    const void *arg,
    size_t alen,
    void *data
)
{
    ib_mpool_t *mp = (ib_mpool_t *)data;
    ib_num_t numval = 5;
    ib_field_t *newf;
    ib_status_t rc;
    ib_list_t *l;

    const char* carg = (const char *)arg;

    rc = ib_list_create(&l, mp);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_field_create(&newf, mp, carg, alen, IB_FTYPE_NUM,
        ib_ftype_num_in(&numval));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_push(l, newf);
    if (rc != IB_OK) {
        return rc;
    }

    *(void**)out_value = l;

    return IB_OK;
}
}

TEST(TestVar, TargetDynamic)
{
    using namespace IronBee;

    ScopedMemoryPool smp;
    ib_status_t rc;
    ib_mpool_t* mp = MemoryPool(smp).ib();

    ib_var_config_t* config = make_config(mp);
    ASSERT_TRUE(config);

    ib_var_source_t* a = make_source(config, "a");
    ASSERT_TRUE(a);

    ib_var_store_t* store = make_store(config);
    ASSERT_TRUE(store);

    ib_field_t *dynf;
    rc = ib_field_create_dynamic(
        &dynf,
        mp,
        "", 0,
        IB_FTYPE_LIST,
        dyn_get, mp,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(dynf);

    rc = ib_var_source_set(a, store, dynf);
    ASSERT_EQ(IB_OK, rc);

    ib_var_target_t *target;

    rc = ib_var_target_acquire_from_string(
        &target,
        mp,
        config,
        IB_S2SL("a:sub"),
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(target);

    const ib_list_t *result;
    rc = ib_var_target_get(target, &result, mp, store);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1UL, ib_list_elements(result));

    ConstField f = ConstList<ConstField>(result).front();
    ASSERT_EQ(5, f.value_as_number());
}
