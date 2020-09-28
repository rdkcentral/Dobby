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

#ifndef HTTPPROX_H
#define HTTPPROX_H

#include "DobbyRdkPluginUtils.h"

#include <string>
#include <memory>

// -----------------------------------------------------------------------------
/**
 *  @namespace HttpProxy
 *
 *  @brief Used to set http proxy environment variables and optionally add
 *  additional root ca certificates to the container.
 *
 *  It sets the http_proxy and no_proxy environment variables based on the
 *  plugin data. And if a root CA certificate is included in the plugin data
 *  it will append that onto the end of the /etc/ssl/certs/ca-certificates.crt
 *  file.
 *
 */
namespace HttpProxy
{

bool setupHttpProxy(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                    const std::shared_ptr<rt_dobby_schema> &config,
                    const std::string &rootfsPath);

bool addProxyToRootCABundle(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                            const std::shared_ptr<rt_dobby_schema> &config,
                            const std::string &rootfsPath);

bool cleanup(const std::string &rootfsPath);
};


bool addCACertificateMount(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                           const std::shared_ptr<rt_dobby_schema> &config,
                           const std::string &rootfsPath);

#endif // HTTPPROXYPLUGIN_H