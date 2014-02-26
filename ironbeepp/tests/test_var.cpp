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
#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/var.hpp>

#include <ironbeepp/test_fixture.hpp>
#include "gtest/gtest.h"

using namespace IronBee;

class TestVar : public ::testing::Test, public TestFixture
{
public:
    TestVar() : m_mm(MemoryPoolLite(m_pool))
    {
        // nop
    }

protected:
    ScopedMemoryPoolLite m_pool;
    MemoryManager m_mm;
};

TEST_F(TestVar, Config)
{
    VarConfig vc = m_engine.var_config();
    ASSERT_TRUE(vc);
    ASSERT_TRUE(vc.memory_manager());

    vc = VarConfig::acquire(m_mm);
    ASSERT_TRUE(vc);
}

TEST_F(TestVar, Store)
{
    VarStore vs = m_transaction.var_store();
    ASSERT_TRUE(vs);
    ASSERT_EQ(m_engine.var_config(), vs.config());
    ASSERT_TRUE(vs.memory_manager());

    vs = VarStore::acquire(m_mm, m_engine.var_config());
    ASSERT_TRUE(vs);

    List<Field> to = List<Field>::create(m_mm);
    ASSERT_NO_THROW(vs.export_(to));
}

TEST_F(TestVar, Source)
{
    VarConfig vc = VarConfig::acquire(m_mm);

    VarSource s = VarSource::register_(vc, "foo");
    ASSERT_TRUE(s);
    ASSERT_EQ("foo", s.name_s());

    VarSource s2 = VarSource::acquire(m_mm, vc, "foo");
    ASSERT_EQ(s, s2);

    VarStore vs = VarStore::acquire(m_mm, vc);
    Field f = s.initialize(vs, Field::NUMBER);
    ASSERT_TRUE(f);
    ASSERT_EQ("0", f.to_s());

    ASSERT_EQ(f, s.get(vs));
}

TEST_F(TestVar, Filter)
{
    VarFilter vf = VarFilter::acquire(m_mm, "bar");
    List<Field> l = List<Field>::create(m_mm);
    l.push_back(Field::create_number(m_mm, "bar", 3, 5));
    Field bar = l.front();
    Field f = Field::create_no_copy_list<Field>(m_mm, "", 0, l);

    ConstList<ConstField> r;

    r = vf.apply(m_mm, f);
    ASSERT_TRUE(r);
    ASSERT_EQ(1UL, r.size());
    EXPECT_EQ(r.front(), bar);

    r = vf.remove(m_mm, f);
    ASSERT_TRUE(r);
    ASSERT_EQ(1UL, r.size());
    EXPECT_EQ(r.front(), bar);
}

TEST_F(TestVar, VarTarget)
{
    VarConfig vc = VarConfig::acquire(m_mm);

    VarSource s = VarSource::register_(vc, "foo");
    ASSERT_TRUE(s);
    ASSERT_EQ("foo", s.name_s());

    VarSource s2 = VarSource::acquire(m_mm, vc, "foo");
    ASSERT_EQ(s, s2);

    VarStore vs = VarStore::acquire(m_mm, vc);

    VarTarget vt = VarTarget::acquire_from_string(m_mm, vc, "foo:bar");
    ASSERT_TRUE(vt);

    vt.set(m_mm, vs, Field::create_number(m_mm, "", 0, 7));
    ConstList<Field> r = vt.get(m_mm, vs);
    ASSERT_EQ(1UL, r.size());
    ASSERT_EQ(7, r.front().value_as_number());
}

TEST_F(TestVar, VarExpand)
{
    VarConfig vc = VarConfig::acquire(m_mm);

    VarSource s = VarSource::register_(vc, "foo");
    ASSERT_TRUE(s);
    ASSERT_EQ("foo", s.name_s());

    VarSource s2 = VarSource::acquire(m_mm, vc, "foo");
    ASSERT_EQ(s, s2);

    VarStore vs = VarStore::acquire(m_mm, vc);
    s.initialize(vs, Field::NUMBER);

    VarExpand ve = VarExpand::acquire(m_mm, "x-%{foo}", vc);
    ASSERT_TRUE(ve);

    std::string result = ve.execute_s(m_mm, vs);
    ASSERT_EQ("x-0", result);
}
