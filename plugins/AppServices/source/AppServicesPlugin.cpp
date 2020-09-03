/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
/*
 * File:   AppServicesPlugin.cpp
 *
 */
#include "AppServicesPlugin.h"

#include <Logging.h>

#include <regex>

#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Registers the AppServicesPlugin plugin object.
 *
 *  The object is constructed at the start of the Dobby daemon and only
 *  destructed when the Dobby daemon is shutting down.
 *
 */
REGISTER_DOBBY_PLUGIN(AppServicesPlugin);



AppServicesPlugin::AppServicesPlugin(const std::shared_ptr<IDobbyEnv>& env,
                                     const std::shared_ptr<IDobbyUtils>& utils)
    : mName("AppServices")
    , mUtilities(utils)
    , mNetfilter(std::make_shared<Netfilter>())
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

AppServicesPlugin::~AppServicesPlugin()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Boilerplate that just returns the name of the hook
 *
 *  This string needs to match the name specified in the container spec json.
 *
 */
std::string AppServicesPlugin::name() const
{
    return mName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Indicates which hook points we want and whether to run the
 *  asynchronously or synchronously with the other hooks
 *
 *  For AppServicesPlugin everything is done in the postConstruction, preStart
 *  and postStop phases.
 */
unsigned AppServicesPlugin::hookHints() const
{
    return (IDobbyPlugin::PostConstructionSync |
            IDobbyPlugin::PreStartAsync |
            IDobbyPlugin::PostStopAsync);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Updates the /etc/services and /etc/hosts file to point to the mapped
 *  AS server.
 *
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  startupState    The start-up state of the container (ignored)
 *  @param[in]  rootfsPath      The absolute path to the rootfs of the container.
 *  @param[in]  jsonData        The parsed json data from the container spec file.
 *
 *  @return true on success, false on failure.
 */
bool AppServicesPlugin::postConstruction(const ContainerId& id,
                                         const std::shared_ptr<IDobbyStartState>& startupState,
                                         const std::string& rootfsPath,
                                         const Json::Value& jsonData)
{
    (void) startupState;

    // get the 'set menu' config which will specify the AS port to use and
    // any additional ports

    // validate / read the json
    const Json::Value setMenu = jsonData["setMenu"];
    if (!setMenu.isString())
    {
        AI_LOG_ERROR_EXIT("'setMenu' field is missing or not a string type");
        return false;
    }

    // get the service number
    static const std::regex matcher(R"(local-services-([0-9]))",
                                    std::regex::ECMAScript | std::regex::icase);

    std::cmatch match;
    if (!std::regex_search(setMenu.asCString(), match, matcher) || (match.size() != 2))
    {
        AI_LOG_ERROR_EXIT("invalid 'setMenu' string");
        return false;
    }

    // the service number determines
    ServicesConfig config;
    switch (match.str(1).front())
    {
        case '1':
            config.asPort = LocalServices1Port;
            config.additionalPorts.insert(8008);
            break;
        case '2':
            config.asPort = LocalServices2Port;
            break;
        case '3':
            config.asPort = LocalServices3Port;
            break;
        case '4':
            config.asPort = LocalServices4Port;
            break;
        case '5':
            config.asPort = LocalServices5Port;
            break;

        default:
            AI_LOG_ERROR_EXIT("invalid 'setMenu' string");
            return false;
    }


    // populate the /etc/service and /etc/hosts files
    int rootfsDirFd = open(rootfsPath.c_str(), O_DIRECTORY | O_CLOEXEC);
    if (rootfsDirFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open rootfs directory for container '%s'",
                         id.c_str());
        return false;
    }

    // write the host file to tell the app that AS is at 100.64.11.1.
    // (nb 100.64.11.1 is the ip address of the dobby0 bridge device, it is
    // fixed, see the Network RDK plugin for more details)
    mUtilities->writeTextFileAt(rootfsDirFd,
                                "etc/hosts",
                                "127.0.0.1\tlocalhost\n"
                                "100.64.11.1\tas\tlocalservices\n",
                                O_CREAT | O_TRUNC | O_WRONLY, 0644);

    // specify the AS port number
    char buf[64];
    snprintf(buf, sizeof(buf), "as\t%hu/tcp\t\t# Sky AS Service\n", config.asPort);
    mUtilities->writeTextFileAt(rootfsDirFd, "etc/services", buf,
                                O_CREAT | O_APPEND | O_WRONLY, 0644);


    if (close(rootfsDirFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, " failed to close dir fd");
    }


    // set the service details
    std::lock_guard<std::mutex> locker(mLock);
    mContainerServices[id] = config;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a DNAT PREROUTING rule to send anything from the container
 *  on the given port to localhost outside the container.
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  containerIp The ip address inside the container.
 *  @param[in]  port        The port number to add the DNAT rule for.
 *
 *  @return the iptables formatted string.
 */
std::string AppServicesPlugin::constructDNATRule(const ContainerId &id,
                                                 const std::string &containerIp,
                                                 in_port_t port) const
{
    char buf[256];

#if defined(DEV_VM)
    const std::string comment("asplugin-" + id.str());
#else
    const std::string comment("\"asplugin-" + id.str() + "\"");
#endif

    snprintf(buf, sizeof(buf), "PREROUTING -s %s/32 -d 100.64.11.1/32 "
                               "-i dobby0 -p tcp -m tcp --dport %hu "
                               "-m comment --comment %s "
                               "-j DNAT --to-destination 127.0.0.1:%hu",
             containerIp.c_str(), port, comment.c_str(), port);

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
 *  @return the iptables formatted string.
 */
std::string AppServicesPlugin::constructACCEPTRule(const ContainerId& id,
                                                   const std::string &containerIp,
                                                   const std::string &vethName,
                                                   in_port_t port) const
{
    char buf[256];

#if defined(DEV_VM)
    const std::string comment("asplugin-" + id.str());
#else
    const std::string comment("\"asplugin-" + id.str() + "\"");
#endif

    snprintf(buf, sizeof(buf), "DobbyInputChain -s %s/32 -d 127.0.0.1/32 "
                               "-i dobby0 -p tcp -m tcp --dport %hu "
                               "-m physdev --physdev-in %s "
                               "-m comment --comment %s "
                               "-j ACCEPT",
             containerIp.c_str(), port, vethName.c_str(), comment.c_str());

    return std::string(buf);
}


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the two iptables firewall rules to enable port forwarding.
 *
 *  The json data is expected (required) to be formatted like the following
 *
 *      {
 *          "setMenu": "local-services-1"
 *      }
 *
 *  The 'setMenu' field is the old way of specifying which services to map into
 *  the container.  It is intended that in the future fine grained API lists
 *  will be specified here.
 *
 *
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  pid         The pid of the processes within the container.
 *  @param[in]  rootfsPath  The absolute path to the rootfs of the container.
 *  @param[in]  jsonData    The parsed json data from the container spec file.
 *
 *  @return true on success, false on failure.
 */
bool AppServicesPlugin::preStart(const ContainerId& id,
                                 pid_t pid,
                                 const std::string& rootfsPath,
                                 const Json::Value& jsonData)
{
    AI_LOG_FN_ENTRY();

    (void) pid;

    // get the ip address and veth name assigned to the container. These are
    // available in the "/dobbyaddress" file in the container rootfs, supplied
    // by the networking plugin
    const std::string addrFilePath = rootfsPath + "/dobbyaddress";
    const std::string addressFileStr = mUtilities->readTextFile(addrFilePath, 100);
    if (addressFileStr.empty())
    {
        AI_LOG_ERROR("failed to get IP address and veth name assigned to"
                     "container from %s", addrFilePath.c_str());
        return false;
    }

    // parse ip address from the read string
    const std::string ipAddress = addressFileStr.substr(0, addressFileStr.find("/"));

    // check if string contains a veth name after the ip address
    if (addressFileStr.length() <= ipAddress.length() + 1)
    {
        AI_LOG_ERROR("failed to get veth name from %s", addrFilePath.c_str());
        return false;
    }

    // parse veth name from the read string
    const std::string vethName = addressFileStr.substr(ipAddress.length() + 1, addressFileStr.length());

    std::lock_guard<std::mutex> locker(mLock);

    // get the service details
    std::map<ContainerId, ServicesConfig>::iterator it = mContainerServices.find(id);
    if (it == mContainerServices.end())
    {
        AI_LOG_ERROR("odd, missing config for container '%s' ?", id.c_str());
        return false;
    }

    ServicesConfig &config = it->second;


    // the service number determines
    std::list<std::string> acceptRules;
    std::list<std::string> natRules;

    // add the AS rules
    acceptRules.emplace_back(constructACCEPTRule(id, ipAddress, vethName, config.asPort));
    natRules.emplace_back(constructDNATRule(id, ipAddress, config.asPort));

    // add any addition port rules
    for (in_port_t port : config.additionalPorts)
    {
        acceptRules.emplace_back(constructACCEPTRule(id, ipAddress, vethName, port));
        natRules.emplace_back(constructDNATRule(id, ipAddress, port));
    }


    // construct the ruleset with just the NAT rules
    Netfilter::RuleSet ruleSet;
    ruleSet[Netfilter::TableType::Filter] = std::move(acceptRules);
    ruleSet[Netfilter::TableType::Nat] = std::move(natRules);

    // add all rules to cache
    if (!mNetfilter->addRules(ruleSet, AF_INET, Netfilter::Operation::Insert))
    {
        AI_LOG_ERROR_EXIT("failed to setup as iptables rules for '%s''", id.c_str());
        return false;
    }

    // Actually apply the rules
    if (!mNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("Failed to apply AS iptables rules for '%s'", id.c_str());
        return false;
    }

    // now finally store the ruleSet so it can removed when the container stops
    config.nfRuleSet.swap(ruleSet);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Post stop hook, we hook this point so we can delete the iptables
 *  firewalls rules added at container start-up
 *
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  rootfsPath  The absolute path to the rootfs of the container.
 *  @param[in]  jsonData    The parsed json data from the container spec file.
 *
 *  @return always returns true.
 */
bool AppServicesPlugin::postStop(const ContainerId& id,
                                 const std::string& rootfsPath,
                                 const Json::Value& jsonData)
{
    AI_LOG_FN_ENTRY();

    // take the lock and remove all the rules added for it
    std::lock_guard<std::mutex> locker(mLock);

    // find the config for the container, contains the netfilter rule sets
    // installed
    std::map<ContainerId, ServicesConfig>::iterator it = mContainerServices.find(id);
    if (it == mContainerServices.end())
    {
        AI_LOG_ERROR("odd, missing config for container '%s' ?", id.c_str());
        return true;
    }

    // delete the rule set
    if (!mNetfilter->addRules(it->second.nfRuleSet, AF_INET, Netfilter::Operation::Delete))
    {
        AI_LOG_ERROR_EXIT("failed to setup AS iptables rules for '%s'", id.c_str());
        return false;
    }

    if (!mNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("Failed to delete AS iptables rules for '%s'", id.c_str());
        return false;
    }

    // remove all the holes from the internal map
    mContainerServices.erase(it);

    AI_LOG_FN_EXIT();
    return true;
}

