/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#include "NetworkSetup.h"
#include "Netlink.h"
#include "BridgeInterface.h"

#include "Netfilter.h"

#include <Logging.h>

#include <fcntl.h>
#include <unistd.h>

// -----------------------------------------------------------------------------
/**
 *  @brief Modifies a rule set replacing all entires which have '%1' in them
 *  with actual external interface names.
 *
 *  @param[in]  ruleSet                 Iptables ruleset to expand.
 *  @param[in]  extIfaces               External interfaces on the device.
 *
 *  This will add new rules to the set if there are more than one external
 *  interfaces defined in the set.
 */
void expandRuleSetForExtIfaces(Netfilter::RuleSet *ruleSet, const std::vector<std::string> &extIfaces)
{
    for (auto & table : *ruleSet)
    {
        std::list<std::string> &rules = table.second;

        auto it = rules.begin();
        while (it != rules.end())
        {
            std::string &rule = *it;

            size_t pos = rule.find("%1");
            if (pos == std::string::npos)
            {
                // no wildcard interface fields so just ignore
                ++it;
            }
            else
            {
                // add new entries for each external interface
                for (const std::string &extIface : extIfaces)
                {
                    std::string newRule = rule;
                    newRule.replace(pos, 2, extIface);
                    rules.emplace(it, std::move(newRule));
                }

                // remove the template rule
                it = rules.erase(it);
            }
        }
    }

}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the NAT rules into rulesets that can be added or removed
 *
 *  @param[in]  netfilter               Instance of Netfilter.
 *  @param[in]  extIfaces               External interfaces on the device.
 *
 *  @return returns a vector containing the constructed rules
 */
std::vector<Netfilter::RuleSet> constructBridgeRules(const std::shared_ptr<Netfilter> &netfilter,
                                                     const std::vector<std::string> &extIfaces)
{
    // start with creating a new chain to filter input into the dobby bridge
    // device
    if (!netfilter->createNewChain(Netfilter::TableType::Filter,
                                   "DobbyInputChain", false))
    {
        AI_LOG_ERROR_EXIT("failed to create iptables 'DobbyInputChain' chain");
        return std::vector<Netfilter::RuleSet>();
    }

    std::vector<Netfilter::RuleSet> ruleSets;

    Netfilter::RuleSet insertRuleSet =
    {
        {   Netfilter::TableType::Filter,
            {
                "INPUT -i " BRIDGE_NAME " -j DobbyInputChain",
                "FORWARD -d " BRIDGE_ADDRESS_RANGE "/24 -i %1 -o " BRIDGE_NAME " -m state --state INVALID -j DROP",
                "FORWARD -s " BRIDGE_ADDRESS_RANGE "/24 -i " BRIDGE_NAME " -o %1 -m state --state INVALID -j DROP",
                "OUTPUT -s " BRIDGE_ADDRESS_RANGE "/24 -o %1 -j DROP"
            }
        },
    };

    // replace the %1 in the above ruleset with the name of external interface(s)
    expandRuleSetForExtIfaces(&insertRuleSet, extIfaces);
    ruleSets.emplace_back(insertRuleSet);

    // the following rule set was obtained by looking at what libvirt had setup
    // for the NAT connection, we're just replicating
    Netfilter::RuleSet appendRuleSet =
    {
        {   Netfilter::TableType::Filter,
            {
                // enable traffic between external interface and dobby bridge device
                "FORWARD -d " BRIDGE_ADDRESS_RANGE "/24 -i %1 -o " BRIDGE_NAME " -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT",
                "FORWARD -s " BRIDGE_ADDRESS_RANGE "/24 -i " BRIDGE_NAME " -o %1 -j ACCEPT",
                "FORWARD -i " BRIDGE_NAME " -o %1 -j ACCEPT",
                "FORWARD -o " BRIDGE_NAME " -j REJECT --reject-with icmp-port-unreachable",
                "FORWARD -i " BRIDGE_NAME " -j REJECT --reject-with icmp-port-unreachable",
            }
        },
        {   Netfilter::TableType::Nat,
            {
                // setup NAT'ing on the BRIDGE_ADDRESS_RANGE/24 ip range through external interface
                "POSTROUTING -s " BRIDGE_ADDRESS_RANGE "/24 -d 224.0.0.0/24 ! -o " BRIDGE_NAME " -j RETURN",
                "POSTROUTING -s " BRIDGE_ADDRESS_RANGE "/24 -d 255.255.255.255/32 ! -o " BRIDGE_NAME " -j RETURN",
                "POSTROUTING -s " BRIDGE_ADDRESS_RANGE "/24 ! -d " BRIDGE_ADDRESS_RANGE "/24 ! -o " BRIDGE_NAME " -p tcp -j MASQUERADE --to-ports 1024-65535",
                "POSTROUTING -s " BRIDGE_ADDRESS_RANGE "/24 ! -d " BRIDGE_ADDRESS_RANGE "/24 ! -o " BRIDGE_NAME " -p udp -j MASQUERADE --to-ports 1024-65535",
                "POSTROUTING -s " BRIDGE_ADDRESS_RANGE "/24 ! -d " BRIDGE_ADDRESS_RANGE "/24 ! -o " BRIDGE_NAME " -j MASQUERADE",
            }
        }
    };

    // replace the %1 in the above ruleset with the name of external interface(s)
    expandRuleSetForExtIfaces(&appendRuleSet, extIfaces);
    ruleSets.emplace_back(appendRuleSet);

    return ruleSets;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Creates a netfilter rule to drop packets received from the veth into
 *  the bridge if they don't have the correct source IP address.
 *
 *  @param[in]  ipAddress       The ip address of the container.
 *  @param[in]  vethName        The name of the veth of the container.
 *
 *  @return returns the created ruleset
 */
Netfilter::RuleSet createAntiSpoofRule(const std::string ipAddress,
                                       const std::string &vethName)
{
    // construct the rule
    char filterRule[128];
    snprintf(filterRule, sizeof(filterRule),
             "DobbyInputChain ! -s %s/32 -i " BRIDGE_NAME " -m physdev --physdev-in %s -j DROP",
             ipAddress.c_str(),
             vethName.c_str());

    // return the rule in the default filter table
    return Netfilter::RuleSet {{ Netfilter::TableType::Filter, { filterRule } }};
}


// -----------------------------------------------------------------------------
/**
 *  @brief Creates a netfilter rule to drop all packets that are sent to the
 *  bridge via the given veth interface.
 *
 *  This is used for private networking, ie. it will block all access to the
 *  wan / lan.  However it is expected that we can still route some traffic
 *  to localhost outside the container by adding new ACCEPT rules prior to
 *  this one.
 *
 *  @param[in]  vethName        The name of the veth of the container.
 *
 *  @return returns the created ruleset
 */
Netfilter::RuleSet createDropAllRule(const std::string &vethName)
{
    // construct the rule
    char filterRule[128];
    snprintf(filterRule, sizeof(filterRule),
             "DobbyInputChain -i " BRIDGE_NAME " -m physdev --physdev-in %s -j DROP",
             vethName.c_str());

    // return the rule in the default filter table
    return Netfilter::RuleSet {
        { Netfilter::TableType::Filter, { filterRule } }};
}


// -----------------------------------------------------------------------------
/**
 *  @brief Called from host namespace
 *
 *  This function will create the bridge device and configure it. Only run if
 *  the bridge device hasn't already been created by another container starting.
 *
 *  @param[in]  utils                   Instance of DobbyRdkPluginUtils.
 *  @param[in]  netfilter               Instance of Netfilter.
 *  @param[in]  extIfaces               External interfaces on the device.
 *
 *  @return true if successful, otherwise false
 */
bool NetworkSetup::setupBridgeDevice(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                     const std::shared_ptr<Netfilter> &netfilter,
                                     const std::vector<std::string> &extIfaces)
{
    AI_LOG_FN_ENTRY();

    std::shared_ptr<Netlink> netlink = std::make_shared<Netlink>();
    if (!netlink->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create netlink object");
        return false;
    }

    // step 1 - create the bridge device needed for NAT / container networking
    if (!BridgeInterface::createBridge(netlink))
    {
        AI_LOG_ERROR_EXIT("failed to create bridge interface with name '%s'", BRIDGE_NAME);
        return false;
    }

    // step 2 - disable Spanning Tree Protocol (STP)
    if (!BridgeInterface::disableStp(utils))
    {
        AI_LOG_ERROR("failed to disable STP");
    }

    // step 3 - assign an IP address to the bridge
    if (!BridgeInterface::setAddress(netlink, INADDR_BRIDGE, INADDR_BRIDGE_NETMASK))
    {
        AI_LOG_ERROR_EXIT("failed to set the ip address on the bridge interface");
        return false;
    }

    // step 4 - construct the rules to be added to iptables and then add them
    std::vector<Netfilter::RuleSet> ruleSets = constructBridgeRules(netfilter, extIfaces);

    // set the iptables drop rules for the NAT
    if (!netfilter->insertRules(ruleSets[0]))
    {
        AI_LOG_ERROR_EXIT("failed to setup iptables drop rules for NAT");
        return false;
    }

    // set the iptables rules for the NAT
    if (!netfilter->appendRules(ruleSets[1]))
    {
        AI_LOG_ERROR_EXIT("failed to setup iptables forwarding rules for NAT");
        return false;
    }

    // step 5 - bring the bridge interface up
    if (!BridgeInterface::up(netlink))
    {
        AI_LOG_ERROR_EXIT("failed to bring the bridge interface up");
        return false;
    }

    // step 6 - enable ip forwarding on our local NAT bridge and external ifaces
    for (const std::string &extIface : extIfaces)
    {
        if (!netlink->setIfaceForwarding(extIface, true))
        {
            AI_LOG_ERROR("failed to enable forwarding on interface '%s'",
                         extIface.c_str());
        }
    }

    if (!BridgeInterface::setIfaceForwarding(utils, netlink, true))
    {
        AI_LOG_ERROR_EXIT("failed to enable forwarding on the NATed ifaces");
        return false;
    }

    // step 7 - enable the 'route localnet' config to re-route dns requests to
    // localhost outside the container
    if (!BridgeInterface::setIfaceRouteLocalNet(utils, netlink, true))
    {
        AI_LOG_ERROR("failed to enable localnet routing, dns may not work");
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Set ip address to a container and register the veth name to it
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils.
 *  @param[in]  dobbyProxy      Instance of DobbyRdkPluginProxy.
 *  @param[in]  rootfsPath      Path to the rootfs on the host.
 *  @param[in]  vethName        Name of the virtual ethernet device
 *
 *  @return ip address that was registered
 */
const std::string setContainerAddress(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                      const std::shared_ptr<DobbyRdkPluginProxy> &dobbyProxy,
                                      const std::string &rootfsPath,
                                      const std::string &vethName)
{
    AI_LOG_FN_ENTRY();

    // get ip address from daemon
    const in_addr_t ipAddress = dobbyProxy->getIpAddress(vethName);
    if (!ipAddress)
    {
        AI_LOG_ERROR_EXIT("failed to get ip address from daemon");
        return std::string();
    }

    // convert address to string
    char addressStr[INET_ADDRSTRLEN];
    struct in_addr ipAddress_ = { htonl(ipAddress) };
    inet_ntop(AF_INET, &ipAddress_, addressStr, INET_ADDRSTRLEN);

    // combine ip address and veth name strings
    const std::string fileContent(std::string() + addressStr + "/" + vethName);

    // write address and veth name to a file in the container rootfs
    const std::string filePath(rootfsPath + ADDRESS_FILE_PATH);
    if (!utils->writeTextFile(filePath, fileContent, O_CREAT | O_TRUNC, 0644))
    {
        AI_LOG_ERROR_EXIT("failed to write ip address file");
        return std::string();
    }

    AI_LOG_FN_EXIT();
    return std::string(addressStr);
}


// -----------------------------------------------------------------------------
/**
 *  @brief Called from within the network namespace of the container
 *
 *  This function does the following:
 *      - sets the link address of both lo and eth0 interfaces
 *      - sets the default routes for both the lo and eth0 interfaces
 *      - brings both interfaces up
 *
 *  @param[in]  address         The IPv4 address to give to the interface inside
 *                              the container.
 *
 *  @return true if successful, otherwise false
 */
bool setupContainerNet(in_addr_t address)
{
    AI_LOG_FN_ENTRY();

    // step 1 - create a new netlink object
    std::shared_ptr<Netlink> netlink = std::make_shared<Netlink>();
    if (!netlink->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create netlink object inside the container");
        return false;
    }

    // step 2 - set the address of the ifaceName interface inside the container
    // nb: htonl used for address to convert to network byte order
    const std::string ifaceName(PEER_NAME);
    if (!netlink->setIfaceAddress(ifaceName, htonl(address), INADDR_BRIDGE_NETMASK))
    {
        AI_LOG_ERROR_EXIT("failed to set the address and netmask of '%s'",
                          ifaceName.c_str());
        return false;
    }

    // step 3 - set the address of the 'lo' interface inside the container
    const std::string loName("lo");
    const in_addr_t loAddress = INADDR_CREATE(127, 0, 0, 1);
    const in_addr_t loNetmask = INADDR_CREATE(255, 0, 0, 0);
    if (!netlink->setIfaceAddress(loName, loAddress, loNetmask))
    {
        AI_LOG_ERROR_EXIT("failed to set the address and netmask of 'lo'");
        return false;
    }

    // step 4 - bring both interfaces up
    if (!netlink->ifaceUp(ifaceName) || !netlink->ifaceUp(loName))
    {
        AI_LOG_ERROR_EXIT("failed to bring up container interfaces");
        return false;
    }

    // step 5 - and the final step is to set the route table for the container
    const struct Route {
        const std::string& iface;
        in_addr_t dest;
        in_addr_t gateway;
        in_addr_t mask;
    } routes[] = {
        //  iface       destination                 gateway                     mask
        {   ifaceName,  INADDR_CREATE(0, 0, 0, 0),  INADDR_BRIDGE,              INADDR_CREATE(0, 0, 0, 0)   },
        {   loName,     (loAddress & loNetmask),    INADDR_CREATE(0, 0, 0, 0),  loNetmask                   },
    };

    // step 6 - set the routes
    for (const struct Route &route : routes)
    {
        if (!netlink->addRoute(route.iface, route.dest, route.gateway, route.mask))
        {
            AI_LOG_ERROR_EXIT("failed to apply route");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Called from host namespace
 *
 *  This function does the following:
 *      - Creates a virtual ethernet interface for the container
 *      - Sets an ip address for the container
 *      - Brings up the veth[i] interface
 *      - Adds required iptables rules
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils.
 *  @param[in]  netfilter       Instance of Netfilter.
 *  @param[in]  dobbyProxy      Instance of DobbyRdkPluginProxy.
 *  @param[in]  rootfsPath      Path to the rootfs on the host.
 *  @param[in]  containerId     The id of the container.
 *  @param[in]  networkType     Container's network type.
 *
 *  @return true if successful, otherwise false
 */
bool NetworkSetup::setupVeth(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                             const std::shared_ptr<Netfilter> &netfilter,
                             const std::shared_ptr<DobbyRdkPluginProxy> &dobbyProxy,
                             const std::string &rootfsPath,
                             const std::string &containerId,
                             const NetworkType networkType)
{
    AI_LOG_FN_ENTRY();

    // step 1 - create a new netlink object
    std::shared_ptr<Netlink> netlink = std::make_shared<Netlink>();
    if (!netlink->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create netlink object");
        return false;
    }

    // step 2 - get container process pid
    pid_t containerPid = utils->getContainerPid(utils->getHookStdin());
    if (!containerPid)
    {
        AI_LOG_ERROR_EXIT("couldn't find container pid");
        return false;
    }

    // step 3 - create a veth pair for the container
    std::string vethName = netlink->createVeth(PEER_NAME, containerPid);
    if (vethName.empty())
    {
        AI_LOG_ERROR_EXIT("failed to create veth pair for container '%s'",
                          containerId.c_str());
        return false;
    }

    // step 4 - set ip address for container
    const std::string ipAddressStr = setContainerAddress(utils, dobbyProxy, rootfsPath, vethName);
    if (ipAddressStr.empty())
    {
        return false;
    }

    in_addr_t ipAddress;
    inet_pton(AF_INET, ipAddressStr.c_str(), &ipAddress);
    AI_LOG_DEBUG("ip address %s set for container %s", ipAddressStr.c_str(),
                 containerId.c_str());

    // step 5 - enable ip forwarding on the veth interface created outside
    // the container
    if (!netlink->setIfaceForwarding(vethName, true))
    {
        AI_LOG_ERROR_EXIT("failed to enable ip forwarding on %s for '%s'",
                          vethName.c_str(), containerId.c_str());
        return false;
    }

    // step 6 - attach the veth iface on the outside of the container to the
    // bridge interface
    if (!netlink->addIfaceToBridge(BRIDGE_NAME, vethName))
    {
        AI_LOG_ERROR_EXIT("failed to attach veth to bridge for container '%s'",
                          containerId.c_str());
        return false;
    }

    // step 7 - enter the network namespace of the container and set the
    // default routes
    if (!utils->callInNamespace(containerPid, CLONE_NEWNET,
                                &setupContainerNet, ipAddress))
    {
        AI_LOG_ERROR_EXIT("failed to setup routing for container '%s'",
                          containerId.c_str());
        return false;
    }

    // step 8 - bring the veth interface outside the container up
    if (!netlink->ifaceUp(vethName))
    {
        AI_LOG_ERROR_EXIT("failed to bring up veth interface");
        return false;
    }

    // step 9 - add an iptables rule to either drop anything that comes from the
    // veth if a private network is enabled, or to drop anything that doesn't
    // have the correct ip address
    Netfilter::RuleSet ruleSet;
    if (networkType == NetworkType::Nat)
    {
        ruleSet = createAntiSpoofRule(ipAddressStr, vethName);
    }
    else if (networkType == NetworkType::None)
    {
        ruleSet = createDropAllRule(vethName);
    }

    if (!netfilter->insertRules(ruleSet))
    {
        AI_LOG_ERROR("failed to add iptables rule to drop veth packets");
        // for now this is not fatal as some kernels lack the netfilter physdev
        // matcher plugin
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Remove iptables entries for the container's veth and bring the
 *  veth pair down.
 *
 *  @param[in]  netfilter       Instance of Netfilter.
 *  @param[in]  ipAddressStr    Container's ip address in string format.
 *  @param[in]  vethName        Name of the container's veth interface
 *  @param[in]  networkType     Container's network type.
 *
 *  @return true if successful, otherwise false
 */
bool NetworkSetup::removeVethPair(const std::shared_ptr<Netfilter> &netfilter,
                                  const std::string ipAddressStr,
                                  const std::string vethName,
                                  const NetworkType networkType)
{
    AI_LOG_FN_ENTRY();

    // create a new netlink object
    std::shared_ptr<Netlink> netlink = std::make_shared<Netlink>();
    if (!netlink->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create netlink object");
        return false;
    }

    // take down the veth interface
    netlink->ifaceDown(vethName);

    // construct rule for container to match in iptables for removal
    Netfilter::RuleSet ruleSet;
    if (networkType == NetworkType::Nat)
    {
        ruleSet = createAntiSpoofRule(ipAddressStr, vethName);
    }
    else if (networkType == NetworkType::None)
    {
        ruleSet = createDropAllRule(vethName);
    }

    // create a netfilter object
    if (!netfilter->deleteRules(ruleSet))
    {
        AI_LOG_ERROR_EXIT("failed to delete netfilter rules for container veth");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Clear out the iptables rules set for the bridge device and brings
 *  the interface down
 *
 *  @param[in]  netfilter       Instance of Netfilter.
 *  @param[in]  extIfaces       External interfaces on the device
 *
 *  @return true if successful, otherwise false
 */
bool NetworkSetup::removeBridgeDevice(const std::shared_ptr<Netfilter> &netfilter,
                                      const std::vector<std::string> &extIfaces)
{
    AI_LOG_FN_ENTRY();

    // delete network rules for bridge device
    std::vector<Netfilter::RuleSet> ruleSets = constructBridgeRules(netfilter, extIfaces);
    for (Netfilter::RuleSet &ruleSet : ruleSets)
    {
        if (!netfilter->deleteRules(ruleSet))
        {
            AI_LOG_ERROR_EXIT("failed to delete netfilter rules for bridge device");
            return false;
        }
    }

    // create a new netlink object
    std::shared_ptr<Netlink> netlink = std::make_shared<Netlink>();
    if (!netlink->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create netlink object");
        return false;
    }

    // bring the bridge device down and destroy it
    BridgeInterface::down(netlink);
    BridgeInterface::destroyBridge(netlink);

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Adds a mount to sysfs in the OCI config
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils.
 *  @param[in]  cfg             Pointer to bundle config struct
 */
void NetworkSetup::addSysfsMount(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                 const std::shared_ptr<rt_dobby_schema> &cfg)
{
    const std::string source = "sysfs";
    const std::string destination = "/sys";

    // iterate through the mounts to check that the mount doesn't already exist
    for (int i=0; i < cfg->mounts_len; i++)
    {
        if (!strcmp(cfg->mounts[i]->source, source.c_str()) && !strcmp(cfg->mounts[i]->destination, destination.c_str()))
        {
            AI_LOG_DEBUG("sysfs mount already exists in the config");
            return;
        }
    }

    // add the mount
    utils->addMount(cfg, source, destination, "sysfs",
                    { "nosuid", "noexec", "nodev", "ro" }
    );
}


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the 'network' namespace to the OCI config
 *
 *  @param[in]  cfg             Pointer to bundle config struct
 */
void NetworkSetup::addNetworkNamespace(const std::shared_ptr<rt_dobby_schema> &cfg)
{
    // check if container already has network namespace enabled
    for (int i = 0; i < cfg->linux->namespaces_len; i++)
    {
        if (strcmp(cfg->linux->namespaces[i]->type, "network") == 0)
        {
            return;
        }
    }

    // allocate memory for namespace entry
    rt_defs_linux_namespace_reference *netNs = (rt_defs_linux_namespace_reference*)calloc(1, sizeof(rt_defs_linux_namespace_reference));
    netNs->type = strdup("network");
    netNs->path = nullptr;

    // add 'network' namespace
    cfg->linux->namespaces_len++;
    cfg->linux->namespaces = (rt_defs_linux_namespace_reference**)realloc(cfg->linux->namespaces, sizeof(rt_defs_linux_namespace_reference*) * cfg->linux->namespaces_len);
    cfg->linux->namespaces[cfg->linux->namespaces_len-1] = netNs;
}
