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
 * @brief IronBee++ Internals --- Transaction Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/transaction.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/var.hpp>

#include <ironbeepp/test_fixture.hpp>
#include "gtest/gtest.h"

using namespace IronBee;

class TestVar : public ::testing::Test, public TestFixture
{
};

TEST_F(TestVar, Config)
{
    VarConfig vc = m_engine.var_config();
    ASSERT_TRUE(vc);
    ASSERT_TRUE(vc.memory_pool());

    ScopedMemoryPool smp;

    vc = VarConfig::acquire(smp);
    ASSERT_TRUE(vc);
}

TEST_F(TestVar, Store)
{
    VarStore vs = m_transaction.var_store();
    ASSERT_TRUE(vs);
    ASSERT_EQ(m_engine.var_config(), vs.config());
    ASSERT_TRUE(vs.memory_pool());

    ScopedMemoryPool smp;

    vs = VarStore::acquire(smp, m_engine.var_config());
    ASSERT_TRUE(vs);

    List<Field> to = List<Field>::create(smp);
    ASSERT_NO_THROW(vs.export_(to));
}

TEST_F(TestVar, Source)
{
    ScopedMemoryPool smp;
    VarConfig vc = VarConfig::acquire(smp);

    VarSource s = VarSource::register_(vc, "foo");
    ASSERT_TRUE(s);
    ASSERT_EQ("foo", s.name_s());

    VarSource s2 = VarSource::acquire(smp, vc, "foo");
    ASSERT_EQ(s, s2);

    VarStore vs = VarStore::acquire(smp, vc);
    Field f = s.initialize(vs, Field::NUMBER);
    ASSERT_TRUE(f);
    ASSERT_EQ("0", f.to_s());

    ASSERT_EQ(f, s.get(vs));
}

TEST_F(TestVar, Filter)
{
    ScopedMemoryPool smp;

    VarFilter vf = VarFilter::acquire(smp, "bar");
    List<Field> l = List<Field>::create(smp);
    l.push_back(Field::create_number(smp, "bar", 3, 5));
    Field bar = l.front();
    Field f = Field::create_no_copy_list<Field>(smp, "", 0, l);

    ConstList<ConstField> r;

    r = vf.apply(smp, f);
    ASSERT_TRUE(r);
    ASSERT_EQ(1UL, r.size());
    EXPECT_EQ(r.front(), bar);

    r = vf.remove(smp, f);
    ASSERT_TRUE(r);
    ASSERT_EQ(1UL, r.size());
    EXPECT_EQ(r.front(), bar);
}

TEST_F(TestVar, VarTarget)
{
    ScopedMemoryPool smp;

    VarConfig vc = VarConfig::acquire(smp);

    VarSource s = VarSource::register_(vc, "foo");
    ASSERT_TRUE(s);
    ASSERT_EQ("foo", s.name_s());

    VarSource s2 = VarSource::acquire(smp, vc, "foo");
    ASSERT_EQ(s, s2);

    VarStore vs = VarStore::acquire(smp, vc);

    VarTarget vt = VarTarget::acquire_from_string(smp, vc, "foo:bar");
    ASSERT_TRUE(vt);

    vt.set(smp, vs, Field::create_number(smp, "", 0, 7));
    ConstList<Field> r = vt.get(smp, vs);
    ASSERT_EQ(1UL, r.size());
    ASSERT_EQ(7, r.front().value_as_number());
}

TEST_F(TestVar, VarExpand)
{
    ScopedMemoryPool smp;
    VarConfig vc = VarConfig::acquire(smp);

    VarSource s = VarSource::register_(vc, "foo");
    ASSERT_TRUE(s);
    ASSERT_EQ("foo", s.name_s());

    VarSource s2 = VarSource::acquire(smp, vc, "foo");
    ASSERT_EQ(s, s2);

    VarStore vs = VarStore::acquire(smp, vc);
    s.initialize(vs, Field::NUMBER);

    VarExpand ve = VarExpand::acquire(smp, "x-%{foo}", vc);
    ASSERT_TRUE(ve);

    std::string result = ve.execute_s(smp, vs);
    ASSERT_EQ("x-0", result);
}
