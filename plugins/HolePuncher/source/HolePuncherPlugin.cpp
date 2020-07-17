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
/*
 * File:   HolePuncherPlugin.cpp
 *
 * Copyright (C) Sky UK 2018+
 */
#include "HolePuncherPlugin.h"

#include <Logging.h>

#include <sstream>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <cstring>

// -----------------------------------------------------------------------------
/**
 *  @brief Registers the HolePuncherPlugin plugin object.
 *
 *  The object is constructed at the start of the Dobby daemon and only
 *  destructed when the Dobby daemon is shutting down.
 *
 */
REGISTER_DOBBY_PLUGIN(HolePuncherPlugin);



HolePuncherPlugin::HolePuncherPlugin(const std::shared_ptr<IDobbyEnv>& env,
                                     const std::shared_ptr<IDobbyUtils>& utils)
    : mName("HolePuncher")
    , mUtilities(utils)
{
    AI_LOG_FN_ENTRY();
    
    AI_LOG_FN_EXIT();
}

HolePuncherPlugin::~HolePuncherPlugin()
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
std::string HolePuncherPlugin::name() const
{
    return mName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Indiciates which hook points we want and whether to run the
 *  asynchronously or synchronously with the other hooks
 *
 *  For HolePuncher everything is done in the preStart and postStop phases.
 */
unsigned HolePuncherPlugin::hookHints() const
{
    return (IDobbyPlugin::PreStartAsync |
            IDobbyPlugin::PostStopAsync);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the two iptables firewall rules to enable port forwarding.
 *
 *  The json data is expected (required) to be formatted like the following
 *
 *      {
 *          "holes": [
 *              {
 *                  "port": 1234,
 *                  "protocol": "tcp"
 *              },
 *              {
 *                  "port": 5678,
 *                  "protocol": "udp"
 *              }
 *            ]
 *        }
 *
 *  The 'protocol' field can be omitted in which case TCP will be specified.
 *
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  pid         The pid of the processes within the container.
 *  @param[in]  rootfsPath  The absolute path to the rootfs of the container.
 *  @param[in]  jsonData    The parsed json data from the container spec file.
 *
 *  @return true on success, false on failure.
 */
bool HolePuncherPlugin::preStart(const ContainerId& id,
                                 pid_t pid,
                                 const std::string& rootfsPath,
                                 const Json::Value& jsonData)
{
    AI_LOG_FN_ENTRY();

    std::string appId(id.str());

    // validate / read the json
    const Json::Value& holes = jsonData["holes"];
    if (!holes.isArray() || holes.empty())
    {
        AI_LOG_ERROR_EXIT("'holes' field is not an array or it's empty");
        return false;
    }

    std::list<HolePunch> holePunches;

    Json::Value::const_iterator it = holes.begin();
    for (; it != holes.end(); ++it)
    {
        const Json::Value& hole = *it;
        if (!hole.isObject())
        {
            AI_LOG_ERROR("invalid 'hole' entry at index %u", it.index());
            continue;
        }

        HolePunch holePunch;
        bzero(&holePunch, sizeof(holePunch));

        const Json::Value port = hole["port"];
        const Json::Value protocol = hole["protocol"];

        if (!port.isIntegral())
        {
            AI_LOG_ERROR("invalid 'hole.port' entry at index %u", it.index());
            continue;
        }

        holePunch.portNumber = static_cast<in_port_t>(port.asInt());
        holePunch.protocol = SOCK_STREAM;

        // protocol is optional, if omitted then use tcp
        if (protocol.isString())
        {
            if (strcasecmp(protocol.asCString(), "udp") == 0)
            {
                holePunch.protocol = SOCK_DGRAM;
            }
            else if (strcasecmp(protocol.asCString(), "tcp") != 0)
            {
                AI_LOG_ERROR("invalid 'hole.protocol' entry at index %u", it.index());
                continue;
            }
        }
        else if (!protocol.isNull())
        {
            AI_LOG_ERROR("invalid type for 'hole.protocol' entry at index %u", it.index());
            continue;
        }

        holePunches.push_back(holePunch);
    }


    // check if we have any holes to punch and if so we need to get the IP
    // address of the container
    if (holePunches.empty())
    {
        AI_LOG_WARN("no holes need punching?");
        AI_LOG_FN_EXIT();
        return true;
    }

    // FIXME: this is a bit inefficient, we know the IP address because we
    // set it in the NetworkHook code, but currently we don't pass it to the
    // plugins, we should fix this.

    in_addr_t containerIp = INADDR_NONE;
    std::function<void()> fn = std::bind(&HolePuncherPlugin::getContainerIpAddress,
                                         this, &containerIp);

    if (!mUtilities->callInNamespace(pid, CLONE_NEWNET, fn))
    {
        AI_LOG_ERROR_EXIT("failed to invoke IP address getter in container");
        return false;
    }
    else if (containerIp == INADDR_NONE)
    {
        AI_LOG_WARN("container doesn't have a network address, do you have 'wan-lan' enabled?");
        AI_LOG_FN_EXIT();
        return true;
    }


    // assign the ip address in all the hole punchers and finally try to apply
    // them to the container / firewall

    // we either apply all the hole punches or none, so in case of any failure
    // we want to roll back any previous successes.
    std::list<HolePunch> addedHolePunches;

    for (HolePunch &holePunch : holePunches)
    {
        holePunch.ipAddress = containerIp;

        if (!addHolePunch(id, holePunch.protocol, holePunch.ipAddress,
                          holePunch.portNumber))
        {
            // failed so remove all previous successes
            for (HolePunch &addedHole : addedHolePunches)
            {
                removeHolePunch(id, addedHole.protocol, addedHole.ipAddress,
                                addedHole.portNumber);
            }

            AI_LOG_ERROR_EXIT("failed to add hole punch for container '%s' and port %hu",
                              id.c_str(), holePunch.portNumber);
            return false;
        }

        // add to the list of successiful entries
        addedHolePunches.push_back(holePunch);
    }


    // now finally store all the added holes for when the container is shutdown
    std::lock_guard<std::mutex> locker(mLock);

    for (const HolePunch &addedHole : addedHolePunches)
    {
        mHolePunchMap.insert(std::make_pair(id, addedHole));
    }

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
bool HolePuncherPlugin::postStop(const ContainerId& id,
                                 const std::string& rootfsPath,
                                 const Json::Value& jsonData)
{
    AI_LOG_FN_ENTRY();

    // take the lock and remove all the hole punches assigned to the terminated
    // container
    std::lock_guard<std::mutex> locker(mLock);

    std::pair< std::multimap<ContainerId, HolePunch>::const_iterator,
               std::multimap<ContainerId, HolePunch>::const_iterator > holes = mHolePunchMap.equal_range(id);

    std::multimap<ContainerId, HolePunch>::const_iterator it = holes.first;
    for (; it != holes.second; ++it)
    {
        removeHolePunch(id, it->second.protocol,
                            it->second.ipAddress,
                            it->second.portNumber);
    }

    // remove all the holes from the internal map
    mHolePunchMap.erase(holes.first, holes.second);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the iptables add or delete arguments for the PREROUTING
 *  table
 *
 *
 *  @param[in]  id          The id of the container making the request
 *  @param[in]  protocol    The string name of protocol for the hole punch.
 *  @param[in]  ipAddress   The ip address of the container as a string.
 *  @param[in]  portNumber  The port number to hole punch as a string.
 *
 *  @return a string list of all the args to supply to iptables.
 */
std::list<std::string> HolePuncherPlugin::constructPreRoutingRuleArgs(bool add,
                                                                      const ContainerId& id,
                                                                      const std::string& protocol,
                                                                      const std::string& ipAddress,
                                                                      const std::string& portNumber) const
{
    // construct the target address string
    char targetStr[INET_ADDRSTRLEN + 32];
    snprintf(targetStr, sizeof(targetStr), "%s:%s",
             ipAddress.c_str(), portNumber.c_str());

    // construct the args
    std::list<std::string> ipTablesArgs =
    {
        "-t", "nat",
        add ? "-A" : "-D", "PREROUTING",
        "!", "-i", "dobby0", "--source", "0.0.0.0/0", "--destination", "0.0.0.0/0", "-p", protocol, "--dport", portNumber,
        "-j", "DNAT", "--to", targetStr,
        "-m", "comment", "--comment", id.str()
    };

    return ipTablesArgs;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the iptables add or delete arguments for the
 *  FORWARDING table
 *
 *
 *  @param[in]  id          The id of the container making the request
 *  @param[in]  protocol    The string name of protocol for the hole punch.
 *  @param[in]  ipAddress   The ip address of the container as a string.
 *  @param[in]  portNumber  The port number to hole punch as a string.
 *
 *  @return a string list of all the args to supply to iptables.
 */
std::list<std::string> HolePuncherPlugin::constructForwardingRuleArgs(bool add,
                                                                      const ContainerId& id,
                                                                      const std::string& protocol,
                                                                      const std::string& ipAddress,
                                                                      const std::string& portNumber) const
{
    // construct the args
    std::list<std::string> ipTablesArgs =
    {
        "!", "-i", "dobby0", "-o", "dobby0", "--source", "0.0.0.0/0", "--destination", ipAddress, "-p", protocol, "--dport", portNumber,
        "-j", "ACCEPT",
        "-m", "comment", "--comment", id.str()
    };

    // if adding the rule prefix with `-I FORWARD 1` and if deleting `-D FORWARD`
    if (add)
    {
        ipTablesArgs.splice(ipTablesArgs.begin(), { "-I", "FORWARD", "1" });
    }
    else
    {
        ipTablesArgs.splice(ipTablesArgs.begin(), { "-D", "FORWARD" });
    }

    return ipTablesArgs;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to add the hole punch IP tables rules.
 *
 *  The first rule sets up pre-routing so the incomming packets have their
 *  ip address and port number changed to match the container.
 *  \code
 *      /usr/sbin/iptables -t nat -A PREROUTING
 *          ! -i dobby0 --source 0.0.0.0/0 --destination 0.0.0.0/0 -p <PROTOCOL> --dport <PORT_NUMBER>
 *          -j DNAT --to <CONTAINER_IP>:<PORT_NUMBER>
 *  \endcode
 *
 *  And the second rule enables forwarding from to the dobby0 bridge and then
 *  on into the container.
 *  \code
 *      /usr/sbin/iptables -I FORWARD 1
 *          ! -i dobby0 -o dobby0 --source 0.0.0.0/0 --destination <CONTAINER_IP> -p <PROTOCOL> --dport <PORT_NUMBER>
 *          -j ACCEPT
 *  \endcode
 *
 *
 *  @param[in]  id          The id of the container making the request
 *  @param[in]  protocol    The protocol of the hole punch (either SOCK_STREAM
 *                          or SOCK_DGRAM).
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  portNumber  The port number.
 *
 *  @return true on success, false on failure.
 */
bool HolePuncherPlugin::addHolePunch(const ContainerId& id, int protocol,
                                     in_addr_t ipAddress, in_port_t portNumber)
{
    AI_LOG_FN_ENTRY();

    // convert the args to string for the iptables command line
    char portStr[32];
    sprintf(portStr, "%hu", portNumber);

    char addressStr[INET_ADDRSTRLEN];
    struct in_addr ipAddress_ = { htonl(ipAddress) };
    inet_ntop(AF_INET, &ipAddress_, addressStr, INET_ADDRSTRLEN);

    std::string protocolStr;
    if (protocol == SOCK_STREAM)
    {
        protocolStr = "tcp";
    }
    else if (protocol == SOCK_DGRAM)
    {
        protocolStr = "udp";
    }
    else
    {
        AI_LOG_ERROR_EXIT("invalid protocol value");
        return false;
    }

    // construct the iptables args
    std::list<std::string> preroutingRule =
        constructPreRoutingRuleArgs(true, id, protocolStr, addressStr, portStr);

    std::list<std::string> forwardingRule =
        constructForwardingRuleArgs(true, id, protocolStr, addressStr, portStr);

    // attempt to add the prerouting rule
    if (!execIpTables(preroutingRule))
    {
        AI_LOG_ERROR_EXIT("failed to add PREROUTING rule");
        return false;
    }

    // attempt to add the forwarding rule, if this fails then we should remove
    // the previously added prerouting fule
    if (!execIpTables(forwardingRule))
    {
        AI_LOG_ERROR("failed to add FORWARDING rule");

        // re-create the prerouting rule args but with delete rather than add
        preroutingRule =
            constructPreRoutingRuleArgs(false, id, protocolStr, addressStr, portStr);

        // try and delete the preouting rule
        if (!execIpTables(preroutingRule))
            AI_LOG_ERROR("failed to remove PREROUTING rule, firewall could be left in invalid state");

        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to remove the hole punch iptables rules.
 *
 *
 *  @param[in]  id          The id of the container making the request
 *  @param[in]  protocol    The protocol of the hole punch (either SOCK_STREAM
 *                          or SOCK_DGRAM).
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  portNumber  The port number.
 *
 */
void HolePuncherPlugin::removeHolePunch(const ContainerId& id, int protocol,
                                        in_addr_t ipAddress, in_port_t portNumber)
{
    AI_LOG_FN_ENTRY();

    // convert the args to string for the iptables command line
    char portStr[32];
    sprintf(portStr, "%hu", portNumber);

    char addressStr[INET_ADDRSTRLEN];
    struct in_addr ipAddress_ = { htonl(ipAddress) };
    inet_ntop(AF_INET, &ipAddress_, addressStr, INET_ADDRSTRLEN);

    std::string protocolStr;
    if (protocol == SOCK_STREAM)
    {
        protocolStr = "tcp";
    }
    else if (protocol == SOCK_DGRAM)
    {
        protocolStr = "udp";
    }
    else
    {
        AI_LOG_ERROR_EXIT("invalid protocol value");
        return;
    }


    // construct the iptables args
    std::list<std::string> preroutingRule =
        constructPreRoutingRuleArgs(false, id, protocolStr, addressStr, portStr);

    std::list<std::string> forwardingRule =
        constructForwardingRuleArgs(false, id, protocolStr, addressStr, portStr);


    // attempt to remove the rules
    if (!execIpTables(forwardingRule))
    {
        AI_LOG_ERROR("failed to remove FORWARDING firewall rule for container '%s'",
                     id.c_str());
    }
    if (!execIpTables(preroutingRule))
    {
        AI_LOG_ERROR("failed to remove PREROUTING firewall rule for container '%s'",
                     id.c_str());
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Performs a fork/exec operation on the iptables tool.
 *
 *  If any of the @a stdinFd, @a stdoutFd or @a stderrFd are less than 0 then
 *  the corresponding fd is redirected to /dev/null.
 *
 *
 *  @param[in]  args        The args to supply to the iptables call
 *  @param[in]  stdinFd     The fd to redirect stdin to
 *  @param[in]  stdoutFd    The fd to redirect stdout to
 *  @param[in]  stderrFd    The fd to redirect stderr to
 *
 *  @return true on success, false on failure.
 */
bool HolePuncherPlugin::execIpTables(const std::list<std::string>& args,
                                     int stdinFd, int stdoutFd, int stderrFd) const
{
    AI_LOG_FN_ENTRY();

    const char iptablesPath[] = "/usr/sbin/iptables";

    // create the args vector (the first arg is always the exe name the last
    // is always nullptr)
    std::vector<char*> execArgs;
    execArgs.reserve(args.size() + 2);
    execArgs.push_back(strdup("iptables"));

    for (const std::string &arg : args)
        execArgs.push_back(strdup(arg.c_str()));

    execArgs.push_back(nullptr);

    // set an empty environment list so we don't leak info
    std::vector<char*> execEnvs(1, nullptr);



    // fork off to execute the iptables-save tool
    pid_t pid = vfork();
    if (pid == 0)
    {
        // within forked child

        // open /dev/null so can redirect stdout and stderr to that
        int devNull = open("/dev/null", O_RDWR);
        if (devNull < 0)
            _exit(EXIT_FAILURE);

        if (stdinFd < 0)
            stdinFd = devNull;
        if (stdoutFd < 0)
            stdoutFd = devNull;
        if (stderrFd < 0)
            stderrFd = devNull;

        // remap the standard descriptors to either /dev/null or one of the
        // supplied descriptors
        if (dup2(stdinFd, STDIN_FILENO) != STDIN_FILENO)
            _exit(EXIT_FAILURE);

        if (dup2(stdoutFd, STDOUT_FILENO) != STDOUT_FILENO)
            _exit(EXIT_FAILURE);

        if (dup2(stderrFd, STDERR_FILENO) != STDERR_FILENO)
            _exit(EXIT_FAILURE);

        // don't need /dev/null anymore
        if (devNull > STDERR_FILENO)
            close(devNull);

        // reset the file mode mask to defaults
        umask(0);

        // reset the signal mask, we need to do this because signal mask are
        // inherited and we've explicitly blocked SIGCHLD as we're monitoring
        // that using sigwaitinfo
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        if (sigprocmask(SIG_UNBLOCK, &set, nullptr) != 0)
            _exit(EXIT_FAILURE);

        // change the current working directory
        if ((chdir("/")) < 0)
            _exit(EXIT_FAILURE);

        // and finally execute the iptables command
        execvpe(iptablesPath, execArgs.data(), execEnvs.data());

        // iptables failed, but don't bother trying to print an error as
        // we've already redirected stdout & stderr to /dev/null
        _exit(EXIT_FAILURE);
    }


    // clean up dup'ed args
    for (char *arg : execArgs)
        free(arg);


    // sanity check the fork worked
    if (pid < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "vfork failed");
        return false;
    }


    // in the parent so wait till the iptables-save process completes
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return false;
    }
    else if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("%s didn't exit? (status: 0x%04x)", iptablesPath,
                          status);
        return false;
    }
    else if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        AI_LOG_ERROR_EXIT("%s failed with exit code %d", iptablesPath,
                          WEXITSTATUS(status));
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Utility expected to be run in the network namespace of the container
 *  to get the ip address.
 *
 *  This uses ioctl's to read the address of the 'eth0' interface inside the
 *  container.  This is a bit inefficient as we know what IP address was
 *  assigned in the NetworkHook code, however this isn't (currently?) passed
 *  on to the plugins.
 *
 *  @param[out] ipAddress       Will be populated by the ip address on success.
 *
 */
void HolePuncherPlugin::getContainerIpAddress(in_addr_t *ipAddress) const
{
    AI_LOG_FN_ENTRY();

    // reset the IP address
    *ipAddress = INADDR_NONE;

    // the interface within the container is always called 'eth0', refer to the
    // plugin code in NatNetworkHook for more details
    struct ifreq ifr;
    bzero(&ifr, sizeof(ifr));
#if defined(DEV_VM)
    strcpy(ifr.ifr_name, "enp0s3");
#else
    strcpy(ifr.ifr_name, "eth0");
#endif

    // create a general socket for the ioctl
    int sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sock < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create socket");
        return;
    }

    // attempt to get the interface details
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to get interface ip address");
        close(sock);
        return;
    }

    if (close(sock) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close interface socket");
    }

    // finally get and store the ip address
    struct sockaddr_in* ifaceAddr = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
    *ipAddress = ntohl(ifaceAddr->sin_addr.s_addr);

    AI_LOG_FN_EXIT();
}
