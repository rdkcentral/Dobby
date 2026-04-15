/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "AppServicesRdkPlugin.h"

#include <regex>

#include <fcntl.h>

REGISTER_RDK_PLUGIN(AppServicesRdkPlugin);

AppServicesRdkPlugin::AppServicesRdkPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                           const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                           const std::string &rootfsPath)
    : mName("AppServicesRdk"),
      mContainerConfig(containerConfig),
      mUtils(utils),
      mRootfsPath(rootfsPath),
      mPluginConfig(nullptr),
      mNetfilter(std::make_shared<Netfilter>()),
      mEnableConnLimit(false)
{
    AI_LOG_FN_ENTRY();

    if (mContainerConfig == nullptr ||
        mContainerConfig->rdk_plugins->appservicesrdk == nullptr ||
        mContainerConfig->rdk_plugins->appservicesrdk->data == nullptr)
    {
        mValid = false;
    }
    else
    {
        mPluginConfig = mContainerConfig->rdk_plugins->appservicesrdk->data;
        mValid = true;
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 */
unsigned AppServicesRdkPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag);
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 *  Updates the /etc/services and /etc/hosts file to point to the mapped
 *  AS server.
 *
 *  The json data is expected (required) to be formatted like the following
 *
 *      {
 *          "setMenu": "local-services-1",
 *          "additionalPorts": [ 8123 ],
 *          "connLimit": 32
 *      }
 *
 *  The 'setMenu' field is the old way of specifying which services to map into
 *  the container.  It is intended that in the future fine grained API lists
 *  will be specified here.
 *
 *  @return True on success, false on failure.
 */
bool AppServicesRdkPlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("Invalid container config");
        return false;
    }

    // populate the /etc/service file
    // write the host file to tell the app that AS is at 100.64.11.1.
    // (nb 100.64.11.1 is the ip address of the dobby0 bridge device, it is
    // fixed, see the Network RDK plugin for more details)
    const std::string hostsPath = "/etc/hosts";
    if (!mUtils->writeTextFile(mRootfsPath + hostsPath,
                               "127.0.0.1\tlocalhost\n"
                               "100.64.11.1\tas\tlocalservices\n",
                               O_CREAT | O_APPEND | O_WRONLY, 0644))
    {
        AI_LOG_ERROR_EXIT("Failed to write AS IP address to %s", hostsPath.c_str());
        return false;
    }

    // populate the /etc/hosts file
    LocalServicesPort asPort = getAsPort();
    if (asPort == LocalServicesInvalid)
    {
        AI_LOG_ERROR_EXIT("Invalid 'setMenu' string");
        return false;
    }
    if (asPort != LocalServicesNone)
    {
        const std::string servicesPath = "/etc/services";
        char buf[64];
        snprintf(buf, sizeof(buf), "as\t%hu/tcp\t\t# Sky AS Service\n", asPort);
        if (!mUtils->writeTextFile(mRootfsPath + servicesPath,
                                   buf,
                                   O_CREAT | O_APPEND | O_WRONLY, 0644))
        {
            AI_LOG_ERROR_EXIT("Failed to write AS IP address to %s", servicesPath.c_str());
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace.
 * Adds the two iptables firewall rules to enable port forwarding.
 *
 * @return True on success, false on failure.
 */
bool AppServicesRdkPlugin::createRuntime()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("Invalid container config");
        return false;
    }

    // construct the ruleset
    Netfilter::RuleSet ruleSet = constructRules();
    if (ruleSet.empty())
    {
        AI_LOG_ERROR_EXIT("failed to construct AS iptables rules for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    // add all rules to cache
    if (!mNetfilter->addRules(ruleSet, AF_INET, Netfilter::Operation::Insert))
    {
        AI_LOG_ERROR_EXIT("failed to setup AS iptables rules for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    // actually apply the rules
    if (!mNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("Failed to apply AS iptables rules for '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    // Add the localhost masquerade rules inside the container namespace
    // Ideally this would be done in the createContainer hook, but that fails
    // on Llama with permissions issues (works fine on VM...)
    Netfilter::RuleSet masqueradeRuleSet = constructMasqueradeRules();
    if (masqueradeRuleSet.empty())
    {
        AI_LOG_ERROR_EXIT("failed to construct AS iptables masquerade rules for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    const pid_t containerPid = mUtils->getContainerPid();
    if (!mUtils->callInNamespace(containerPid, CLONE_NEWNET,
                                &AppServicesRdkPlugin::setupLocalhostMasquerade, this, masqueradeRuleSet))
    {
        AI_LOG_ERROR_EXIT("Failed to add AS localhost masquerade iptables rules inside container");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

bool AppServicesRdkPlugin::setupLocalhostMasquerade(Netfilter::RuleSet& ruleSet)
{
    std::shared_ptr<Netfilter> nsNetfilter = std::make_shared<Netfilter>();

    // Add rules to cache
    if (!nsNetfilter->addRules(ruleSet, AF_INET, Netfilter::Operation::Insert))
    {
        AI_LOG_ERROR_EXIT("failed to setup AS localhost masquerade iptables rules inside container for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    // actually apply the rules
    if (!nsNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("Failed to apply AS iptables rules for '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    // Enable route_localnet inside the container
    const std::string routingFilename = "/proc/sys/net/ipv4/conf/eth0/route_localnet";
    mUtils->writeTextFile(routingFilename, "1", O_TRUNC | O_WRONLY, 0);

    AI_LOG_FN_EXIT();
    return true;
}


/**
 * @brief Dobby Hook - Run in host namespace when container terminates.
 * We hook this point so we can delete the iptables firewalls rules added at container start-up.
 *
 * @return True on success, false on failure.
 */
bool AppServicesRdkPlugin::postHalt()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("Invalid container config");
        return false;
    }

    // construct the same ruleset as in createRuntime() to delete the rules
    // No need to clean up localhost masqeurade rules as they are removed when
    // container network namespace destroyed
    Netfilter::RuleSet ruleSet = constructRules();
    if (ruleSet.empty())
    {
        AI_LOG_ERROR_EXIT("failed to construct AS iptables rules for deletion for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    // add all rules to cache
    if (!mNetfilter->addRules(ruleSet, AF_INET, Netfilter::Operation::Delete))
    {
        AI_LOG_ERROR_EXIT("failed to setup AS iptables rules for deletion for '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    // actually delete the rules
    if (!mNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("Failed to delete AS iptables rules for '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End hook methods

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> AppServicesRdkPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_app_services_rdk* pluginConfig = mContainerConfig->rdk_plugins->appservicesrdk;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods

/**
 *  @brief Gets the AS port based on the "setMenu" config setting.
 *
 *  @return The AS port if successful, AppServicesRdkPlugin::LocalServicesPort::LocalServicesInvalid otherwise.
 */
AppServicesRdkPlugin::LocalServicesPort AppServicesRdkPlugin::getAsPort() const
{
    AI_LOG_FN_ENTRY();

    // get the 'set menu' config which will specify the AS port to use and
    // any additional ports
    const char* setMenu = mPluginConfig->set_menu;
    if (setMenu != nullptr)
    {
        // get the service number
        static const std::regex matcher(R"(local-services-([0-9])$)",
                                        std::regex::ECMAScript | std::regex::icase);

        std::cmatch match;
        if (!std::regex_search(setMenu, match, matcher) || (match.size() != 2))
        {
            AI_LOG_ERROR_EXIT("invalid 'setMenu' string");
            return LocalServicesInvalid;
        }

        // the service number determines the AS port
        switch (match.str(1).front())
        {
            case '1':
                AI_LOG_FN_EXIT();
                return LocalServices1Port;
            case '2':
                AI_LOG_FN_EXIT();
                return LocalServices2Port;
            case '3':
                AI_LOG_FN_EXIT();
                return LocalServices3Port;
            case '4':
                AI_LOG_FN_EXIT();
                return LocalServices4Port;
            case '5':
                AI_LOG_FN_EXIT();
                return LocalServices5Port;

            default:
                AI_LOG_ERROR_EXIT("invalid 'setMenu' string");
                return LocalServicesInvalid;
        }
    }
    else
    {
        AI_LOG_FN_EXIT();
        return LocalServicesNone;
    }
}

/**
 * @brief Get all the ports we need to forward
 *
 * @return Set of ports on the host that the container should have access to
 */
std::set<in_port_t> AppServicesRdkPlugin::getAllPorts() const
{
    AI_LOG_FN_ENTRY();

    std::set<in_port_t> allPorts = {};

    LocalServicesPort asPort = getAsPort();
    if (asPort != LocalServicesNone && asPort != LocalServicesInvalid)
    {
        allPorts.insert(asPort);
    }

    // LocalServices1 also grants access to port 8008
    if (asPort == LocalServices1Port)
    {
        allPorts.insert(8008);
    }

    // Add any additional ports
    const uint16_t *additionalPorts = mPluginConfig->additional_ports;
    const size_t additionalPortsLen = mPluginConfig->additional_ports_len;
    for (size_t i = 0; i < additionalPortsLen; ++i)
    {
        const in_port_t additionalPort = additionalPorts[i];
        if (additionalPort < 128)
        {
            AI_LOG_WARN("invalid port value (%hu) in additionalPorts array", additionalPort);
        }
        else
        {
            allPorts.insert(additionalPort);
        }
    }

    AI_LOG_FN_EXIT();
    return allPorts;
}

/**
 *  @brief Adds the ACCEPT, DNAT and CONNLIMIT iptables rules for the given port to the given rule sets.
 *
 *  @param[in]      containerIp The ip address inside the container.
 *  @param[in]      vethName    The name of the veth device (outside the container).
 *  @param[in]      port        The port number to add the rules for.
 *  @param[in,out]  acceptRules The ACCEPT rule set.
 *  @param[in,out]  natRules    The DNAT rule set.
 */
void AppServicesRdkPlugin::addRulesForPort(const std::string &containerIp,
                                           const std::string &vethName,
                                           in_port_t port,
                                           std::list<std::string> &acceptRules,
                                           std::list<std::string> &natRules) const
{
    if (mEnableConnLimit)
    {
        acceptRules.emplace_back(
            constructCONNLIMITRule(containerIp, vethName,
                                   port, mPluginConfig->conn_limit));
    }

    acceptRules.emplace_back(constructACCEPTRule(containerIp, vethName, port));
    natRules.emplace_back(constructDNATRule(containerIp, port));
}

/**
 *  @brief Creates the required iptables rules based on the container and plugin config.
 *
 *  @return The created iptables rule set.
 */
Netfilter::RuleSet AppServicesRdkPlugin::constructRules() const
{
    AI_LOG_FN_ENTRY();

    Netfilter::RuleSet ruleSet;

    // get the ip address and veth name assigned to the container
    ContainerNetworkInfo networkInfo;
    if (!mUtils->getContainerNetworkInfo(networkInfo))
    {
        AI_LOG_ERROR("failed to get IP address and veth name assigned to container");
        return ruleSet;
    }
    const std::string &ipAddress = networkInfo.ipAddress;
    const std::string &vethName = networkInfo.vethName;

    // add the AS iptables rules
    std::list<std::string> acceptRules;
    std::list<std::string> natRules;

    const std::set<in_port_t> allPorts = getAllPorts();

    for (const auto &port : allPorts)
    {
        addRulesForPort(ipAddress, vethName, port, acceptRules, natRules);
    }

    ruleSet[Netfilter::TableType::Filter] = std::move(acceptRules);
    ruleSet[Netfilter::TableType::Nat] = std::move(natRules);

    AI_LOG_FN_EXIT();
    return ruleSet;
}

/**
 *  @brief Constructs a DNAT PREROUTING rule to send anything from the container
 *  on the given port to localhost outside the container.
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  containerIp The ip address inside the container.
 *  @param[in]  port        The port number to add the DNAT rule for.
 *
 *  @return The iptables formatted string.
 */
std::string AppServicesRdkPlugin::constructDNATRule(const std::string &containerIp,
                                                    in_port_t port) const
{
    AI_LOG_FN_ENTRY();

    char buf[256];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("asplugin:" + containerId);
#else
    const std::string comment("\"asplugin:" + containerId + "\"");
#endif

    snprintf(buf, sizeof(buf), "PREROUTING -s %s/32 -d 100.64.11.1/32 "
                               "-i dobby0 -p tcp -m tcp --dport %hu "
                               "-m comment --comment %s "
                               "-j DNAT --to-destination 127.0.0.1:%hu",
                               containerIp.c_str(), port, comment.c_str(), port);

    AI_LOG_DEBUG("Constructed rule: %s", buf);
    AI_LOG_FN_EXIT();
    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an INPUT REJECT rule to reject connection if exceed the
 *  limit.
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  containerIp The ip address inside the container.
 *  @param[in]  vethName    The name of the veth device (outside the container)
 *                          that belongs to the container.
 *  @param[in]  port        The port number to add the DNAT rule for.
 *
 *  @return The iptables formatted string.
 */
std::string AppServicesRdkPlugin::constructCONNLIMITRule(const std::string &containerIp,
                                                         const std::string &vethName,
                                                         in_port_t port,
                                                         uint32_t connLimit) const
{
    AI_LOG_FN_ENTRY();

    char buf[256];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("asplugin:" + containerId);
#else
    const std::string comment("\"asplugin:" + containerId + "\"");
#endif

    snprintf(buf, sizeof(buf), "DobbyInputChain -s %s/32 -d 127.0.0.1/32 "
                               "-i dobby0 -p tcp "
                               "-m tcp --dport %hu --tcp-flags FIN,SYN,RST,ACK SYN "
                               "-m connlimit --connlimit-above %u --connlimit-mask 32 --connlimit-saddr "
                               "-m comment --comment %s "
                               "-j REJECT --reject-with tcp-reset",
                               containerIp.c_str(), port, connLimit, comment.c_str());

    AI_LOG_DEBUG("Constructed rule: %s", buf);
    AI_LOG_FN_EXIT();
    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a INPUT ACCEPT rule to allow packets from the container
 *  over the dobby0 bridge to localhost.
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  containerIp The ip address inside the container.
 *  @param[in]  vethName    The name of the veth device (outside the container)
 *                          that belongs to the container.
 *  @param[in]  port        The port number to add the DNAT rule for.
 *
 *  @return The iptables formatted string.
 */
std::string AppServicesRdkPlugin::constructACCEPTRule(const std::string &containerIp,
                                                      const std::string &vethName,
                                                      in_port_t port) const
{
    AI_LOG_FN_ENTRY();

    char buf[256];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("asplugin:" + containerId);
#else
    const std::string comment("\"asplugin:" + containerId + "\"");
#endif

    snprintf(buf, sizeof(buf), "DobbyInputChain -s %s/32 -d 127.0.0.1/32 "
                               "-i dobby0 -p tcp -m tcp --dport %hu "
                               "-m physdev --physdev-in %s "
                               "-m comment --comment %s "
                               "-j ACCEPT",
                               containerIp.c_str(), port, vethName.c_str(), comment.c_str());

    AI_LOG_DEBUG("Constructed rule: %s", buf);
    AI_LOG_FN_EXIT();
    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 * @brief Constructs rules to forward requests to AS ports on the container localhost
 * interface to the host.
 *
 * Simplified version of portForwarding code in Networking plugin
 *
 * @return RuleSet to configure iptables
 */
Netfilter::RuleSet AppServicesRdkPlugin::constructMasqueradeRules() const
{
    AI_LOG_FN_ENTRY();

    Netfilter::RuleSet ruleSet;
    const std::string containerId = mUtils->getContainerId();

    // get the ip address and veth name assigned to the container
    ContainerNetworkInfo networkInfo;
    if (!mUtils->getContainerNetworkInfo(networkInfo))
    {
        AI_LOG_ERROR("failed to get IP address and veth name assigned to container");
        return ruleSet;
    }

    std::list<std::string> natRules;
    const std::set<in_port_t> allPorts = getAllPorts();

    for (const auto &port : allPorts)
    {
        const std::string dnatRule = createMasqueradeDnatRule(port);
        natRules.emplace_back(dnatRule);
    }

    std::string snatRule = createMasqueradeSnatRule(networkInfo.ipAddress);
    natRules.emplace_back(snatRule);

    ruleSet[Netfilter::TableType::Nat] = std::move(natRules);

    AI_LOG_FN_EXIT();
    return ruleSet;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an OUTPUT DNAT rule to forward packets from 127.0.0.1 inside
 *  the container to the bridge device (100.64.11.1) on the given port
 *
 *  @param[in]  portForward The port to forward.
 *
 *  @return returns the created rule.
 */
std::string AppServicesRdkPlugin::createMasqueradeDnatRule(const in_port_t &port) const
{
    char buf[256];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("asplugin:" + containerId);
#else
    const std::string comment("\"asplugin:" + containerId + "\"");
#endif

    std::string baseRule("OUTPUT "
                         "-o lo "
                         "-p tcp "      // protocol
                         "-m tcp "      // protocol
                         "--dport %hu " // port number
                         "-j DNAT "
                         "-m comment --comment %s "         // Container id
                         "--to-destination 100.64.11.1:%hu" // Bridge address:port
    );

    // populate fields in base rule
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             port,
             comment.c_str(),
             port);

    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an POSTROUTING SNAT rule so that the source address is changed
 *  to the veth0 inside the container so we get the replies.
 *
 *  @param[in]  ipAddress   The ip address of the container.
 *
 *  @return returns the created rule.
 *
 */
std::string AppServicesRdkPlugin::createMasqueradeSnatRule(const std::string &ipAddress) const
{
    char buf[256];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("asplugin:" + containerId);
#else
    const std::string comment("\"asplugin:" + containerId + "\"");
#endif

    std::string baseRule("POSTROUTING "
                         "-p tcp "         // protocol
                         "-s 127.0.0.1 "   // container localhost
                         "-d 100.64.11.1 " // bridge address
                         "-j SNAT "
                         "-m comment --comment %s " // container id
                         "--to %s");                // container address

    // populate fields in base rule
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             comment.c_str(),
             ipAddress.c_str());

    return std::string(buf);
}
