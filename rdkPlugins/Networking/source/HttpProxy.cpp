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

#include "HttpProxy.h"
#include "NetworkingPluginCommon.h"

#include <Logging.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the http_proxy and no_proxy environment variables to the
 *  container.
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils class.
 *  @param[in]  config          libocispec bundle config structure.
 *  @param[in]  rootfsPath      The absolute path to the rootfs of the container.
 *
 *  @return true on success, false on failure.
 */
bool HttpProxy::setupHttpProxy(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                               const std::shared_ptr<rt_dobby_schema> &config,
                               const std::string &rootfsPath)
{
    AI_LOG_FN_ENTRY();

    // sanity check required fields
    rt_defs_plugins_networking_data_http_proxy *cfg = config->rdk_plugins->networking->data->http_proxy;
    if (cfg->proxy->host == nullptr)
    {
        AI_LOG_ERROR_EXIT("missing http proxy host address");
        return false;
    }
    if (cfg->proxy->port == 0)
    {
        AI_LOG_ERROR_EXIT("missing http proxy port number");
        return false;
    }

    const std::string proxyHost = cfg->proxy->host;
    const int proxyPort = cfg->proxy->port;
    std::string noProxyList;

    // check the ignoreProxyOnBridge which tells the plugin to automatically
    // add the dobby0 bridge address to the list of hosts to not proxy
    if (cfg->ignore_proxy_on_bridge)
    {
        noProxyList += BRIDGE_ADDRESS;
    }

    // get the list of domains to ignore the proxy setting
    for (int i = 0; i < cfg->ignore_proxy_len; i++)
    {
        if (!noProxyList.empty())
        {
            noProxyList += ',';
        }
        noProxyList += cfg->ignore_proxy[i];
    }

    // add the 'no_proxy' environment var if there are any domains to ignore
    if (!noProxyList.empty())
    {
        std::string noProxyEnvVar = std::string("no_proxy=") + noProxyList;
        if (!utils->addEnvironmentVar(config, noProxyEnvVar))
        {
            AI_LOG_ERROR_EXIT("failed to add no_proxy environment variable");
            return false;
        }
    }

    // add the 'http_proxy' environment var
    char httpProxyEnvVar[256];
    snprintf(httpProxyEnvVar, sizeof(httpProxyEnvVar), "http_proxy=http://%s:%d",
             proxyHost.c_str(), proxyPort);
    if (!utils->addEnvironmentVar(config, httpProxyEnvVar))
    {
        AI_LOG_ERROR_EXIT("failed to add http_proxy environment variable");
        return false;
    }

    // if we're adding a proxy certificate, add a mount for it
    if (config->rdk_plugins->networking->data->http_proxy->proxy_root_ca_cert)
    {
        if (!addCACertificateMount(utils, config, rootfsPath))
        {
            AI_LOG_ERROR_EXIT("failed to add CA certificate mount");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a mount to the new ca-certificates.crt file created in the
 *  container's bundle directory in the preCreation hook.
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils class.
 *  @param[in]  config          libocispec bundle config structure.
 *  @param[in]  rootfsPath      The absolute path to the rootfs of the container.
 *
 *  @return true on success, false on failure.
 */
bool addCACertificateMount(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                           const std::shared_ptr<rt_dobby_schema> &config,
                           const std::string &rootfsPath)
{
    AI_LOG_FN_ENTRY();

    // get real path of the ca-certificates
    char caCertsPath[PATH_MAX];
    realpath("/etc/ssl/certs/ca-certificates.crt", caCertsPath);

    const std::string bundlePath = rootfsPath + "../";
    const std::string newCertsPath = bundlePath + "ca-certificates.crt";

    // add a bind mount to the ca-certificates.crt file in the container's
    // bundle. This file is created in the preCreation hook.
    if (!utils->addMount(config, newCertsPath, caCertsPath, "bind",
                         { "bind", "rec", "ro" }))
    {
        AI_LOG_ERROR_EXIT("failed to add bind mount from '%s' to '%s'",
                          newCertsPath.c_str(), caCertsPath);
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
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils class.
 *  @param[in]  config          libocispec bundle config structure.
 *  @param[in]  rootfsPath      The absolute path to the rootfs of the container.
 *
 *  @return true on success, false on failure.
 */
bool HttpProxy::addProxyToRootCABundle(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                       const std::shared_ptr<rt_dobby_schema> &config,
                                       const std::string &rootfsPath)
{
    AI_LOG_FN_ENTRY();

    const std::string bundlePath = rootfsPath + "../";

    // if there's no root CA certificate, we can just exit
    if (config->rdk_plugins->networking->data->http_proxy->proxy_root_ca_cert == nullptr)
    {
        AI_LOG_FN_EXIT();
        return true;
    }

    // get real path of the ca-certificates
    char caCertsPath[PATH_MAX];
    realpath("/etc/ssl/certs/ca-certificates.crt", caCertsPath);

    // get the existing ca certs
    const std::string existingCerts = utils->readTextFile(caCertsPath);
    if (existingCerts.empty())
    {
        AI_LOG_WARN("empty '%s' file - missing default ca certs?", caCertsPath);
    }

    // prepend the proxy's CA cert (dos2unix)
    std::string newCerts(config->rdk_plugins->networking->data->http_proxy->proxy_root_ca_cert);
    newCerts.append(existingCerts);

    // write the new certs file into the container bundle directory
    const std::string newCertsPath = bundlePath + "ca-certificates.crt";
    if (!utils->writeTextFile(newCertsPath, newCerts, O_CREAT | O_TRUNC, 0644))
    {
        AI_LOG_ERROR_EXIT("failed to write new ca bundle @ '%s'", newCertsPath.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Cleans up any temp ca-certifice.crt files created for the container.
 *
 *  @param[in]  rootfsPath    The absolute path to the rootfs of the container.
 *
 *  @return true on success, false on failure.
 */
bool HttpProxy::cleanup(const std::string &rootfsPath)
{
    AI_LOG_FN_ENTRY();

    const std::string bundlePath = rootfsPath + "/..";
    const std::string certsPath = bundlePath + "/ca-certificates.crt";

    // remove the copied ca-certificates.crt file from bundle dir
    if (unlink(certsPath.c_str()) < 0)
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