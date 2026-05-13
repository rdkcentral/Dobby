/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
#include "TapInterface.h"
#include "NetworkingHelper.h"
#include "IPAllocator.h"
#include "Netfilter.h"

#include <Logging.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <dirent.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Modifies a rule set replacing all entries which have '%y' in them
 *  with actual address names.
 *
 *  @param[in]  ruleSet                 Iptables ruleset to expand.
 *  @param[in]  address                 IP address string to insert to rule.
 */
void expandRuleSetAddresses(Netfilter::RuleSet *ruleSet, const std::string &address)
{
    for (auto &table : *ruleSet)
    {
        std::list<std::string> &rules = table.second;

        auto it = rules.begin();
        while (it != rules.end())
        {
            std::string &rule = *it;
            bool replaceRule = false;

            // add address to template rule
            std::string newRule = rule;
            size_t pos = rule.find("%y");
            while (pos != std::string::npos)
            {
                // replace '%y' with address
                newRule.replace(pos, 2, address);
                replaceRule = true;

                // try and find '%y' again
                pos = newRule.find("%y", pos + 1);
                if (pos == std::string::npos)
                {
                    // no '%y' found, move on to next rule
                    break;
                }
            }

            if (replaceRule)
            {
                // add new rule and remove the template
                rules.emplace(it, std::move(newRule));
                it = rules.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}


// -----------------------------------------------------------------------------
/**
 *  @brief Modifies a rule set replacing all entries which have '%1' in them
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
    for (auto &table : *ruleSet)
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
 *  @param[in]  netfilter       Instance of Netfilter.
 *  @param[in]  extIfaces       External interfaces on the device.
 *  @param[in]  ipVersion       IP address family to use (AF_INET/AF_INET6).
 *
 *  @return returns a vector containing the constructed rules
 */
std::vector<Netfilter::RuleSet> constructBridgeRules(const std::shared_ptr<Netfilter> &netfilter,
                                                     const std::vector<std::string> &extIfaces,
                                                     const int ipVersion)
{
    // the following rulesets were obtained by looking at what libvirt had setup
    // for the NAT connection, we're just replicating
    // '%y' will be replaced by the dobby bridge address range
    // '%l' will be replaced with an external interface name
    Netfilter::RuleSet insertRuleSet =
    {
        {
            Netfilter::TableType::Filter,
            {
                "INPUT -i " BRIDGE_NAME " -j DobbyInputChain",
                "FORWARD -d %y -i %1 -o " BRIDGE_NAME " -m state --state INVALID -j DROP",
                "FORWARD -s %y -i " BRIDGE_NAME " -o %1 -m state --state INVALID -j DROP",
                "OUTPUT -s %y -o %1 -j DROP"
            }
        }
    };

    Netfilter::RuleSet appendRuleSet =
    {
        {
            Netfilter::TableType::Nat,
            {
                // setup NAT'ing on the BRIDGE_ADDRESS_RANGE ip range through external interface
                "POSTROUTING -s %y ! -d %y ! -o " BRIDGE_NAME " -p tcp -j MASQUERADE --to-ports 1024-65535",
                "POSTROUTING -s %y ! -d %y ! -o " BRIDGE_NAME " -p udp -j MASQUERADE --to-ports 1024-65535",
                "POSTROUTING -s %y ! -d %y ! -o " BRIDGE_NAME " -j MASQUERADE"
            }
        },
        {
            Netfilter::TableType::Filter,
            {
                // enable traffic between external interface and dobby bridge device
                "FORWARD -d %y -i %1 -o " BRIDGE_NAME " -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT",
                "FORWARD -s %y -i " BRIDGE_NAME " -o %1 -j ACCEPT",
                "FORWARD -i " BRIDGE_NAME " -o %1 -j ACCEPT",
            }
        }
    };

    if (ipVersion == AF_INET6)
    {
        // add DobbyInputChain rule to accept Network Discovery Protocol messages, otherwise
        // the Neigh table (which is equivalent of IPv4 ARP table) will not be able to update
        appendRuleSet[Netfilter::TableType::Filter].emplace_front("DobbyInputChain -p ICMPv6 -j ACCEPT");
    }

    // add addresses to rules depending on ipVersion
    std::string bridgeAddressRange;
    if (ipVersion == AF_INET)
    {
        // add POSTROUTING RETURN rules to the front of the NAT table
        appendRuleSet[Netfilter::TableType::Nat].emplace_front("POSTROUTING -s %y -d 255.255.255.255/32 ! -o " BRIDGE_NAME " -j RETURN");
        appendRuleSet[Netfilter::TableType::Nat].emplace_front("POSTROUTING -s %y -d 224.0.0.0/24 ! -o " BRIDGE_NAME " -j RETURN");

        // reject with "icmp-port-unreachable" if not ACCEPTed by now
        appendRuleSet[Netfilter::TableType::Filter].emplace_back("FORWARD -o " BRIDGE_NAME " -j REJECT --reject-with icmp-port-unreachable");
        appendRuleSet[Netfilter::TableType::Filter].emplace_back("FORWARD -i " BRIDGE_NAME " -j REJECT --reject-with icmp-port-unreachable");

        bridgeAddressRange = BRIDGE_ADDRESS_RANGE "/24";
    }
    else if (ipVersion == AF_INET6)
    {

        // add DobbyInputChain rule to accept solicited-node multicast requests from containers
        appendRuleSet[Netfilter::TableType::Filter].emplace_front("DobbyInputChain -s %y -d ff02::1:ff40:b01/128 -i " BRIDGE_NAME " -j ACCEPT");

        // reject with "icmp6-port-unreachable" if not ACCEPTed by now
        appendRuleSet[Netfilter::TableType::Filter].emplace_back("FORWARD -o " BRIDGE_NAME " -j REJECT --reject-with icmp6-port-unreachable");
        appendRuleSet[Netfilter::TableType::Filter].emplace_back("FORWARD -i " BRIDGE_NAME " -j REJECT --reject-with icmp6-port-unreachable");

        bridgeAddressRange = BRIDGE_ADDRESS_RANGE_IPV6 "/120";
    }
    else
    {
        AI_LOG_ERROR_EXIT("supported ip address families are AF_INET or AF_INET6");
        return std::vector<Netfilter::RuleSet>();
    }

    std::vector<Netfilter::RuleSet> ruleSets;

    // replace the %y in rulesets with Dobby bridge address range
    expandRuleSetAddresses(&insertRuleSet, bridgeAddressRange);
    // replace the %1 in the ruleset with the name of external interface(s)
    expandRuleSetForExtIfaces(&insertRuleSet, extIfaces);
    ruleSets.emplace_back(insertRuleSet);


    // replace the %y in rulesets with Dobby bridge address range
    expandRuleSetAddresses(&appendRuleSet, bridgeAddressRange);
    // replace the %1 in the ruleset with the name of external interface(s)
    expandRuleSetForExtIfaces(&appendRuleSet, extIfaces);
    ruleSets.emplace_back(appendRuleSet);

    return ruleSets;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Creates a netfilter rule to drop packets received from the veth into
 *  the bridge if they don't have the correct source IP address.
 *
 *  @param[in]  vethName        The name of the veth of the container.
 *  @param[in]  address         IPv4/6 address of the container.
 *  @param[in]  ipVersion       IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created ruleset
 */
Netfilter::RuleSet createAntiSpoofRule(const std::string &vethName,
                                       const std::string &address,
                                       const int ipVersion)
{
    std::list<std::string> antiSpoofRules;
    char buf[128];

    std::string filterRule("DobbyInputChain "
                           "! -s %s/%s "                    // ipAddress / mask
                           "-i %s "                         // bridge name
                           "-m physdev --physdev-in %s "    // veth name
                           "-j DROP");

    // construct the rule
    if (ipVersion == AF_INET)
    {
        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 address.c_str(), "32",
                 BRIDGE_NAME,
                 vethName.c_str());
        antiSpoofRules.emplace_back(buf);
    }
    else if (ipVersion == AF_INET6)
    {
        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 address.c_str(), "128",
                 BRIDGE_NAME,
                 vethName.c_str());
        antiSpoofRules.emplace_back(buf);
    }
    else
    {
        return Netfilter::RuleSet();
    }

    // return the rule in the default filter table
    return Netfilter::RuleSet {{ Netfilter::TableType::Filter, antiSpoofRules }};
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
    std::list<std::string> rules;

    // construct the rule
    char filterRule[128];
    snprintf(filterRule, sizeof(filterRule),
             "DobbyInputChain -i " BRIDGE_NAME " -m physdev --physdev-in %s -j DROP",
             vethName.c_str());

    rules.emplace_back(std::string(filterRule));
    memset(filterRule, 0, sizeof(filterRule));

    snprintf(filterRule, sizeof(filterRule),
             "FORWARD -i " BRIDGE_NAME " -m physdev --physdev-in %s -j REJECT",
             vethName.c_str());
    rules.emplace_back(std::string(filterRule));
    memset(filterRule, 0, sizeof(filterRule));

    // return the rule in the default filter table
    return Netfilter::RuleSet {{ Netfilter::TableType::Filter, rules}};
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
        AI_LOG_ERROR_EXIT("failed to disable STP");
        return false;
    }

    // step 3 - assign an IPv4 and IPv6 address to the bridge
    if (!BridgeInterface::setAddresses(netlink))
    {
        AI_LOG_ERROR_EXIT("failed to set the ip addresses on the bridge interface");
        return false;
    }

    // create an (unused) tap device and attach to the bridge, this is purely
    // to stop the bridge MAC address from changing as we add / remove veths
    // @see https://backreference.org/2010/07/28/linux-bridge-mac-addresses-and-dynamic-ports/
    if (TapInterface::platformSupportsTapInterface())
    {
        if (!TapInterface::createTapInterface(netlink) || !TapInterface::isValid())
        {
            AI_LOG_ERROR("failed to create tap device");
        }
        else if (!BridgeInterface::attachLink(netlink, TapInterface::name()))
        {
            AI_LOG_ERROR("failed to attach '%s' to the bridge",
                        TapInterface::name().c_str());
        }
        else if (!BridgeInterface::setMACAddress(netlink, TapInterface::macAddress(netlink)))
        {
            AI_LOG_ERROR("failed to set bridge MAC address");
        }
    }
    else
    {
        AI_LOG_WARN("Platform does not support tap devices, skipping creating %s", TapInterface::name().c_str());
    }

    // step 4 - construct the IPv4 rules to be added to iptables and then add them
    // start with creating a chain to filter input into the bridge device
    netfilter->createNewChain(Netfilter::TableType::Filter, "DobbyInputChain", AF_INET);
    std::vector<Netfilter::RuleSet> ipv4RuleSets = constructBridgeRules(netfilter, extIfaces, AF_INET);
    if (ipv4RuleSets.empty())
    {
        AI_LOG_ERROR_EXIT("failed to setup device bridge due to empty IPv4 ruleset");
        return false;
    }

    // set the iptables drop rules for the NAT
    if (!netfilter->addRules(ipv4RuleSets[0], AF_INET, Netfilter::Operation::Insert))
    {
        AI_LOG_ERROR_EXIT("failed to setup iptables drop rules for NAT");
        return false;
    }

    // set the iptables rules for the NAT
    if (!netfilter->addRules(ipv4RuleSets[1], AF_INET, Netfilter::Operation::Append))
    {
        AI_LOG_ERROR_EXIT("failed to setup iptables forwarding rules for NAT");
        return false;
    }

    // step 5 - construct the IPv6 rules to be added to iptables and then add them
    // start with creating a chain to filter input into the bridge device
    netfilter->createNewChain(Netfilter::TableType::Filter, "DobbyInputChain", AF_INET6);
    std::vector<Netfilter::RuleSet> ipv6RuleSets = constructBridgeRules(netfilter, extIfaces, AF_INET6);
    if (ipv6RuleSets.empty())
    {
        AI_LOG_ERROR_EXIT("failed to setup device bridge due to empty IPv6 ruleset");
        return false;
    }

    // set the ip6tables drop rules for the NAT
    if (!netfilter->addRules(ipv6RuleSets[0], AF_INET6, Netfilter::Operation::Insert))
    {
        AI_LOG_ERROR_EXIT("failed to setup iptables drop rules for NAT");
        return false;
    }

    // set the ip6tables rules for the NAT
    if (!netfilter->addRules(ipv6RuleSets[1], AF_INET6, Netfilter::Operation::Append))
    {
        AI_LOG_ERROR_EXIT("failed to setup iptables forwarding rules for NAT");
        return false;
    }

    // step 6 - bring the bridge interface up
    if (!BridgeInterface::up(netlink))
    {
        AI_LOG_ERROR_EXIT("failed to bring the bridge interface up");
        return false;
    }

    // step 7 - enable IPv6 forwarding on all devices for device-specific
    // forwarding enables to work. IPv4 usually has this enabled by default.
    if (!netlink->setIfaceForwarding6(utils, "all", true))
    {
        AI_LOG_ERROR_EXIT("failed to enable IPv6 forwarding on all interfaces");
        return false;
    }

    // step 8 - enable ip forwarding on our local NAT bridge and external ifaces
    for (const std::string &extIface : extIfaces)
    {
        // set up IPv4 forwarding on interface
        if (!netlink->setIfaceForwarding(extIface, true))
        {
            AI_LOG_ERROR_EXIT("failed to enable IPv4 forwarding on interface"
                              "'%s'", extIface.c_str());
            return false;
        }

        // set up IPv6 forwarding on interface
        if (!netlink->setIfaceForwarding6(utils, extIface, true))
        {
            AI_LOG_ERROR_EXIT("failed to enable IPv6 forwarding on interface"
                              "'%s'", extIface.c_str());
            return false;
        }

        // enable IPv6 router advertisements even when forwarding is enabled
        if (!netlink->setIfaceAcceptRa(utils, extIface, 2))
        {
            AI_LOG_ERROR_EXIT("failed to enable accept_ra on interface %s",
                              extIface.c_str());
            return false;
        }
    }

    // set up forwarding on the bridge interface
    if (!BridgeInterface::setIfaceForwarding(utils, netlink, true))
    {
        AI_LOG_ERROR_EXIT("failed to enable forwarding on the NATed ifaces");
        return false;
    }

    // accept router advertisements on the bridge even with forwarding enabled
    if (!BridgeInterface::setIfaceAcceptRa(utils, netlink, 2))
    {
        AI_LOG_ERROR_EXIT("failed to enable accept_ra on the bridge device");
        return false;
    }

    // step 9 - enable the 'route localnet' config to re-route dns requests to
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
 *  @brief Saves an ip address to a container and register the veth name to it.
 *
 *  The ip address is stored in the NetworkingHelper object provided in args.
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils.
 *  @param[in]  helper          Instance of NetworkingHelper.
 *  @param[in]  rootfsPath      Path to the rootfs on the host.
 *  @param[in]  vethName        Name of the virtual ethernet device
 *
 *  @return true if successful, otherwise false.
 */
bool saveContainerAddress(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                          const std::shared_ptr<NetworkingHelper> &helper,
                          const std::string &rootfsPath,
                          const std::string &vethName)
{
    AI_LOG_FN_ENTRY();

    // Allocate an IP for the container
    IPAllocator ipAllocator(utils);
    const in_addr_t ipAddress = ipAllocator.allocateIpAddress(vethName);
    if (!ipAddress)
    {
        AI_LOG_ERROR_EXIT("failed to get ip address");
        return false;
    }

    // store addresses and veth name in helper
    if (!helper->storeContainerInterface(ipAddress, vethName))
    {
        AI_LOG_ERROR_EXIT("failed to set ip addresses");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
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
 *  @param[in]  helper          Instance of NetworkingHelper.
 *
 *  @return true if successful, otherwise false
 */
bool setupContainerNet(const std::shared_ptr<NetworkingHelper> &helper)
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

    // first add IPv4 address if enabled
    const std::string ifaceName(PEER_NAME);
    if (helper->ipv4())
    {
        if (!netlink->setIfaceAddress(ifaceName, helper->ipv4Addr(), INADDR_BRIDGE_NETMASK))
        {
            AI_LOG_ERROR_EXIT("failed to set the IPv4 address and netmask of '%s'",
                              ifaceName.c_str());
            return false;
        }
    }

    // add IPv6 address if enabled, create address by merging IPv4 address into
    // a base address
    if (helper->ipv6())
    {
        if (!netlink->setIfaceAddress(ifaceName, helper->ipv6Addr(), 64))
        {
            AI_LOG_ERROR_EXIT("failed to set the IPv6 address and netmask of '%s'",
                              ifaceName.c_str());
            return false;
        }
    }

    // step 3 - set the address of the 'lo' interface inside the container
    const std::string loName("lo");
    if (!netlink->setIfaceAddress(loName, INADDR_LO, INADDR_LO_NETMASK))
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

    // step 5 - and the final step is to set the route table(s) for the container
    if (helper->ipv4())
    {
        const struct ipv4Route {
            const std::string& iface;
            in_addr_t dest;
            in_addr_t mask;
            in_addr_t gateway;
        } ipv4Routes[] = {
            //  iface       destination                         mask                        gateway
            {   ifaceName,  INADDR_CREATE(0, 0, 0, 0),          INADDR_CREATE(0, 0, 0, 0),  INADDR_BRIDGE               },
            {   loName,     (INADDR_LO & INADDR_LO_NETMASK),    INADDR_LO_NETMASK,          INADDR_CREATE(0, 0, 0, 0)   },
        };

        // set the routes
        for (auto &route : ipv4Routes)
        {
            if (!netlink->addRoute(route.iface, route.dest, route.mask, route.gateway))
            {
                AI_LOG_ERROR_EXIT("failed to apply route");
                return false;
            }
        }
    }

    if (helper->ipv6())
    {
        //construct lo address (::1)
        //struct in6_addr loAddr = IN6ADDR_ANY;
        //loAddr.s6_addr[15] = 1;

        const struct ipv6Route {
            const std::string& iface;
            struct in6_addr dest;
            int mask;
            struct in6_addr gateway;
        } ipv6Routes[] = {
            //  iface       destination     mask    gateway
            {   ifaceName,  IN6ADDR_ANY,    0,      NetworkingHelper::in6addrCreate(INADDR_BRIDGE)  }
        };

        // set the routes
        for (auto &route : ipv6Routes)
        {
            if (!netlink->addRoute(route.iface, route.dest, route.mask, route.gateway))
            {
                AI_LOG_ERROR_EXIT("failed to apply route");
                return false;
            }
        }
    }

    // get the mac address of the eth0 virtual interface, this
    // may be used to update the ARP table on the bridge
    helper->storeContainerVethPeerMac(netlink->getIfaceMAC(PEER_NAME));

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
 *  @param[in]  helper          Instance of NetworkingHelper.
 *  @param[in]  rootfsPath      Path to the rootfs on the host.
 *  @param[in]  containerId     The id of the container.
 *  @param[in]  networkType     Network type.
 *
 *  @return true if successful, otherwise false
 */
bool NetworkSetup::setupVeth(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                             const std::shared_ptr<Netfilter> &netfilter,
                             const std::shared_ptr<NetworkingHelper> &helper,
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
    pid_t containerPid = utils->getContainerPid();
    if (!containerPid)
    {
        AI_LOG_ERROR_EXIT("couldn't find container pid");
        return false;
    }

    // step 3 - create a veth pair for the container, using the name of the
    // first external interface defined in Dobby settings
    std::vector<std::string> takenVeths;
    utils->getTakenVeths(takenVeths);

    std::string vethName = netlink->createVeth(PEER_NAME, containerPid, takenVeths);
    if (vethName.empty())
    {
        AI_LOG_ERROR_EXIT("failed to create veth pair for container '%s'",
                          containerId.c_str());
        return false;
    }

    // step 4 - get and save the ip address for container
    if (!saveContainerAddress(utils, helper, rootfsPath, vethName))
    {
        AI_LOG_ERROR_EXIT("failed to get address for container '%s'",
                          containerId.c_str());
        return false;
    }

    // step 5 - enable ip forwarding on the veth interface created outside
    // the container
    if (!netlink->setIfaceForwarding(vethName, true))
    {
        AI_LOG_ERROR_EXIT("failed to enable IPv4 forwarding on %s for '%s'",
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

    // step 7 - enable IPv6 forwarding on veth interface if using IPv6
    if (helper->ipv6())
    {
        // enable IPv6 forwarding
        if (!netlink->setIfaceForwarding6(utils, vethName, true))
        {
            AI_LOG_ERROR_EXIT("failed to enable IPv6 forwarding on %s for '%s'",
                              vethName.c_str(), containerId.c_str());
            return false;
        }

        // accept router advertisements even with forwarding enabled
        if (!netlink->setIfaceAcceptRa(utils, vethName, 2))
        {
            AI_LOG_ERROR_EXIT("failed to enable accept_ra on %s for '%s'",
                              vethName.c_str(), containerId.c_str());
            return false;
        }
    }

    // step 8 - enter the network namespace of the container and set the
    // default routes
    if (!utils->callInNamespace(containerPid, CLONE_NEWNET,
                                &setupContainerNet, helper))
    {
        AI_LOG_ERROR_EXIT("failed to setup routing for container '%s'",
                          containerId.c_str());
        return false;
    }

    // step 9 - add routing table entry to the container
    // This shouldn't be needed as there is already existing rule for the
    // bridge itself with lower metric (higher priority) which looks like:
    // 2080:d0bb:1e::/64 dev dobby0  metric 256 
    // So this one will take precedence, and will be valid for all
    // containers
    /*
    if (helper->ipv4())
    {
        if (!netlink->addRoute(BRIDGE_NAME, helper->ipv4Addr(), INADDR_CREATE(255, 255, 255, 255), INADDR_CREATE(0, 0, 0, 0)))
        {
            AI_LOG_ERROR_EXIT("failed to apply route");
            return false;
        }
    }
    if (helper->ipv6())
    {
        if (!netlink->addRoute(BRIDGE_NAME, helper->ipv6Addr(), 128, IN6ADDR_ANY))
        {
            AI_LOG_ERROR_EXIT("failed to apply route");
            return false;
        }
    }
    */

    // step 10 - bring the veth interface outside the container up
    if (!netlink->ifaceUp(vethName))
    {
        AI_LOG_ERROR_EXIT("failed to bring up veth interface");
        return false;
    }

    // step 11 - preload the ARP rules for routing packets to the container's veth
    if (helper->ipv4())
    {
        const std::array<uint8_t, 6> peerMac = helper->vethPeerMac();
        if (!netlink->addArpEntry(BRIDGE_NAME, helper->ipv4Addr(), peerMac))
        {
            AI_LOG_WARN("failed to add arp rule for %s for container %s",
                        vethName.c_str(), containerId.c_str());
        }
    }

    Netfilter::Operation operation = Netfilter::Operation::Append;

    // step 12 - add an iptables rule to either drop anything that comes from the
    // veth if a private network is enabled, or to drop anything that doesn't
    // have the correct ip address
    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4RuleSet;
        if (networkType == NetworkType::Nat)
        {
            ipv4RuleSet = createAntiSpoofRule(vethName, helper->ipv4AddrStr(), AF_INET);
            operation = Netfilter::Operation::Append;
        }
        else if (networkType == NetworkType::None)
        {
            ipv4RuleSet = createDropAllRule(vethName);
            operation = Netfilter::Operation::Insert;
        }

        if (!netfilter->addRules(ipv4RuleSet, AF_INET, operation))
        {
            AI_LOG_ERROR_EXIT("failed to add iptables rule to drop veth packets");
            return false;
        }
    }
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6RuleSet;
        if (networkType == NetworkType::Nat)
        {
            ipv6RuleSet = createAntiSpoofRule(vethName, helper->ipv6AddrStr(), AF_INET6);
            operation = Netfilter::Operation::Append;
        }
        else if (networkType == NetworkType::None)
        {
            ipv6RuleSet = createDropAllRule(vethName);
            operation = Netfilter::Operation::Insert;
        }

        if (!netfilter->addRules(ipv6RuleSet, AF_INET6, operation))
        {
            AI_LOG_ERROR_EXIT("failed to add iptables rule to drop veth packets");
            return false;
        }
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
 *  @param[in]  helper          Instance of NetworkingHelper.
 *  @param[in]  vethName        Name of the container's veth interface.
 *  @param[in]  networkType     Container's network type.
 *
 *  @return true if successful, otherwise false
 */
bool NetworkSetup::removeVethPair(const std::shared_ptr<Netfilter> &netfilter,
                                  const std::shared_ptr<NetworkingHelper> &helper,
                                  const std::string &vethName,
                                  const NetworkType networkType,
                                  const std::string &containerId)
{
    AI_LOG_FN_ENTRY();

    bool success = true;

    // create a new netlink object
    std::shared_ptr<Netlink> netlink = std::make_shared<Netlink>();
    if (!netlink->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create netlink object");
        return false;
    }

    // take down the veth interface
    netlink->ifaceDown(vethName);

    // construct iptables rules and remove them
    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4RuleSet;
        if (networkType == NetworkType::Nat)
        {
            ipv4RuleSet = createAntiSpoofRule(vethName, helper->ipv4AddrStr(), AF_INET);
        }
        else if (networkType == NetworkType::None)
        {
            ipv4RuleSet = createDropAllRule(vethName);
        }

        if (!netfilter->addRules(ipv4RuleSet, AF_INET, Netfilter::Operation::Delete))
        {
            AI_LOG_ERROR("failed to delete netfilter rules for container veth");
            success = false;
        }
    }

    // construct ip6tables rules and remove them
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6RuleSet;
        if (networkType == NetworkType::Nat)
        {
            ipv6RuleSet = createAntiSpoofRule(vethName, helper->ipv6AddrStr(), AF_INET6);
        }
        else if (networkType == NetworkType::None)
        {
            ipv6RuleSet = createDropAllRule(vethName);
        }

        if (!netfilter->addRules(ipv6RuleSet, AF_INET6, Netfilter::Operation::Delete))
        {
            AI_LOG_ERROR("failed to delete netfilter rules for container veth");
            success = false;
        }
    }

    // remove the container ip address from the ARP table in the bridge
    if (helper->ipv4())
    {
        netlink->delArpEntry(BRIDGE_NAME, helper->ipv4Addr());
    }

    // delete veth from bridge if it's still up. No error checking needed
    // because failing to get the interface means that it's already deleted.
    netlink->delIfaceFromBridge(BRIDGE_NAME, vethName);

    AI_LOG_FN_EXIT();
    return success;
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

    bool success = true;

    // delete IPv4 network rules for bridge device
    std::vector<Netfilter::RuleSet> ipv4RuleSets = constructBridgeRules(netfilter, extIfaces, AF_INET);
    if (ipv4RuleSets.empty())
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    for (Netfilter::RuleSet &ipv4RuleSet : ipv4RuleSets)
    {
        if (!netfilter->addRules(ipv4RuleSet, AF_INET, Netfilter::Operation::Delete))
        {
            AI_LOG_ERROR("failed to delete netfilter rules for bridge device");
            success = false;
        }
    }

    // delete IPv6 network rules for bridge device
    std::vector<Netfilter::RuleSet> ipv6RuleSets = constructBridgeRules(netfilter, extIfaces, AF_INET6);
    if (ipv6RuleSets.empty())
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    for (Netfilter::RuleSet &ipv6RuleSet : ipv6RuleSets)
    {
        if (!netfilter->addRules(ipv6RuleSet, AF_INET6, Netfilter::Operation::Delete))
        {
            AI_LOG_ERROR("failed to delete netfilter rules for bridge device");
            success = false;
        }
    }

    // create a new netlink object
    std::shared_ptr<Netlink> netlink = std::make_shared<Netlink>();
    if (!netlink->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create netlink object");
        return false;
    }

    // Close tap interface
    // If issue around container losing network connectivity when veths are added
    // and removed from the bridge will reocur it will mean that we should hold
    // Tap device even when we destroy bridge, but then it leave the question
    // where should we delete it. For now we destory it here.
    if (TapInterface::platformSupportsTapInterface())
    {
        TapInterface::destroyTapInterface(netlink);
    }

    // bring the bridge device down and destroy it
    BridgeInterface::down(netlink);
    BridgeInterface::destroyBridge(netlink);

    AI_LOG_FN_EXIT();
    return success;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Adds a mount to /etc/resolv.conf
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils.
 *  @param[in]  cfg             Pointer to bundle config struct.
 */
void NetworkSetup::addResolvMount(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                  const std::shared_ptr<rt_dobby_schema> &cfg)
{
    const std::string source = "/etc/resolv.conf";
    const std::string destination = "/etc/resolv.conf";

    // iterate through the mounts to check that the mount doesn't already exist
    for (size_t i=0; i < cfg->mounts_len; i++)
    {
        if (!strcmp(cfg->mounts[i]->source, source.c_str()) &&
            !strcmp(cfg->mounts[i]->destination, destination.c_str()))
        {
            AI_LOG_DEBUG("/etc/resolv.conf mount already exists in the config");
            return;
        }
    }

    // add the mount
    utils->addMount(source, destination, "bind",
                    { "ro", "rbind", "rprivate", "nosuid", "noexec", "nodev", }
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
    for (size_t i = 0; i < cfg->linux->namespaces_len; i++)
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