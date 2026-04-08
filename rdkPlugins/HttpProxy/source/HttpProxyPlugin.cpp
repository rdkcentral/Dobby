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

#include "HttpProxyPlugin.h"
#include "NetworkingPluginCommon.h"

#include <Logging.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>


REGISTER_RDK_PLUGIN(HttpProxyPlugin);

HttpProxyPlugin::HttpProxyPlugin(std::shared_ptr<rt_dobby_schema> &cfg,
                                 const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                 const std::string &rootfsPath)
    : mName("HttpProxy"),
      mContainerConfig(cfg),
      mPluginData(nullptr),
      mUtils(utils),
      mMountedCACertsPath(rootfsPath + "../ca-certificates.crt")
{
    AI_LOG_FN_ENTRY();

    if (!mContainerConfig || !cfg->rdk_plugins->httpproxy ||
        !cfg->rdk_plugins->httpproxy->data)
    {
        mValid = false;
    }
    else
    {
        mPluginData = cfg->rdk_plugins->httpproxy->data;
        mValid = true;
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 * This plugin uses all the hooks so set all the flags
 */
unsigned HttpProxyPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
        IDobbyRdkPlugin::HintFlags::PreCreationFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag
    );
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 */
bool HttpProxyPlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("invalid config file");
        return false;
    }

    if (!setupHttpProxy())
    {
        AI_LOG_ERROR_EXIT("failed to setup HTTP Proxy environment variables");
        return false;
    }

    // if we're adding a proxy certificate, add a mount for it
    if (mPluginData->proxy_root_ca_cert)
    {
        if (!addCACertificateMount())
        {
            AI_LOG_ERROR_EXIT("failed to add CA certificate mount");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Dobby Hook - run in host namespace before container creation process
 */
bool HttpProxyPlugin::preCreation()
{
    // add proxy to container root CA if needed
    if (mPluginData->proxy_root_ca_cert)
    {
        if (!addProxyToRootCABundle())
        {
            AI_LOG_ERROR_EXIT("failed to add proxy to root CA bundle");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}


/**
 * @brief Dobby Hook - Run in host namespace when container terminates
 */
bool HttpProxyPlugin::postHalt()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("invalid config file");
        return false;
    }

    // remove copied/edited root CA bundle if one was created
    if (mPluginData->proxy_root_ca_cert)
    {
        if (!cleanup())
        {
            AI_LOG_ERROR_EXIT("failed to remove container's root CA bundle");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> HttpProxyPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_http_proxy* pluginConfig = mContainerConfig->rdk_plugins->httpproxy;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the httpproxy and no_proxy environment variables to the
 *  container.
 *
 *  @return true on success, false on failure.
 */
bool HttpProxyPlugin::setupHttpProxy()
{
    AI_LOG_FN_ENTRY();

    // sanity check required fields
    if (mPluginData->proxy->host == nullptr)
    {
        AI_LOG_ERROR_EXIT("missing or invalid http proxy host address");
        return false;
    }
    if (mPluginData->proxy->port == 0)
    {
        AI_LOG_ERROR_EXIT("missing or invalid http proxy port number");
        return false;
    }

    const std::string proxyHost = mPluginData->proxy->host;
    const int proxyPort = mPluginData->proxy->port;
    std::string noProxyList;

    // check the ignoreProxyOnBridge which tells the plugin to automatically
    // add the dobby0 bridge address to the list of hosts to not proxy
    if (mPluginData->ignore_proxy_on_bridge)
    {
        noProxyList += BRIDGE_ADDRESS;
    }

    // get the list of domains to ignore the proxy setting
    for (int i = 0; i < mPluginData->ignore_proxy_len; i++)
    {
        if (!noProxyList.empty())
        {
            noProxyList += ',';
        }
        noProxyList += mPluginData->ignore_proxy[i];
    }

    // add the 'no_proxy' environment var if there are any domains to ignore
    if (!noProxyList.empty())
    {
        std::string noProxyEnvVar = std::string("no_proxy=") + noProxyList;
        if (!mUtils->addEnvironmentVar(noProxyEnvVar))
        {
            AI_LOG_ERROR_EXIT("failed to add no_proxy environment variable");
            return false;
        }
    }

    // add the 'httpproxy' environment var
    char httpProxyEnvVar[256];
    snprintf(httpProxyEnvVar, sizeof(httpProxyEnvVar), "http_proxy=http://%s:%d",
             proxyHost.c_str(), proxyPort);
    if (!mUtils->addEnvironmentVar(httpProxyEnvVar))
    {
        AI_LOG_ERROR_EXIT("failed to add httpproxy environment variable");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a mount to the new ca-certificates.crt file created in the
 *  container's bundle directory in the preCreation hook.
 *
 *  @return true on success, false on failure.
 */
bool HttpProxyPlugin::addCACertificateMount()
{
    AI_LOG_FN_ENTRY();

    // get real path of the ca-certificates
    char hostCACertsPath[PATH_MAX];
    realpath("/etc/ssl/certs/ca-certificates.crt", hostCACertsPath);

    // add a bind mount to the ca-certificates.crt file in the container's
    // bundle. This file is created in the preCreation hook.
    if (!mUtils->addMount(mMountedCACertsPath, hostCACertsPath, "bind",
                         { "bind", "ro" }))
    {
        AI_LOG_ERROR_EXIT("failed to add bind mount from '%s' to '%s'",
                          mMountedCACertsPath.c_str(), hostCACertsPath);
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the proxy's CA cert to the bundle used by the container.
 *
 *  It copies the existing /etc/ssl/certs/ca-certificates.crt file to the
 *  container bundle location and then appends the supplied .crt / .pem
 *  certificate to it.
 *
 *  It then adds a bind mount to the container start-up so that we overlay the
 *  modified the file into the container.
 *
 *  @return true on success, false on failure.
 */
bool HttpProxyPlugin::addProxyToRootCABundle()
{
    AI_LOG_FN_ENTRY();

    // if there's no root CA certificate, we can just exit
    if (mPluginData->proxy_root_ca_cert == nullptr)
    {
        AI_LOG_FN_EXIT();
        return true;
    }

    // get real path of the ca-certificates
    char hostCACertsPath[PATH_MAX];
    realpath("/etc/ssl/certs/ca-certificates.crt", hostCACertsPath);

    // get the existing ca certs
    const std::string existingCerts = mUtils->readTextFile(hostCACertsPath);
    if (existingCerts.empty())
    {
        AI_LOG_WARN("empty '%s' file - missing default ca certs?", hostCACertsPath);
    }

    // prepend the proxy's CA cert (dos2unix)
    std::string newCerts(mPluginData->proxy_root_ca_cert);
    newCerts.append(existingCerts);

    // write the new certs file into the container bundle directory
    if (!mUtils->writeTextFile(mMountedCACertsPath, newCerts, O_CREAT | O_TRUNC, 0644))
    {
        AI_LOG_ERROR_EXIT("failed to write new ca bundle @ '%s'",
                          mMountedCACertsPath.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Cleans up any temp ca-certifice.crt files created for the container.
 *
 *  @return true on success, false on failure.
 */
bool HttpProxyPlugin::cleanup()
{
    AI_LOG_FN_ENTRY();

    // remove the copied ca-certificates.crt file from bundle dir
    if (unlink(mMountedCACertsPath.c_str()) < 0)
    {
        if (errno == ENOENT)
        {
            // file doesn't exist, so there's nothing to remove
            AI_LOG_FN_EXIT();
            return true;
        }
        else
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "could not remove container's "
                                "ca-certificates.crt file");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}
