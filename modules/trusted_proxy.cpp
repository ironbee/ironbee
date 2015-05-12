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
 * @brief IronBee --- Trusted Proxy Module
 */

// Include all of IronBee++
#include <ironbeepp/all.hpp>
#include <ironbee/ipset.h>
#include <ironbee/string.h>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <vector>

using namespace std;

namespace {

/**
 * Configuration for the Trusted Proxy Module
 */
class TrustedProxyConfig
{
public:
    //! Constructor.
    TrustedProxyConfig();

    //! Is X-Forwarded-For handling enabled?
    bool is_xff_enabled() const;

    //! Enable or disable X-Forwarded-For handling.
    void set_xff_enabled(bool enabled);

    //! Clear the trusted and untrusted network lists.
    void clear_networks();

    //! Add a network to the trusted list.
    void add_trusted_network(const char* network);

    //! Add a network to the untrusted list.
    void add_untrusted_network(const char* network);

    /**
     * Check if the ip address is trusted.
     *
     * The default is to trust all addresses.
     *
     * @param[in] ipstr IP address to check.
     *
     * @returns True if the address is trusted.
     */
    bool is_trusted(const char* ipstr) const;

    //! Finalize the configuration when the context is closed.
    void context_close(IronBee::Engine& ib);

private:
    //! X-Forwarding-For handling enabled?
    bool m_xff_enabled;

    //! List of trusted networks
    vector<ib_ipset4_entry_t> m_trusted_net_list;

    //! List of untrusted networks
    vector<ib_ipset4_entry_t> m_untrusted_net_list;

    //! IP set of the trusted and untrusted networks.
    ib_ipset4_t m_trusted_networks;
};

/**
 * Module to handle X-Forwarded-For headers from trusted Proxies.
 */
class TrustedProxyModule : public IronBee::ModuleDelegate
{
public:
    //! Constructor.
    explicit TrustedProxyModule(IronBee::Module module);

private:
    //! Source for recording the remote address
    IronBee::VarSource m_remote_addr_source;

    /**
     * Handle the TrustedProxyUseXFFHeader directive.
     *
     * @param[in] cp Configuration parser.
     * @param[in] enabled Should XFF handling be enabled?
     */
    void enable_xff_directive(
        IronBee::ConfigurationParser cp,
        bool                         enabled);

    /**
     * @param[in] cp Configuration parser.
     * @param[in] ip_list List of IPs or CIDR blocks.
     */
    void trusted_ips_directive(
        IronBee::ConfigurationParser    cp,
        IronBee::ConstList<const char*> ip_list);

    void on_context_close(IronBee::Engine ib, IronBee::Context ctx);

    /**
     * Update the transaction effective IP based.
     *
     * @param[in] ib Ironbee engine.
     * @param[in,out] tx Transaction to update.
     */
    void set_effective_ip(
        IronBee::Engine      ib,
        IronBee::Transaction tx);
};

} // Anonymous namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("TrustedProxyModule", TrustedProxyModule)

namespace {

void make_ipset_entry(const char* cidr_or_ip, ib_ipset4_entry_t& entry)
{
    if (strchr(cidr_or_ip, '/') != NULL) {
        // Has / assume CIDR
        IronBee::throw_if_error(
            ib_ip4_str_to_net(cidr_or_ip, &(entry.network)),
            "Invalid CIDR block");
    }
    else {
        // IP make /32
        IronBee::throw_if_error(
            ib_ip4_str_to_ip(cidr_or_ip, &(entry.network.ip)),
            "Invalid IP address");
        entry.network.size=32;
    }
}

TrustedProxyConfig::TrustedProxyConfig()
    : m_xff_enabled(true)
{
    ib_ipset4_init(&m_trusted_networks, NULL, 0, NULL, 0);
}

bool TrustedProxyConfig::is_xff_enabled() const
{
    return m_xff_enabled;
}

void TrustedProxyConfig::set_xff_enabled(bool enabled)
{
    m_xff_enabled = enabled;
}

void TrustedProxyConfig::clear_networks()
{
    m_trusted_net_list.clear();
    m_untrusted_net_list.clear();
}

void TrustedProxyConfig::add_trusted_network(const char* cidr_or_ip)
{
    ib_ipset4_entry_t net_entry;
    make_ipset_entry(cidr_or_ip, net_entry);
    m_trusted_net_list.push_back(net_entry);
}

void TrustedProxyConfig::add_untrusted_network(const char* cidr_or_ip)
{
    ib_ipset4_entry_t net_entry;
    make_ipset_entry(cidr_or_ip, net_entry);
    m_untrusted_net_list.push_back(net_entry);
}

void TrustedProxyConfig::context_close(IronBee::Engine& ib)
{
    IronBee::throw_if_error(
        ib_ipset4_init(
            &m_trusted_networks,
            m_untrusted_net_list.data(),
            m_untrusted_net_list.size(),
            m_trusted_net_list.data(),
            m_trusted_net_list.size()),
        "Failed to initialize IPv4 set.");
}

bool TrustedProxyConfig::is_trusted(const char* ipstr) const
{
    ib_ip4_t ip;

    IronBee::throw_if_error(
        ib_ip4_str_to_ip(ipstr, &ip),
        "Invalid remote IP address");
    ib_status_t rc =
        ib_ipset4_query(&m_trusted_networks, ip, NULL, NULL, NULL);
    return rc == IB_OK;
}

TrustedProxyModule::TrustedProxyModule(IronBee::Module module) :
    IronBee::ModuleDelegate(module)
{
    module.set_configuration_data<TrustedProxyConfig>();

    m_remote_addr_source = IronBee::VarSource::register_(
        module.engine().var_config(),
        IB_S2SL("remote_addr"),
        IB_PHASE_REQUEST_HEADER,
        IB_PHASE_REQUEST_HEADER);

    module.engine().register_configuration_directives()
        .on_off(
            "TrustedProxyUseXFFHeader",
            boost::bind(&TrustedProxyModule::enable_xff_directive,
                        this, _1, _3))
        .list(
            "TrustedProxyIPs",
            boost::bind(&TrustedProxyModule::trusted_ips_directive,
                        this, _1, _3));

    module.engine().register_hooks()
        .context_close(
            boost::bind(&TrustedProxyModule::on_context_close,
                        this, _1, _2))
        .handle_context_transaction(
            boost::bind(&TrustedProxyModule::set_effective_ip,
                        this, _1, _2));
}

void TrustedProxyModule::enable_xff_directive(
    IronBee::ConfigurationParser cp,
    bool                         enabled
)
{
    TrustedProxyConfig& config =
        module().configuration_data<TrustedProxyConfig>(cp.current_context());
    config.set_xff_enabled(enabled);
}

void TrustedProxyModule::trusted_ips_directive(
    IronBee::ConfigurationParser    cp,
    IronBee::ConstList<const char*> ip_list
)
{
    TrustedProxyConfig& config =
        module().configuration_data<TrustedProxyConfig>(cp.current_context());

    const char* first_arg = ip_list.front();
    if (*first_arg != '+' and *first_arg != '-') {
        config.clear_networks();
    }

    BOOST_FOREACH(const char* arg, ip_list) {
        if (*arg == '+') {
            config.add_trusted_network(arg+1);
        }
        else if (*arg == '-') {
            config.add_untrusted_network(arg+1);
        }
        else {
            config.add_trusted_network(arg);
        }
    }
}

void TrustedProxyModule::on_context_close(
    IronBee::Engine ib,
    IronBee::Context ctx
)
{
    TrustedProxyConfig& config =
        module().configuration_data<TrustedProxyConfig>(ctx);

    config.context_close(ib);
}

void TrustedProxyModule::set_effective_ip(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    using namespace boost::algorithm;

    ib_status_t rc;
    IronBee::Context ctx = tx.context();
    TrustedProxyConfig& config =
        module().configuration_data<TrustedProxyConfig>(ctx);

    ib_log_debug_tx(tx.ib(), "checking: %s",
                    tx.connection().remote_ip_string());
    // check actual remote ip against trusted ips
    if (! config.is_trusted(tx.connection().remote_ip_string())) {
        ib_log_debug_tx(tx.ib(), "Remote address '%s' not a trusted proxy.",
                        tx.connection().remote_ip_string());
        return;
    }

    // Last remote address is trusted, get the last X-Forwarded-For value.
    string forwarded;
    for (
        IronBee::ParsedHeader header = tx.request_header();
        header;
        header = header.next()
    )
    {
        if (iequals("X-Forwarded-For", header.name().to_s())) {
            forwarded = header.value().to_s();
        }
    }

    if (forwarded.length() == 0) {
        return;
    }

    list<string> forwarded_list;

    split(forwarded_list, forwarded, is_any_of(","));

    string remote_ip = trim_copy(forwarded_list.back());

    /* Verify that it looks like a valid IP address, ignore it if not */
    rc = ib_ip_validate(remote_ip.c_str());
    if (rc != IB_OK) {
        ib_log_error_tx(tx.ib(),
                        "X-Forwarded-For \"%s\" is not a valid IP address",
                        remote_ip.c_str());
        return;
    }
    char* buf = static_cast<char*>(tx.memory_manager().alloc(remote_ip.length()+1));
    strcpy(buf, remote_ip.c_str());

    /* This will lose the pointer to the original address
     * buffer, but it should be cleaned up with the rest
     * of the memory pool. */
    tx.ib()->remote_ipstr = buf;

    ib_log_debug_tx(tx.ib(), "Remote address changed to \"%s\"", buf);

    IronBee::ByteString remote_addr_bs = IronBee::ByteString::create_alias(
        tx.memory_manager(),
        buf);

    try {
        IronBee::Field f = m_remote_addr_source.get(tx.var_store());
        f.set_no_copy_byte_string(remote_addr_bs);
    }
    catch (const IronBee::enoent &e) {
        IronBee::Field remote_addr_field = IronBee::Field::create_byte_string(
            tx.memory_manager(),
            IB_S2SL("REMOTE_ADDR"),
            remote_addr_bs);
        m_remote_addr_source.set(tx.var_store(), remote_addr_field);
    }
}

} // Anonymous namespace
