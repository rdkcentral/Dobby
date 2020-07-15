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
 * File:   PluginBase.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef PLUGINBASE_H
#define PLUGINBASE_H

#include <IDobbyPlugin.h>

// -----------------------------------------------------------------------------
/**
 *  @class PluginBase
 *  @brief Basic object that provides the default overrides for a plugin.
 *
 *  This class just saves the plugins from having to implement hook functions
 *  that are not needed.
 *
 */
class PluginBase : public IDobbyPlugin
{
public:
    virtual ~PluginBase() { }

    
    // Inherited classes must implement these, no excuses
public:
    // virtual std::string name() const override;
    // virtual unsigned hookHints() const override;


    // Inherited classes should override the following where appropriate
public:
    virtual bool postConstruction(const ContainerId& id,
                                  const std::shared_ptr<IDobbyStartState>& startupState,
                                  const std::string& rootfsPath,
                                  const Json::Value& jsonData) override
    {   return true;    }

    virtual bool preStart(const ContainerId& id,
                          pid_t pid,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) override
    {   return true;    }

    virtual bool postStart(const ContainerId& id,
                           pid_t pid,
                           const std::string& rootfsPath,
                           const Json::Value& jsonData) override
    {   return true;    }

    virtual bool postStop(const ContainerId& id,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) override
    {   return true;    }

    virtual bool preDestruction(const ContainerId& id,
                                const std::string& rootfsPath,
                                const Json::Value& jsonData) override
    {   return true;    }

};

#endif // !defined(PLUGINBASE_H)
