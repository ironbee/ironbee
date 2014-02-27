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

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Simple fixture for Ironbee tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#ifndef __IBTEST_LOG_FIXTURE_H__
#define __IBTEST_LOG_FIXTURE_H__

#include <ironbee/mm.h>

#include "gtest/gtest.h"

#include <stdexcept>
#include <string>
#include <stdexcept>

#include <boost/regex.hpp>


class IBLogFixture : public testing::Test
{
public:
    enum { linebuf_size = 1024 };
    IBLogFixture() : log_fp(NULL)
    {
    }

    virtual ~IBLogFixture()
    {
        TearDown( );
    }

    virtual void SetUp()
    {
        Close( );
        log_fp = tmpfile();
        if (log_fp == NULL) {
            throw std::runtime_error("Failed to open temporary file.");
        }
    }

    virtual void TearDown()
    {
        Close( );
    }

    virtual void ClosedFp() { };
    void Close( )
    {
        if (log_fp != NULL) {
            fclose(log_fp);
            log_fp = NULL;
            ClosedFp( );
        }
    }

    const std::string &Cat()
    {
        fpos_t pos;

        if (fgetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to get file position.");
        }
        rewind(log_fp);

        catbuf = "";
        while(fgets(linebuf, linebuf_size, log_fp) != NULL) {
            catbuf += linebuf;
        }
        if (fsetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to set file position.");
        }
        return catbuf;
    }

    bool Grep(const std::string &pat)
    {
        boost::regex re = boost::regex(pat);
        bool match_found = false;
        fpos_t pos;

        if (fgetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to get file position.");
        }
        rewind(log_fp);

        while(fgets(linebuf, linebuf_size, log_fp) != NULL) {
            bool result = boost::regex_search(linebuf, re);
            if(result) {
                match_found = true;
                break;
            }
        }
        if (fsetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to set file position.");
        }

        return match_found;
    }

    bool Grep(const char *pat)
    {
        return Grep( std::string(pat) );
    }

    bool Grep(const std::string &p1, const std::string &p2)
    {
        std::string pat;
        pat = p1;
        pat += ".*";
        pat += p2;
        return Grep(pat);
    }

protected:
    mutable std::string  catbuf;
    FILE                *log_fp;
    char                 linebuf[linebuf_size+1];
};

#endif /* __IBTEST_LOG_FIXTURE_H__ */
