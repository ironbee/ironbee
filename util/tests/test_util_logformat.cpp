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
/// @brief IronBee --- Logformat Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/mm.h>
#include <ironbee/util.h>
#include <ironbee/logformat.h>

#include "gtest/gtest.h"
#include "simple_fixture.hpp"

#include <stdexcept>

/* -- Tests -- */

class TestIBUtilLogformat : public SimpleFixture
{
};

/// @test Test util logformat library - ib_logformat_create() and *_set()
TEST_F(TestIBUtilLogformat, test_create)
{
    ib_status_t rc;

    ib_logformat_t *lf = NULL;

    rc = ib_logformat_create(MM(), &lf);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(lf->items);
}

#define REMOTE_IP  "10.10.10.10"
#define LOCAL_IP   "192.168.1.1"
#define HOST_NAME  "myhost.some.org"
#define SITE_ID    "AAAABBBB-1111-2222-3333-000000000000"
#define SENSOR_ID  "AAAABBBB-1111-2222-3333-FFFF00000023"
#define TX_ID      "00001111-1111-2222-3333-444455556666"
#define TIME_STAMP "2012-01-23:34:56.4567-0600"
#define LOG_FILE   "/tmp/my_file.log"

ib_status_t format_field(
    const ib_logformat_t        *lf,
    const ib_logformat_field_t  *field,
    const void                  *cbdata,
    const char                 **str)
{
    assert(cbdata == NULL);

    switch (field->fchar) {
    case IB_LOG_FIELD_REMOTE_ADDR:
        *str = REMOTE_IP;
        break;
    case IB_LOG_FIELD_LOCAL_ADDR:
        *str = LOCAL_IP;
        break;
    case IB_LOG_FIELD_HOSTNAME:
        *str = HOST_NAME;
        break;
    case IB_LOG_FIELD_SITE_ID:
        *str = SITE_ID;
        break;
    case IB_LOG_FIELD_SENSOR_ID:
        *str = SENSOR_ID;
        break;
    case IB_LOG_FIELD_TRANSACTION_ID:
        *str = TX_ID;
        break;
    case IB_LOG_FIELD_TIMESTAMP:
        *str = TIME_STAMP;
        break;
    case IB_LOG_FIELD_LOG_FILE:
        *str = LOG_FILE;
        break;
    default:
        *str = "\n";
        /* Not understood */
        return IB_EINVAL;
        break;
    }
    return IB_OK;
}

static const size_t buflen = 8192;
static const size_t trunclen = 64;
TEST_F(TestIBUtilLogformat, test_parse_default)
{
    ib_status_t rc;
    ib_logformat_t *lf = NULL;
    const ib_list_node_t *node;
    const ib_logformat_item_t *item;
    static char linebuf[buflen + 1];
    static const char *formatted = \
        TIME_STAMP " " HOST_NAME " " REMOTE_IP " " SENSOR_ID " " SITE_ID " " \
        TX_ID " " LOG_FILE;
    size_t len;

    rc = ib_logformat_create(MM(), &lf);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_logformat_parse(lf, IB_LOGFORMAT_DEFAULT);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_STREQ(IB_LOGFORMAT_DEFAULT, lf->format);

    ASSERT_EQ(13U, ib_list_elements(lf->items));

    node = ib_list_first_const(lf->items);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_format, item->itype);
    ASSERT_EQ('T', item->item.field.fchar);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_literal, item->itype);
    ASSERT_STREQ(" ", item->item.literal.buf.short_str);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_format, item->itype);
    ASSERT_EQ('h', item->item.field.fchar);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_literal, item->itype);
    ASSERT_STREQ(" ", item->item.literal.buf.short_str);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_format, item->itype);
    ASSERT_EQ('a', item->item.field.fchar);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_literal, item->itype);
    ASSERT_STREQ(" ", item->item.literal.buf.short_str);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_format, item->itype);
    ASSERT_EQ('S', item->item.field.fchar);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_literal, item->itype);
    ASSERT_STREQ(" ", item->item.literal.buf.short_str);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_format, item->itype);
    ASSERT_EQ('s', item->item.field.fchar);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_literal, item->itype);
    ASSERT_STREQ(" ", item->item.literal.buf.short_str);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_format, item->itype);
    ASSERT_EQ('t', item->item.field.fchar);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_literal, item->itype);
    ASSERT_STREQ(" ", item->item.literal.buf.short_str);

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    item = (const ib_logformat_item_t *)node->data;
    ASSERT_EQ(item_type_format, item->itype);
    ASSERT_EQ('f', item->item.field.fchar);

    rc = ib_logformat_format(lf, linebuf, buflen, &len, format_field, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(formatted, linebuf);

    /* Verify that truncation is handled correctly */
    rc = ib_logformat_format(lf, linebuf, trunclen, &len, format_field, NULL);
    ASSERT_EQ(IB_ETRUNC, rc);
    ASSERT_EQ(len, trunclen-1);
    char trunc_buf[trunclen];
    strncpy(trunc_buf, formatted, trunclen-1);
    trunc_buf[trunclen-1] = '\0';
    ASSERT_STREQ(trunc_buf, linebuf);
}

/// @test Test util logformat library - ib_logformat_parse()
TEST_F(TestIBUtilLogformat, test_parse_custom1)
{
    ib_status_t rc;
    ib_logformat_t *lf = NULL;
    size_t len;
    static char linebuf[buflen + 1];
    static const char *formatted = \
        "MyFormat " SITE_ID " " SENSOR_ID " " HOST_NAME " " LOG_FILE " END";

    rc = ib_logformat_create(MM(), &lf);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_logformat_parse(lf, "MyFormat %s %S %h %f END");
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(9U, ib_list_elements(lf->items));

    rc = ib_logformat_format(lf, linebuf, buflen, &len, format_field, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(formatted, linebuf);
}

TEST_F(TestIBUtilLogformat, test_parse_custom2)
{
    ib_status_t rc;
    ib_logformat_t *lf = NULL;
    size_t len;
    static char linebuf[buflen + 1];
    static const char *formatted = \
        "Start" SITE_ID SENSOR_ID " " HOST_NAME LOG_FILE "End";

    rc = ib_logformat_create(MM(), &lf);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_logformat_parse(lf, "Start%s%S %h%fEnd");
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(7U, ib_list_elements(lf->items));

    rc = ib_logformat_format(lf, linebuf, buflen, &len, format_field, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(formatted, linebuf);
}

TEST_F(TestIBUtilLogformat, test_parse_custom3)
{
    ib_status_t rc;
    ib_logformat_t *lf = NULL;
    size_t len;
    static char linebuf[buflen + 1];
    static const char *formatted = \
        "Start" SITE_ID " \\ " SENSOR_ID " " HOST_NAME "  \t" LOG_FILE " %End";

    rc = ib_logformat_create(MM(), &lf);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_logformat_parse(lf, "Start%s \\\\ %S %h\\n\\r\\t%f %%End");
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(9U, ib_list_elements(lf->items));

    rc = ib_logformat_format(lf, linebuf, buflen, &len, format_field, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(formatted, linebuf);
}
