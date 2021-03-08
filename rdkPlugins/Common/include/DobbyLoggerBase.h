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
#ifndef DOBBYLOGGERBASE_H
#define DOBBYLOGGERBASE_H

#include <IDobbyRdkLoggingPlugin.h>

// -----------------------------------------------------------------------------
/**
 *  @class RdkPluginBase
 *  @brief Basic object that provides the default overrides for a plugin.
 *
 *  This class just saves the plugins from having to implement hook functions
 *  that are not needed.
 *
 */
class DobbyLoggerBase : public IDobbyRdkLoggingPlugin
{
public:
    virtual ~DobbyLoggerBase() {}

public:
    /**
     * Hook Name: postInstallation
     * Hook Execution Namespace: host
     * Hook Path Resolution: host
     *
     * Execution: Dobby
     *
     * Hook Description: Runs after the OCI bundle has been downloaded to the
     * client STB, before the runtime’s create operation is called. This hook
     * is called only once in lifecycle of container.
     */
    virtual bool postInstallation()
    {
        return true;
    };

    /**
     * Hook Name: preCreation
     * Hook Execution Namespace: host
     * Hook Path Resolution: host
     *
     * Execution: Dobby
     *
     * Hook Description: Runs before the runtime’s create operation is called.
     * This hook runs every time container need to be created.
     */
    virtual bool preCreation()
    {
        return true;
    };

    /**
     * Hook Name: createRuntime
     * Hook Execution Namespace: host
     * Hook Path Resolution: host
     *
     * Execution: OCI Runtime (runc/crun)
     *
     * Hook Description: Run during the create operation, after the runtime
     * environment has been created and before the pivot root or any equivalent
     * operation.
     *
     * Called after the container namespaces are created, so provides an
     * opportunity to customize the container (e.g. the network namespace could
     * be specified in this hook).
     */
    virtual bool createRuntime()
    {
        return true;
    };

    /**
     * Hook Name: createContainer
     * Hook Execution Namespace: container
     * Hook Path Resolution: host
     *
     * Execution: OCI Runtime (runc/crun)
     *
     * Hook Description: Run during the create operation, after the runtime
     * environment has been created and before the pivot root or any equivalent
     * operation.
     *
     * This would run before the pivot_root operation is executed but after the
     * mount namespace was created and setup.
     */
    virtual bool createContainer()
    {
        return true;
    };

#ifdef USE_STARTCONTAINER_HOOK
    /**
     * Hook Name: startContainer
     * Hook Execution Namespace: container
     * Hook Path Resolution: container
     *
     * Execution: OCI Runtime (runc/crun)
     *
     * Hook Description: Runs after the start operation is called but before
     * the user-specified program command is executed.
     *
     * This hook can be used to execute some operations in the container,
     * for example running the ldconfig binary on linux before the container
     * process is spawned.
     */
    virtual bool startContainer()
    {
        return true;
    };
#endif

    /**
     * Hook Name: postStart
     * Hook Execution Namespace: host
     * Hook Path Resolution: host
     *
     * Execution: OCI Runtime (runc/crun)
     *
     * Hook Description: Runs after the user-specified process is executed but
     * before the start operation returns.
     *
     * For example, this hook can notify the user that the container process is
     * spawned.
     */
    virtual bool postStart()
    {
        return true;
    };

    /**
     * Hook Name: postHalt
     * Hook Execution Namespace: host
     * Hook Path Resolution: host
     *
     * Execution: Dobby
     *
     * Hook Description: When a SIGTERM signal is received from the container.
     * Before the delete operation is called
     *
     * For example, this hook could perform clean up when a container closes
     */
    virtual bool postHalt()
    {
        return true;
    };

    /**
     * Hook Name: postStop
     * Hook Execution Namespace: host
     * Hook Path Resolution: host
     *
     * Execution: OCI Runtime (runc/crun)
     *
     * Hook Description: After the container is **deleted** but before the
     * delete operation returns.
     *
     * This hook has a confusing name due to to the stateless nature of OCI
     * runtimes - is relates to the OCI DELETE operation so won't be run until
     * `crun delete` is called
     */
    virtual bool postStop()
    {
        return true;
    };

public:
    /**
     * @brief Should return the names of the plugins this plugin depends on.
     *
     * This can be used to determine the order in which the plugins should be
     * processed when running hooks.
     *
     * @return Names of the plugins this plugin depends on.
     */
    std::vector<std::string> getDependencies() const override
    {
        std::vector<std::string> noDependencies;
        return noDependencies;
    }
};

#endif // !defined(DOBBYLOGGERBASE_H)
