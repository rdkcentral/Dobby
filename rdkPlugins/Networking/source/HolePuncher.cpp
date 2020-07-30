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

#include "HolePuncher.h"

#include <Logging.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the two iptables firewall rules to enable port forwarding.
 *
 *  The 'protocol' field can be omitted in which case TCP will be specified.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  holes               libocispec structs containing holes to punch.
 *  @param[in]  len                 Number of holes to punch.
 *
 *  @return true on success, false on failure.
 */
bool HolePuncher::punchHoles(const std::shared_ptr<Netfilter> &netfilter,
                             const std::shared_ptr<NetworkingHelper> &helper,
                             const std::string &containerId,
                             rt_defs_plugins_networking_data_holes_element **holes,
                             const size_t len)
{
    AI_LOG_FN_ENTRY();

    // check that we have an accepted protocol specified for each hole
    for (int i = 0; i < len; i++)
    {
        // accept empty protocol which is defaulted to tcp later on
        if (holes[i]->protocol == nullptr)
        {
            continue;
        }

        std::string prot = holes[i]->protocol;
        if (strcmp(prot, "tcp") == 0 || strcmp(prot, "udp") == 0)
        {
            continue;
        }
        else
        {
            AI_LOG_ERROR_EXIT("invalid protocol value '%s' for hole %d",
                              prot.c_str(), i);
            return false;
        }
    }


    // add IPv4 rules to iptables if needed
    if (helper->ipv4())
    {
        std::vector<Netfilter::RuleSet> ipv4Rules = constructRules(netfilter, helper, containerId, holes, len, AF_INET);

        // append constructed rules to iptables
        if (!netfilter->appendRules(ipv4Rules[0], AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to append holepunch rule in iptables");
            return false;
        }

        // insert constructed rules to iptables
        if (!netfilter->insertRules(ipv4Rules[1], AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to insert holepunch rule in iptables");
            return false;
        }
    }

    // add IPv6 rules to iptables if needed
    if (helper->ipv6())
    {
        std::vector<Netfilter::RuleSet> ipv6Rules = constructRules(netfilter, helper, containerId, holes, len, AF_INET6);

        // append constructed rules to ip6tables
        if (!netfilter->appendRules(ipv6Rules[0], AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to append holepunch rule in ip6tables");
            return false;
        }

        // insert constructed rules to ip6tables
        if (!netfilter->insertRules(ipv6Rules[1], AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to insert holepunch rule in ip6tables");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes holes assigned to the container from iptables/ip6tables.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  holes               libocispec structs containing holes to punch.
 *  @param[in]  len                 Number of holes to punch.
 *
 *  @return always returns true.
 */
bool HolePuncher::removeHoles(const std::shared_ptr<Netfilter> &netfilter,
                              const std::shared_ptr<NetworkingHelper> &helper,
                              const std::string &containerId,
                              rt_defs_plugins_networking_data_holes_element **holes,
                              const size_t len)
{
    AI_LOG_FN_ENTRY();

    // delete IPv4 rules to ip6tables if needed
    if (helper->ipv4())
    {
        std::vector<Netfilter::RuleSet> ipv4Rules = constructRules(netfilter, helper, containerId, holes, len, AF_INET);

        // delete constructed rulesets
        if (!netfilter->deleteRules(ipv4Rules[0], AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to delete holepunch rule");
            return false;
        }
        if (!netfilter->deleteRules(ipv4Rules[1], AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to delete holepunch rule");
            return false;
        }

    }

    // delete IPv6 rules to ip6tables if needed
    if (helper->ipv6())
    {
        std::vector<Netfilter::RuleSet> ipv6Rules = constructRules(netfilter, helper, containerId, holes, len, AF_INET6);

        // delete constructed rulesets
        if (!netfilter->deleteRules(ipv6Rules[0], AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to delete holepunch rule");
            return false;
        }
        if (!netfilter->deleteRules(ipv6Rules[1], AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to delete holepunch rule");
            return false;
        }

    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Construct all the Holepunch rules based on input in the bundle config.
 *
 *  The first rule sets up pre-routing so the incoming packets have their
 *  ip address and port number changed to match the container.
 *
 *      iptables -t nat -A PREROUTING ! -i <BRIDGE_NAME> -p <PROTOCOL>
 *               --dport <PORT_NUMBER> -j DNAT --to <CONTAINER_IP>:<PORT_NUMBER>
 *
 *  And the second rule enables forwarding to the bridge and then on into the
 *  container.
 *
 *      iptables -I FORWARD 1 ! -i <BRIDGE_NAME> -o <BRIDGE_NAME>
 *               --destination <CONTAINER_IP> -p <PROTOCOL> --dport <PORT_NUMBER>
 *               -j ACCEPT
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  holes               libocispec structs containing holes to punch.
 *  @param[in]  len                 Number of holes to punch.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return always returns true.
 */
std::vector<Netfilter::RuleSet> constructRules(const std::shared_ptr<Netfilter> &netfilter,
                                               const std::shared_ptr<NetworkingHelper> &helper,
                                               const std::string &containerId,
                                               rt_defs_plugins_networking_data_holes_element **holes,
                                               const size_t len,
                                               const int ipVersion)
{
    std::string address;
    if (ipVersion == AF_INET)
    {
        address = helper->ipv4AddrStr();
    }
    else if (ipVersion == AF_INET6)
    {
        address = helper->ipv6AddrStr();
    }
    else
    {
        AI_LOG_ERROR_EXIT("supported ip address families are AF_INET or AF_INET6");
        return std::vector<Netfilter::RuleSet>();
    }

    std::list<std::string> appendRules;
    std::list<std::string> insertRules;

    // construct rules for each hole
    for (int i = 0; i < len; i++)
    {
        std::string port = std::to_string(holes[i]->port);
        // default to tcp if no protocol is set
        std::string protocol;
        if (holes[i]->protocol)
        {
            protocol = holes[i]->protocol;
        }
        else
        {
            protocol = "tcp";
        }

        // construct prerouting rule to append to iptables
        const std::string preroutingRule =
            createPreroutingRule(containerId, protocol,
                                 address, port, ipVersion);
        appendRules.emplace_back(preroutingRule);

        // construct forwarding rule to insert to iptables
        std::string forwardingRule =
            createForwardingRule(containerId, protocol,
                                 address, port, ipVersion);
        insertRules.emplace_back(forwardingRule);
    }

    std::vector<Netfilter::RuleSet> ruleSets;
    Netfilter::RuleSet appendRuleSet = {{ Netfilter::TableType::Nat, appendRules }};
    ruleSets.emplace_back(appendRuleSet);
    Netfilter::RuleSet insertRuleSet = {{ Netfilter::TableType::Filter, insertRules }};
    ruleSets.emplace_back(insertRuleSet);

    return ruleSets;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the iptables rule for the PREROUTING chain.
 *
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  protocol    The string name of protocol for the hole punch.
 *  @param[in]  ipAddress   The ip address of the container as a string.
 *  @param[in]  portNumber  The port number to hole punch as a string.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createPreroutingRule(const std::string& id,
                                 const std::string& protocol,
                                 const std::string& ipAddress,
                                 const std::string& portNumber,
                                 const int ipVersion)
{
    char buf[256];

    std::string natRule("PREROUTING "
                        "! -i " BRIDGE_NAME " "
                        "-p %s "                            // protocol
                        "-m %s "                            // protocol
                        "--dport %s "                       // port number
                        "-m comment --comment %s "          // container id
                        "-j DNAT --to-destination %s:%s");  // target address

    // construct the rule
    if (ipVersion == AF_INET)
    {
        snprintf(buf, sizeof(buf), natRule.c_str(),
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str(),
                 ipAddress.c_str(), portNumber.c_str());
    }
    else if (ipVersion == AF_INET6)
    {
        snprintf(buf, sizeof(buf), natRule.c_str(),
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str(),
                 ipAddress.c_str(), portNumber.c_str());
    }
    else
    {
        return std::string();
    }

    return buf;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the iptables rule for the FORWARDING chain.
 *
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  protocol    The string name of protocol for the hole punch.
 *  @param[in]  ipAddress   The ip address of the container as a string.
 *  @param[in]  portNumber  The port number to hole punch as a string.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createForwardingRule(const std::string &id,
                                 const std::string &protocol,
                                 const std::string &ipAddress,
                                 const std::string &portNumber,
                                 const int ipVersion)
{
    char buf[256];

    std::string filterRule("FORWARD "
                           "-d %s/32 "                  // container ip address
                           "! -i " BRIDGE_NAME " "
                           "-o " BRIDGE_NAME " "
                           "-p %s "                     // protocol
                           "-m %s "                     // protocol
                           "--dport %s "                // port number
                           "-m comment --comment %s "   // container id
                           "-j ACCEPT");

    // construct the rule
    if (ipVersion == AF_INET)
    {

        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 ipAddress.c_str(),
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str());
    }
    else if (ipVersion == AF_INET6)
    {
        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 ipAddress.c_str(),
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str());
    }
    else
    {
        return std::string();
    }

    return buf;
}
