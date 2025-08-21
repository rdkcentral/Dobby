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

#include "ThunderPlugin.h"

#ifdef HAS_SECURITY_AGENT
#include <WPEFramework/securityagent/securityagent.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(ThunderPlugin);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
ThunderPlugin::ThunderPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                             const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                             const std::string &rootfsPath)
    : mName("Thunder"),
      mContainerConfig(containerConfig),
      mRootfsPath(rootfsPath),
      mUtils(utils),
      mNetfilter(std::make_shared<Netfilter>()),
      mThunderPort(9998), // Change this if Thunder runs on non-standard port
      mEnableConnLimit(false),
      mSocketDirectory("/tmp/SecurityAgent"),
      mSocketPath(mSocketDirectory + "/token")
{
    AI_LOG_FN_ENTRY();

    mSocketExists = access(mSocketPath.c_str(), F_OK) == 0;

    AI_LOG_FN_EXIT();
}

ThunderPlugin::~ThunderPlugin()
{
    AI_LOG_FN_ENTRY();

    /*
    TODO:: To be a good citizen, we should call dispose() to clean up after
    ourselves when we're done.

    However this dtor is called every time a container exits, so we end up
    disposing the singleton then attempting to reuse it, causing Dobby to
    lock up. In theory the singleton should be recreated when we call GetToken
    but this doesn't work properly and hangs (bug in Thunder?). We should ideally
    only call this when the entire daemon goes for shutdown, not after every
    container exit.

    Not calling dispose() causes some Thunder warnings at daemon shutdown
    but does not seem to cause a crash or user-facing issue so ignoring it for
    now.
    */

    // #ifdef HAS_SECURITY_AGENT
    //      securityagent_dispose();
    // #endif

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 * This plugin uses all the hooks so set all the flags
 */
unsigned ThunderPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
        IDobbyRdkPlugin::HintFlags::PreCreationFlag |
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag);
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 *
 * Updates the /etc/services and /etc/hosts file to point to the mapped
 * thunder server
 */
bool ThunderPlugin::postInstallation()
{
    // This all assumes we're in NAT network mode. If in Open network,
    // don't do this...
    if (isNatNetworkMode())
    {
        // We can get network info - must be running in NAT network mode
        // Set up the /etc/hosts and /etc/service files
        std::string hostFilePath = mRootfsPath + "/etc/hosts";
        if (!mUtils->writeTextFile(hostFilePath, "100.64.11.1\tthunder\t\n",
                                    O_CREAT | O_APPEND | O_WRONLY, 0644))
        {
            AI_LOG_ERROR("Failed to update hosts file with Thunder IP address");
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "thunder\t%hu/tcp\t\t# Thunder Services\n", mThunderPort);

        std::string servicesFilePath = mRootfsPath + "/etc/services";
        if (!mUtils->writeTextFile(servicesFilePath, buf, O_CREAT | O_APPEND | O_WRONLY, 0644))
        {
            AI_LOG_ERROR("Failed to update services file with Thunder details");
        }

        // Set the THUNDER_ACCESS envvar to the Dobby bridge IP address
        bzero(buf, sizeof(buf));
        snprintf(buf, sizeof(buf), "THUNDER_ACCESS=100.64.11.1:%hu", mThunderPort);
        mUtils->addEnvironmentVar(buf);
    }
    else
    {
        // Set the THUNDER_ACCESS envvar to host localhost
        char buf[64];
        bzero(buf, sizeof(buf));
        snprintf(buf, sizeof(buf), "THUNDER_ACCESS=127.0.0.1:%hu", mThunderPort);
        mUtils->addEnvironmentVar(buf);
    }

    // Check if app is trusted - do in PostInstallation so we don't add duplicate
    // mounts
    if (mContainerConfig->rdk_plugins->thunder->data->trusted_present && mContainerConfig->rdk_plugins->thunder->data->trusted)
    {
        if (!mSocketExists)
        {
            AI_LOG_ERROR("Thunder security agent socket not found @ '%s', cannot add bind-mount",
                         mSocketPath.c_str());
        }
        else
        {
            // This is a "trusted" app so we will allow it to generate a token
            // by itself
            AI_LOG_INFO("Container is trusted. Adding bind mount for Thunder SecurityAgent socket @ '%s'",
                        mSocketPath.c_str());

            mUtils->addMount(mSocketPath, mSocketPath, "bind",
                             {"bind", "ro", "nosuid", "nodev", "noexec"});
    }
    mUtils->addMount("/tmp/communicator", "/tmp/communicator", "bind",
                             {"bind", "ro", "nosuid", "nodev", "noexec"});

    return true;
}

bool ThunderPlugin::preCreation()
{
    AI_LOG_FN_ENTRY();

    if (mContainerConfig->rdk_plugins->thunder->data->trusted_present &&
        mContainerConfig->rdk_plugins->thunder->data->trusted && mSocketExists)
    {
        // The /tmp/SecurityAgent dir must have +x set for search
        // Do this every time as this will be reset every bootup
        struct stat buf;
        if (stat(mSocketDirectory.c_str(), &buf) != 0)
        {
            AI_LOG_SYS_WARN(errno, "stat failed on '%s'", mSocketDirectory.c_str());
        }

        if (chmod(mSocketDirectory.c_str(), buf.st_mode | S_IXOTH) != 0)
        {
            AI_LOG_SYS_WARN(errno, "failed to set the thunder socket permissions");
        }
    }

    // Add an environment variable to the config containing the token
    if (mContainerConfig->rdk_plugins->thunder->data->bearer_url)
    {
#ifdef HAS_SECURITY_AGENT
        if (!mSocketExists)
        {
            AI_LOG_ERROR("Thunder security agent socket not found @ '%s', cannot generate token",
                         mSocketPath.c_str());
            return false;
        }

        // Protect from unbounded payload size
        const size_t maxPayloadSize = 2048;
        const std::string payload(mContainerConfig->rdk_plugins->thunder->data->bearer_url);
        const size_t inputLength = std::min(maxPayloadSize, payload.length());

        if (payload.length() > maxPayloadSize)
        {
            AI_LOG_WARN("Bearer URL is too long and will be truncated");
        }

        std::vector<uint8_t> buffer;
        buffer.resize(maxPayloadSize);
        memcpy(buffer.data(), payload.c_str(), inputLength);

        // Retry twice
        const int maxRetryCount = 2;
        int tokenLength;
        for (int i = 1; i <= maxRetryCount; i++)
        {
            if ((tokenLength = GetToken(maxPayloadSize, inputLength, buffer.data())) < 0)
            {
                AI_LOG_ERROR("Failed to generate Thunder token - attempt %d/%d", i, maxRetryCount);
            }
            else
            {
                break;
            }
        }

        if (tokenLength < 0)
        {
            // Couldn't generate the token
            AI_LOG_FN_EXIT();
            return false;
        }

        std::string token(reinterpret_cast<const char *>(buffer.data()), tokenLength);
        mUtils->addEnvironmentVar("THUNDER_SECURITY_TOKEN=" + token);
#else
        AI_LOG_ERROR("bearerUrl set in config but Dobby built without SecurityAgent support. Cannot generate token");
#endif
    }
    else
    {
        AI_LOG_INFO("No bearerUrl set - skipping token generation");
    }

    AI_LOG_FN_EXIT();
    return true;
}

bool ThunderPlugin::createRuntime()
{
    AI_LOG_FN_ENTRY();

    if (!isNatNetworkMode())
    {
        AI_LOG_DEBUG("Not running in NAT network mode - no firewall rules to add");
        return true;
    }

    // construct the ruleset
    Netfilter::RuleSet ruleSet = constructRules();
    if (ruleSet.empty())
    {
        AI_LOG_ERROR_EXIT("failed to construct Thunder iptables rules for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    // add all rules to cache
    if (!mNetfilter->addRules(ruleSet, AF_INET, Netfilter::Operation::Insert))
    {
        AI_LOG_ERROR_EXIT("failed to setup Thunder iptables rules for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    // actually apply the rules
    if (!mNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("Failed to apply Thunder iptables rules for '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

bool ThunderPlugin::postHalt()
{
    AI_LOG_FN_ENTRY();

    if (!isNatNetworkMode())
    {
        AI_LOG_DEBUG("Not running in NAT network mode - no firewall rules to remove");
        return true;
    }

    // construct the same ruleset as in createRuntime() to delete the rules
    Netfilter::RuleSet ruleSet = constructRules();
    if (ruleSet.empty())
    {
        AI_LOG_ERROR_EXIT("failed to construct Thunder iptables rules for deletion for '%s''", mUtils->getContainerId().c_str());
        return false;
    }

    // add all rules to cache
    if (!mNetfilter->addRules(ruleSet, AF_INET, Netfilter::Operation::Delete))
    {
        AI_LOG_ERROR_EXIT("failed to setup Thunder iptables rules for deletion for '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    // actually delete the rules
    if (!mNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("Failed to delete Thunder iptables rules for '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End Hook Methods

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> ThunderPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_thunder *pluginConfig = mContainerConfig->rdk_plugins->thunder;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods

Netfilter::RuleSet ThunderPlugin::constructRules() const
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

    // add the Thunder iptables rules
    std::list<std::string> acceptRules;
    std::list<std::string> natRules;

    int connLimit;
    if (mContainerConfig->rdk_plugins->thunder->data->conn_limit_present)
    {
        connLimit = mContainerConfig->rdk_plugins->thunder->data->conn_limit;
    }
    else
    {
        // Seems like a reasonable limit
        connLimit = 32;
    }

    // Add connection limit rules
    if (mEnableConnLimit)
    {
        acceptRules.emplace_back(constructCONNLIMITRule(ipAddress, vethName, mThunderPort, connLimit));
    }

    // Add input accept rules
    acceptRules.emplace_back(constructACCEPTRule(ipAddress, vethName, mThunderPort));

    // Add DNAT rules
    natRules.emplace_back(constructDNATRule(ipAddress, mThunderPort));

    ruleSet[Netfilter::TableType::Filter] = std::move(acceptRules);
    ruleSet[Netfilter::TableType::Nat] = std::move(natRules);

    AI_LOG_FN_EXIT();
    return ruleSet;
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
std::string ThunderPlugin::constructDNATRule(const std::string &containerIp,
                                             in_port_t port) const
{
    AI_LOG_FN_ENTRY();

    char buf[256];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("dobby-thunder:" + containerId);
#else
    const std::string comment("\"dobby-thunder:" + containerId + "\"");
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
 *  @return the iptables formatted string.
 */
std::string ThunderPlugin::constructCONNLIMITRule(const std::string &containerIp,
                                                  const std::string &vethName,
                                                  in_port_t port,
                                                  uint32_t connLimit) const
{
    AI_LOG_FN_ENTRY();

    char buf[512];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("dobby-thunder:" + containerId);
#else
    const std::string comment("\"dobby-thunder:" + containerId + "\"");
#endif

    snprintf(buf, sizeof(buf), "DobbyInputChain -s %s/32 -d 127.0.0.1/32 "
                               "-i dobby0 -p tcp "
                               "-m tcp --dport %hu --tcp-flags FIN,SYN,RST,ACK SYN "
                               "-m connlimit --connlimit-above %d --connlimit-mask 32 --connlimit-saddr "
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
 *  @return the iptables formatted string.
 */
std::string ThunderPlugin::constructACCEPTRule(const std::string &containerIp,
                                               const std::string &vethName,
                                               in_port_t port) const
{
    AI_LOG_FN_ENTRY();

    char buf[256];
    const std::string containerId(mUtils->getContainerId().c_str());

#if defined(DEV_VM)
    const std::string comment("dobby-thunder:" + containerId);
#else
    const std::string comment("\"dobby-thunder:" + containerId + "\"");
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

bool ThunderPlugin::isNatNetworkMode() const
{
    if (mContainerConfig->rdk_plugins->networking == nullptr || mContainerConfig->rdk_plugins->networking->data == nullptr)
    {
        AI_LOG_WARN("Cannot find Networking plugin - container requires network access to reach Thunder");
        return false;
    }

    return std::string(mContainerConfig->rdk_plugins->networking->data->type) == "nat";
}
