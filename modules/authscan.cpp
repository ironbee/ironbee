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
 * @brief IronBee --- Authenticated Scan Module
 *
 * This module allows IronBee to pass (not inspect and not block) requests
 * that satisfy cryptographic requirements.
 *
 * See the module documentation in the reference manual for deatails.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/clock.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/parsed_header.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/parsed_request_line.hpp>

#include <ironbee/type_convert.h>

#include <openssl/hmac.h>

#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <boost/regex.hpp>

// HMAC C++ wrapping
namespace {

class HmacException : public std::runtime_error {
public:
    HmacException(const std::string& msg);
};

HmacException::HmacException(const std::string& msg)
:
    std::runtime_error(msg)
{
}

class HmacSha256 {
public:

    /**
     * @throws HmacException on error.
     */
    HmacSha256(const void *key, int key_len);

    /**
     * Update this hash.
     * @throws HmacException on error.
     */
    void update(const unsigned char *data, int len);

    void update(const std::string data);

    void update(IronBee::ConstByteString bs);

    /**
     * @throws HmacExcpetion.
     */
    std::vector<unsigned char> finish();

    /**
     * Reset this hash so it may be used again.
     */
    void reset();

    ~HmacSha256();
private:
    HMAC_CTX     m_ctx;
    const void*  m_key;
    int          m_key_len;
};

void HmacSha256::update(const unsigned char *data, int len) {
    int rc = HMAC_Update(&m_ctx, data, len);
    if (rc == 0) {
        BOOST_THROW_EXCEPTION(
            HmacException("Failed to update hash.")
        );
    }
}

void HmacSha256::update(const std::string data) {
    update(
        reinterpret_cast<const unsigned char *>(data.data()),
        data.length()
    );
}

void HmacSha256::update(IronBee::ConstByteString bs)
{
    update(
        reinterpret_cast<const unsigned char*>(bs.const_data()),
        bs.length()
    );
}


std::vector<unsigned char> HmacSha256::finish() {
    std::vector<unsigned char> d(256/8);
    unsigned int len;
    int rc = HMAC_Final(&m_ctx, &d[0], &len);
    if (rc == 0) {
        BOOST_THROW_EXCEPTION(
            HmacException("Failed to finish hash.")
        );
    }

    d.resize(len);

    return d;
}

void HmacSha256::reset() {
    int hmac_rc = HMAC_Init_ex(
        &m_ctx,
        m_key,
        m_key_len,
        EVP_sha256(),
        NULL /* No engine. */
    );
    if (hmac_rc == 0) {
        BOOST_THROW_EXCEPTION(
            HmacException("Failed to reset hash context.")
        );
    }
}

HmacSha256::HmacSha256(const void *key, int key_len)
:
    m_key(key),
    m_key_len(key_len)
{
    HMAC_CTX_init(&m_ctx);

    int hmac_rc;

    hmac_rc = HMAC_Init_ex(
        &m_ctx,
        m_key,
        m_key_len,
        EVP_sha256(),
        NULL /* No engine. */
    );
    if (hmac_rc == 0) {
        BOOST_THROW_EXCEPTION(
            HmacException("Failed to initialize hash context.")
        );
    }
}

HmacSha256::~HmacSha256() {
    HMAC_CTX_cleanup(&m_ctx);
}

} // namespace for HMAC

namespace {

using namespace IronBee;

struct Config {

    /**
     * The request header to examine.
     */
    std::string header;

    /**
     * The secret used to compute the Hmac.
     */
    std::string secret;

    /**
     * Clock skew in seconds. This is always a positive value.
     */
    ib_num_t clock_skew;

    Config() :
        header("X-Auth-Scan"),
        secret(""),
        clock_skew(60 * 5) /* 5 minutes is the default. */
    {}

};

//! Module delegate.
class Delegate :
    public ModuleDelegate
{
private:
    Config       m_default_config;
    boost::regex m_parse_header_re;

    /**
     * @param[in] cp Configuration parser.
     * @param[in] name Directive name.
     * @param[in] param The parameter for this setting.
     */
    void dir_scan_req_header(
        IronBee::ConfigurationParser cp,
        const char*                  name,
        const char*                  param
    ) const;

    /**
     * @param[in] cp Configuration parser.
     * @param[in] name Directive name.
     * @param[in] param The parameter for this setting.
     */
    void dir_grace_period(
        IronBee::ConfigurationParser cp,
        const char*                  name,
        const char*                  param
    ) const;

    /**
     * @param[in] cp Configuration parser.
     * @param[in] name Directive name.
     * @param[in] param The parameter for this setting.
     */
    void dir_shared_secret(
        IronBee::ConfigurationParser cp,
        const char*                  name,
        const char*                  param
    ) const;

    /**
     * Callback the engine calls to handle request headers.
     *
     * @param[in] ib Engine.
     * @param[in] tx Transaction.
     * @param[in] state What state is being called.
     * @param[in] header The header data.
     */
    void handle_headers(
        Engine ib,
        Transaction tx,
        Engine::state_e state,
        ParsedHeader header
    ) const;

    /**
     * Flag the transaction as it should be allowed.
     *
     * @param[in] tx Transcation to allow.
     */
    void allow(Transaction tx) const;

    /**
     * Check if the timestamp is within the given clock skew of now.
     *
     * This function will parse the string into a timestamp.
     * The get the current UTC system time. If the given time
     * minus this time is greater than the allowable clock skew,
     * then false is returned. True otherwise.
     *
     * The clock skew is defined in the current configuration context.
     *
     * @param[in] tx Transaction.
     * @param[in] config The current configuration containing the clock skew.
     * @parma[in] str The timestamp as a string.
     *
     * @returns
     * - True if the timestamp is within the clock skew.
     * - False otherwise.
     */
    bool check_clock_skew(
        Transaction        tx,
        const Config&      config,
        const std::string& str
    ) const;
public:
    Delegate(Module module);

};

void Delegate::dir_scan_req_header(
    IronBee::ConfigurationParser cp,
    const char*                  name,
    const char*                  param
) const
{
    Config& config = module().configuration_data<Config>(cp.current_context());

    config.header = param;
}

void Delegate::dir_grace_period(
    IronBee::ConfigurationParser cp,
    const char*                  name,
    const char*                  param
) const
{
    Config& config = module().configuration_data<Config>(cp.current_context());

    throw_if_error(
        ib_type_atoi(param, 10, &config.clock_skew)
    );

    /* Absolute values only. */
    if (config.clock_skew < 0) {
        config.clock_skew = -config.clock_skew;
    }
}

void Delegate::dir_shared_secret(
    IronBee::ConfigurationParser cp,
    const char*                  name,
    const char*                  param
) const
{
    Config& config = module().configuration_data<Config>(cp.current_context());

    config.secret = param;
}

void Delegate::allow(Transaction tx) const {
    ib_log_debug_tx(tx.ib(), "Allowing Transaction");

    /* Clear any block flags. */
    IronBee::throw_if_error(
        ib_tx_flags_unset(
            tx.ib(),
            IB_TX_FBLOCK_IMMEDIATE |
                IB_TX_FBLOCK_PHASE |
                IB_TX_FBLOCK_ADVISORY
        )
    );

    /* Set the allow flag. */
    IronBee::throw_if_error(
        ib_tx_flags_set(
            tx.ib(),
            IB_TX_FALLOW_ALL
        )
    );
}

bool Delegate::check_clock_skew(
    Transaction tx,
    const Config& config,
    const std::string& str
) const
{
    namespace pt = boost::posix_time;

    /* Check that the time is correct. */
    pt::ptime p = parse_time(str);
    if (p == pt::not_a_date_time) {
        ib_log_debug_tx(
            tx.ib(),
            "Cannot parse date stamp."
        );
        return false;
    }

    pt::ptime now(pt::second_clock::universal_time());
    pt::time_duration td(now - p);

    if (td.is_negative()) {
        td = td.invert_sign();
    }

    /* If the total time duration is greater than the
     * clock skew, this is an invalid request. Skip. */
    if (td.total_seconds() > config.clock_skew) {
        ib_log_debug_tx(
            tx.ib(),
            "Date stamp is outside of the allowable clock skew: %d seconds.",
            td.total_seconds()
        );
        return false;
    }

    return true;
}

void Delegate::handle_headers(
    Engine ib,
    Transaction tx,
    Engine::state_e state,
    ParsedHeader header
) const
{
    Config& config = module().configuration_data<Config>(tx.context());

    for (; header; header = header.next()) {
        std::string header_name = header.name().to_s();

        ib_log_debug_tx(
            tx.ib(),
            "Checking header %.*s",
            static_cast<int>(header_name.length()),
            header_name.data()
        );

        /* Does the header match? */
        if (boost::algorithm::iequals(header.name().to_s(), config.header)) {

            boost::cmatch results;

            if (boost::regex_match(
                    header.value().const_data(),
                    header.value().const_data()  + header.value().length(),
                    results,
                    m_parse_header_re
                )
            )
            {
                try
                {
                    std::string date = results[2].str();

                    /* Compute the HMAC for ourselves. */
                    HmacSha256 hash(
                        config.secret.data(),
                        config.secret.length());

                    ib_log_debug_tx(
                        tx.ib(),
                        "Hashing request line %.*s.",
                        static_cast<int>(tx.request_line().raw().length()),
                        tx.request_line().raw().const_data()
                    );
                    hash.update(tx.request_line().raw());

                    /* Hash the host value. */
                    ib_log_debug_tx(
                        tx.ib(), "Hasing host %s.", tx.hostname()
                    );
                    hash.update(
                        reinterpret_cast<const unsigned char *>(tx.hostname()),
                        strlen(tx.hostname()));

                    /* Hash the date value. */
                    ib_log_debug_tx(
                        tx.ib(), "Hashing %.*s",
                        static_cast<int>(date.length()),
                        date.data()
                    );
                    hash.update(date);

                    /* Tidy up the hash. */
                    std::vector<unsigned char> hash_bytes = hash.finish();

                    /* Convert bytes into hex. */
                    size_t hash_str_len = 2 * 256 / 8;
                    std::vector<char> hash_str(hash_str_len+1);

                    /* For each byte in the binary hash. */
                    for (size_t i = 0; i < hash_bytes.size(); ++i) {
                        /* Convert into 2 bytes for the destiantion output. */
                        snprintf(&hash_str[i*2], 3, "%.2x", hash_bytes[i]);
                    }

                    /* -1 to remove the \0 terminator. */
                    std::string hash_str2(&hash_str[0], hash_str.size()-1);

                    ib_log_debug_tx(
                        tx.ib(),
                        "Computed request hash of %.*s",
                        static_cast<int>(hash_str2.length()),
                        hash_str2.data());

                    /* Validate the hash. */
                    if (
                        ! boost::algorithm::iequals(
                            results[1].str(),
                            hash_str2
                        )
                    )
                    {
                        ib_log_debug_tx(
                            tx.ib(),
                            "Submitted hash %.*s does not equal computed hash %.*s. No action taken.",
                            static_cast<int>(results[1].str().length()),
                            results[1].str().data(),
                            static_cast<int>(hash_str2.length()),
                            hash_str2.data()
                        );

                        return;
                    }

                    if (check_clock_skew(tx, config, date)) {
                        allow(tx);
                    }
                }
                catch (const HmacException& e) {
                    ib_log_debug_tx(
                        tx.ib(),
                        "Hash exception. Cannot validate request."
                    );
                }
            }
        }
    }
}

Delegate::Delegate(Module module)
:
    ModuleDelegate(module),
    m_parse_header_re("^\\s*(\\S*);date=(.*\\S)\\s*$")
{

    module.set_configuration_data(m_default_config);

    module.engine().register_configuration_directives()
        .param1(
            "AuthScanSharedSecret",
            boost::bind(&Delegate::dir_shared_secret, this, _1, _2, _3)
        )
        .param1(
            "AuthScanRequestHeader",
            boost::bind(&Delegate::dir_scan_req_header, this, _1, _2, _3)
        )
        .param1(
            "AuthScanGracePeriod",
            boost::bind(&Delegate::dir_grace_period, this, _1, _2, _3)
        )
        ;

    module.engine().register_hooks()
        .header_data(
            IronBee::ConstEngine::request_header_data,
            boost::bind(&Delegate::handle_headers, this, _1, _2, _3, _4)
        )
        ;
}
} // anonymous namespace end

IBPP_BOOTSTRAP_MODULE_DELEGATE("authscan", Delegate)
