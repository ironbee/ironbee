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
/// @brief IronBee &mdash JSON Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/json.h>

#include <ironbee/list.h>
#include <ironbee/field.h>
#include <ironbee/string.h>

#include "gtest/gtest.h"

#include "simple_fixture.hpp"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdexcept>

/**
 * Parameter structure for TestIBUtilJsonDecode tests.
 */
struct TestIBUtilJsonDecode_t {
    ib_field_t *field;
    char *name;
};

/* -- JSON decode tests */
class TestIBUtilJsonDecode : public SimpleFixture
{
public:
    virtual void SetUp(void)
    {
        SimpleFixture::SetUp();
    }
    virtual void TearDown()
    {
        SimpleFixture::TearDown();
    }

    void CheckFieldName(const ib_field_t *field,
                        const char *name)
    {
        if (name != NULL) {
            size_t len = strlen(name);
            ASSERT_EQ(len, field->nlen);
            ASSERT_EQ(0, memcmp(name, field->name, len));
        }
    }

    void CheckNode(const ib_list_node_t *node,
                   const char *name,
                   int expected)
    {
        const ib_field_t *field;
        ib_num_t          num;

        ASSERT_TRUE(node);
        ASSERT_TRUE(node->data);

        field = (const ib_field_t *)node->data;
        ASSERT_EQ(IB_FTYPE_NUM, field->type);
        ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_num_out(&num)) );
        ASSERT_EQ(expected, num);
        CheckFieldName(field, name);
    }

    void CheckNode(const ib_list_node_t *node,
                   const char *name,
                   double expected)
    {
        const ib_field_t *field;
        ib_float_t        fnum;

        ASSERT_TRUE(node);
        ASSERT_TRUE(node->data);

        field = (const ib_field_t *)node->data;
        ASSERT_EQ(IB_FTYPE_FLOAT, field->type);
        ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_float_out(&fnum)) );
        ASSERT_DOUBLE_EQ(expected, fnum);
        CheckFieldName(field, name);
    }

    void CheckNode(const ib_list_node_t *node,
                   const char *name,
                   const char *expected)
    {
        const ib_field_t     *field;
        const ib_bytestr_t   *bs;
        const uint8_t        *bsval;
        size_t                len = strlen(expected);

        ASSERT_TRUE(node);
        ASSERT_TRUE(node->data);

        field = (const ib_field_t *)node->data;
        ASSERT_EQ(IB_FTYPE_BYTESTR, field->type);
        ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_bytestr_out(&bs)) );
        ASSERT_EQ(len, ib_bytestr_size(bs));
        bsval = ib_bytestr_const_ptr(bs);
        ASSERT_TRUE(bsval);
        ASSERT_EQ(0, memcmp(bsval, expected, len));

        CheckFieldName(field, name);
    }

    void CheckNode(const ib_list_node_t *node,
                   const char *name,
                   size_t elements,
                   const ib_list_t **list)
    {
        const ib_field_t     *field;

        ASSERT_TRUE(node);
        ASSERT_TRUE(node->data);

        field = (ib_field_t *)node->data;
        ASSERT_EQ(IB_FTYPE_LIST, field->type);
        ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_list_out(list)) );
        ASSERT_EQ(elements, ib_list_elements(*list));

        CheckFieldName(field, name);
    }
};

/// @test Test util JSON functions - Basic decode
TEST_F(TestIBUtilJsonDecode, json_decode_basic)
{
    const char *buf1 = "{ \"x\": 5 }";
    const char *buf2 =
        "{\r\n"
        "\"one\": 1,\r\n"
        "\"two\": 2,\r\n"
        "\"f1\":  1.2,\r\n"
        "\"f2\":  11.1,\r\n"
        "\"s1\":  \"abc\",\r\n"
        "\"s2\":  \"def\",\r\n"
        "\"reallyreallreallyreallyreallylongname\": \"xyzzy\"\r\n"
        "}";

    ib_list_t            *list;
    ib_status_t           rc;
    const char           *error;
    const ib_list_node_t *node;

    rc = ib_list_create(&list, MemPool());
    ASSERT_EQ(IB_OK, rc);

    rc = ib_json_decode(MemPool(), "", list, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(0U, ib_list_elements(list));

    ib_list_clear(list);
    rc = ib_json_decode(MemPool(), buf1, list, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1U, ib_list_elements(list));

    ib_list_clear(list);
    rc = ib_json_decode(MemPool(), buf2, list, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(7U, ib_list_elements(list));

    node = ib_list_first_const(list);
    { SCOPED_TRACE("node 1"); CheckNode(node, NULL, 1); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 2"); CheckNode(node, NULL, 2); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 3"); CheckNode(node, NULL, 1.2); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 4"); CheckNode(node, NULL, 11.1); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 5"); CheckNode(node, NULL, "abc"); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 6"); CheckNode(node, NULL, "def"); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 7"); CheckNode(node, NULL, "xyzzy"); }
}

/// @test Test util JSON functions - Complex decode
TEST_F(TestIBUtilJsonDecode, json_decode_complex)
{
    const char *buf =
        "{\r\n"
        "\"num\": 1,\r\n"
        "\"float\":  1.2,\r\n"
        "\"str\":  \"abc\",\r\n"
        "\"list\": [ 1, 2, 3 ]\r\n"
        "}";

    ib_status_t           rc;
    const char           *error;
    ib_list_t            *list;
    const ib_list_node_t *node;

    rc = ib_list_create(&list, MemPool());
    ASSERT_EQ(IB_OK, rc);

    rc = ib_json_decode(MemPool(), buf, list, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(4U, ib_list_elements(list));

    node = ib_list_first_const(list);
    { SCOPED_TRACE("node 1"); CheckNode(node, NULL, 1); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 2"); CheckNode(node, NULL, 1.2); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 3"); CheckNode(node, NULL, "abc"); }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("list"); CheckNode(node, "list", 3, &list2); }

        node2 = ib_list_first_const(list2);
        { SCOPED_TRACE("list:1"); CheckNode(node2, NULL, 1); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("list:2"); CheckNode(node2, NULL, 2); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("list:3"); CheckNode(node2, NULL, 3); }
    }
}

/// @test Test util JSON functions - Nested decode
TEST_F(TestIBUtilJsonDecode, json_decode_nested)
{
    const char *buf =
        "{\r\n"
        "\"num\": 1,\r\n"
        "\"float\":  1.2,\r\n"
        "\"str\":  \"abc\",\r\n"
        "\"list\": [ 1, 2.0, \"three\" ],\r\n"
        "\"dict1\": { \"v1\":1, \"v2\":2.0, \"v3\":\"three\", \"v4\":4 },\r\n"
        "\"dict2\": { \"l\":[2,3,4], \"d\":{\"v1\":1.0, \"v2\":\"two\"} }\r\n"
        "}";

    ib_list_t            *list;
    ib_status_t           rc;
    const ib_list_node_t *node;
    const char           *error = NULL;

    rc = ib_list_create(&list, MemPool());
    ASSERT_EQ(IB_OK, rc);

    rc = ib_json_decode(MemPool(), buf, list, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(6U, ib_list_elements(list));

    node = ib_list_first_const(list);
    { SCOPED_TRACE("node 1"); CheckNode(node, NULL, 1); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 2"); CheckNode(node, NULL, 1.2); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 3"); CheckNode(node, NULL, "abc"); }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("list"); CheckNode(node, "list", 3, &list2); }

        node2 = ib_list_first_const(list2);
        { SCOPED_TRACE("list:1"); CheckNode(node2, NULL, 1); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("list:2"); CheckNode(node2, NULL, 2.0); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("list:3"); CheckNode(node2, NULL, "three"); }
    }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("dict1"); CheckNode(node, "dict1", 4, &list2); }

        node2 = ib_list_first_const(list2);
        { SCOPED_TRACE("dict1:v1"); CheckNode(node2, "v1", 1); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("dict1:v2"); CheckNode(node2, "v2", 2.0); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("dict1:v3"); CheckNode(node2, "v3", "three"); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("dict1:v4"); CheckNode(node2, "v4", 4); }
    }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("dict2"); CheckNode(node, "dict2", 2, &list2); }

        node2 = ib_list_first_const(list2);
        {
            const ib_list_t      *list3;
            const ib_list_node_t *node3;

            SCOPED_TRACE("dict2:l"); CheckNode(node2, "l", 3, &list3);

            node3 = ib_list_first_const(list3);
            { SCOPED_TRACE("dict2:l[0]"); CheckNode(node3, NULL, 2); }

            node3 = ib_list_node_next_const(node3);
            { SCOPED_TRACE("dict2:l[1]"); CheckNode(node3, NULL, 3); }

            node3 = ib_list_node_next_const(node3);
            { SCOPED_TRACE("dict2:l[2]"); CheckNode(node3, NULL, 4); }
        }

        node2 = ib_list_node_next_const(node2);
        {
            const ib_list_t      *list3;
            const ib_list_node_t *node3;

            SCOPED_TRACE("dict2:d"); CheckNode(node2, "d", 2, &list3);

            node3 = ib_list_first_const(list3);
            { SCOPED_TRACE("dict2:d:v1"); CheckNode(node3, "v1", 1.0); }

            node3 = ib_list_node_next_const(node3);
            { SCOPED_TRACE("dict2:d:v2"); CheckNode(node3, "v2", "two"); }
        }
    }
}

/* -- JSON encode tests */
class TestIBUtilJsonEncode : public TestIBUtilJsonDecode,
                             public ::testing::WithParamInterface<bool>
{
public:
    void AddNode(ib_list_t *list,
                 ib_field_t *field)
    {
        ib_status_t rc = ib_list_push(list, field);
        ASSERT_EQ(IB_OK, rc);
    }

    void AddNode(ib_list_t *list,
                 const char *name,
                 int value)
    {
        ib_field_t       *field;
        ib_num_t          num = value;
        ib_status_t       rc;

        rc = ib_field_create(&field, MemPool(), IB_S2SL(name),
                             IB_FTYPE_NUM, ib_ftype_num_in(&num));
        ASSERT_EQ(IB_OK, rc);
        AddNode(list, field);
    }

    void AddNode(ib_list_t *list,
                 const char *name,
                 double value)
    {
        ib_field_t       *field;
        ib_float_t        fnum = value;
        ib_status_t       rc;

        rc = ib_field_create(&field, MemPool(), IB_S2SL(name),
                             IB_FTYPE_FLOAT, ib_ftype_float_in(&fnum));
        ASSERT_EQ(IB_OK, rc);
        AddNode(list, field);
    }

    void AddNode(ib_list_t *list,
                 const char *name,
                 const char *value)
    {
        ib_field_t       *field;
        ib_status_t       rc;

        rc = ib_field_create(&field, MemPool(), IB_S2SL(name),
                             IB_FTYPE_NULSTR, ib_ftype_nulstr_in(value));
        ASSERT_EQ(IB_OK, rc);
        AddNode(list, field);
    }

    void AddNode(ib_list_t *list,
                 const char *name,
                 ib_list_t *ilist)
    {
        ib_field_t       *field;
        ib_status_t       rc;

        rc = ib_field_create(&field, MemPool(), IB_S2SL(name),
                             IB_FTYPE_LIST, ib_ftype_list_in(ilist));
        ASSERT_EQ(IB_OK, rc);
        AddNode(list, field);
    }
};

TEST_P(TestIBUtilJsonEncode, json_encode_basic)
{
    ib_status_t           rc;
    ib_list_t            *list;
    ib_list_t            *olist;
    char                 *buf = NULL;
    size_t                buflen = 0;
    const char           *error;
    const ib_list_node_t *node;
    bool                  pretty = GetParam();

    rc = ib_list_create(&list, MemPool());
    ASSERT_EQ(IB_OK, rc);

    AddNode(list, "Zero", 0.0);
    AddNode(list, "One", 1);
    AddNode(list, "Two", 2);
    AddNode(list, "Three", 3.0);
    AddNode(list, "Four", 4.0);
    AddNode(list, "Five", "five");
    AddNode(list, "Six", "six");

    rc = ib_json_encode(MemPool(), list, pretty, &buf, &buflen);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(buf);
    ASSERT_NE(0U, buflen);
    //puts(buf);

    rc = ib_list_create(&olist, MemPool());
    ASSERT_EQ(IB_OK, rc);

    rc = ib_json_decode(MemPool(), buf, olist, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(7U, ib_list_elements(olist));

    node = ib_list_first_const(olist);
    { SCOPED_TRACE("node 0"); CheckNode(node, "Zero", 0.0); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 1"); CheckNode(node, "One", 1); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 2"); CheckNode(node, "Two", 2); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 3"); CheckNode(node, "Three", 3.0); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 4"); CheckNode(node, "Four", 4.0); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 5"); CheckNode(node, "Five", "five"); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 6"); CheckNode(node, "Six", "six"); }
}

TEST_P(TestIBUtilJsonEncode, json_encode_complex)
{
    ib_status_t           rc;
    ib_list_t            *list;
    const char           *error;
    const ib_list_node_t *node;
    ib_list_t            *olist;
    char                 *buf = NULL;
    size_t                buflen = 0;
    bool                  pretty = GetParam();

    /* Build the IronBee list */
    ASSERT_EQ(IB_OK, ib_list_create(&list, MemPool()));
    { SCOPED_TRACE("num"); AddNode(list, "num", 1); }
    { SCOPED_TRACE("float"); AddNode(list, "float", 2.0); }
    { SCOPED_TRACE("str"); AddNode(list, "str", "abc"); }
    {
        ib_list_t *list2;
        ASSERT_EQ(IB_OK, ib_list_create(&list2, MemPool()));
        { SCOPED_TRACE("one"); AddNode(list2, "one", 1); }
        { SCOPED_TRACE("two"); AddNode(list2, "two", 2); }
        { SCOPED_TRACE("three"); AddNode(list2, "three", 3); }

        { SCOPED_TRACE("list"); AddNode(list, "list", list2); }
    }

    /* Encode the IronBee list into JSON */
    rc = ib_json_encode(MemPool(), list, pretty, &buf, &buflen);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(buf);
    ASSERT_NE(0U, buflen);
    //puts(buf);

    /* Decode the JSON into an IronBee list & validate the list */
    ASSERT_EQ(IB_OK, ib_list_create(&olist, MemPool()));
    rc = ib_json_decode(MemPool(), buf, olist, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(4U, ib_list_elements(olist));

    node = ib_list_first_const(olist);
    { SCOPED_TRACE("num"); CheckNode(node, "num", 1); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("float"); CheckNode(node, "float", 2.0); }

    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("str"); CheckNode(node, "str", "abc"); }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("list"); CheckNode(node, "list", 3, &list2); }

        node2 = ib_list_first_const(list2);
        { SCOPED_TRACE("one"); CheckNode(node2, "one", 1); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("two"); CheckNode(node2, "two", 2); }

        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("three"); CheckNode(node2, "three", 3); }
    }
}

TEST_P(TestIBUtilJsonEncode, json_encode_nested)
{
    /* This is the effective JSON that should be built
       "{
       "num": 1,
       "float":  1.2,
       "str":  "abc",
       "list": [ 1, 2.0, "three" ],
       "dict1": { "v1":1, "v2":2.0, "v3":"three", "v4":4 },
       "dict2": { "l":[2,3,4], "d":{"v1":1.0, "v2":"two"} }
       }
    */

    ib_list_t            *list;
    ib_status_t           rc;
    const ib_list_node_t *node;
    const char           *error = NULL;
    ib_list_t            *olist;
    char                 *buf = NULL;
    size_t                buflen = 0;
    bool                  pretty = GetParam();


    /* Build the IronBee list */
    ASSERT_EQ(IB_OK, ib_list_create(&list, MemPool()));
    { SCOPED_TRACE("num"); AddNode(list, "num", 1); }
    { SCOPED_TRACE("float"); AddNode(list, "float", 1.2); }
    { SCOPED_TRACE("str"); AddNode(list, "str", "abc"); }
    {
        ib_list_t *list2;
        ASSERT_EQ(IB_OK, ib_list_create(&list2, MemPool()));
        { SCOPED_TRACE(":1"); AddNode(list2, ":1", 1); }
        { SCOPED_TRACE(":2"); AddNode(list2, ":2", 2.0); }
        { SCOPED_TRACE(":3"); AddNode(list2, ":3", "three"); }

        { SCOPED_TRACE("list"); AddNode(list, "list", list2); }
    }
    {
        ib_list_t *list2;
        ASSERT_EQ(IB_OK, ib_list_create(&list2, MemPool()));
        { SCOPED_TRACE("v1"); AddNode(list2, "v1", 1); }
        { SCOPED_TRACE("v2"); AddNode(list2, "v2", 2.0); }
        { SCOPED_TRACE("v3"); AddNode(list2, "v3", "three"); }
        { SCOPED_TRACE("v4"); AddNode(list2, "v4", 4); }

        { SCOPED_TRACE("dict1"); AddNode(list, "dict1", list2); }
    }
    {
        ib_list_t *list2;
        ASSERT_EQ(IB_OK, ib_list_create(&list2, MemPool()));
        {
            ib_list_t *list3;
            ASSERT_EQ(IB_OK, ib_list_create(&list3, MemPool()));
            { SCOPED_TRACE("0"); AddNode(list3, ":0", 2); }
            { SCOPED_TRACE("1"); AddNode(list3, ":1", 3); }
            { SCOPED_TRACE("2"); AddNode(list3, ":2", 4); }
            { SCOPED_TRACE("l"); AddNode(list2, "l", list3); }
        }
        {
            ib_list_t *list3;
            ASSERT_EQ(IB_OK, ib_list_create(&list3, MemPool()));
            { SCOPED_TRACE("v1"); AddNode(list3, "v1", 1.0); }
            { SCOPED_TRACE("v2"); AddNode(list3, "v2", "two"); }
            { SCOPED_TRACE("d"); AddNode(list2, "d", list3); }
        }

        { SCOPED_TRACE("dict2"); AddNode(list, "dict2", list2); }
    }

    /* Encode the IronBee list into JSON */
    rc = ib_json_encode(MemPool(), list, pretty, &buf, &buflen);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(buf);
    ASSERT_NE(0U, buflen);
    //puts(buf);

    /* Decode the JSON into an IronBee list & validate the list */
    rc = ib_list_create(&olist, MemPool());
    ASSERT_EQ(IB_OK, rc);

    rc = ib_json_decode(MemPool(), buf, olist, &error);
    if (error != NULL) {
        printf("Error @ \"%s\"", error);
    }
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(6U, ib_list_elements(olist));

    node = ib_list_first_const(olist);
    { SCOPED_TRACE("node 1"); CheckNode(node, NULL, 1); }
    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 2"); CheckNode(node, NULL, 1.2); }
    node = ib_list_node_next_const(node);
    { SCOPED_TRACE("node 3"); CheckNode(node, NULL, "abc"); }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("list"); CheckNode(node, "list", 3, &list2); }
        node2 = ib_list_first_const(list2);
        { SCOPED_TRACE("list:1"); CheckNode(node2, NULL, 1); }
        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("list:2"); CheckNode(node2, NULL, 2.0); }
        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("list:3"); CheckNode(node2, NULL, "three"); }
    }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("dict1"); CheckNode(node, "dict1", 4, &list2); }
        node2 = ib_list_first_const(list2);
        { SCOPED_TRACE("dict1:v1"); CheckNode(node2, "v1", 1); }
        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("dict1:v2"); CheckNode(node2, "v2", 2.0); }
        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("dict1:v3"); CheckNode(node2, "v3", "three"); }
        node2 = ib_list_node_next_const(node2);
        { SCOPED_TRACE("dict1:v4"); CheckNode(node2, "v4", 4); }
    }

    {
        const ib_list_t      *list2;
        const ib_list_node_t *node2;

        node = ib_list_node_next_const(node);
        { SCOPED_TRACE("dict2"); CheckNode(node, "dict2", 2, &list2); }
        node2 = ib_list_first_const(list2);
        {
            const ib_list_t      *list3;
            const ib_list_node_t *node3;

            SCOPED_TRACE("dict2:l"); CheckNode(node2, "l", 3, &list3);

            node3 = ib_list_first_const(list3);
            { SCOPED_TRACE("dict2:l[0]"); CheckNode(node3, NULL, 2); }

            node3 = ib_list_node_next_const(node3);
            { SCOPED_TRACE("dict2:l[1]"); CheckNode(node3, NULL, 3); }

            node3 = ib_list_node_next_const(node3);
            { SCOPED_TRACE("dict2:l[2]"); CheckNode(node3, NULL, 4); }
        }

        node2 = ib_list_node_next_const(node2);
        {
            const ib_list_t      *list3;
            const ib_list_node_t *node3;

            SCOPED_TRACE("dict2:d"); CheckNode(node2, "d", 2, &list3);
            node3 = ib_list_first_const(list3);
            { SCOPED_TRACE("dict2:d:v1"); CheckNode(node3, "v1", 1.0); }
            node3 = ib_list_node_next_const(node3);
            { SCOPED_TRACE("dict2:d:v2"); CheckNode(node3, "v2", "two"); }
        }
    }
}

INSTANTIATE_TEST_CASE_P(TestPrettyTrueFalse, TestIBUtilJsonEncode, ::testing::Bool());
