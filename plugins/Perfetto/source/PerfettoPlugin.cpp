/*
 * File:   PerfettoPlugin.cpp
 *
 * Copyright (C) Sky.uk 2020+
 */
#include "PerfettoPlugin.h"

#include <Logging.h>

#include <regex>


// -----------------------------------------------------------------------------
/**
 *  @brief Registers the PerfettoPlugin plugin object.
 *
 *  The object is constructed at the start of the Dobby daemon and only
 *  destructed when the Dobby daemon is shutting down.
 *
 */
REGISTER_DOBBY_PLUGIN(PerfettoPlugin);



PerfettoPlugin::PerfettoPlugin(const std::shared_ptr<IDobbyEnv>& env,
                               const std::shared_ptr<IDobbyUtils>& utils)
    : mName("Perfetto")
    , mUtilities(utils)
    , mDefaultPerfettoSockPath("/tmp/perfetto-producer")
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

PerfettoPlugin::~PerfettoPlugin()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Boilerplate that just returns the name of the plugin.
 *
 *  This string needs to match the name specified in the container spec json.
 *
 */
std::string PerfettoPlugin::name() const
{
    return mName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Indicates which hook points we want and whether to run the
 *  asynchronously or synchronously with the other hooks
 *
 *  For PerfettoPlugin everything is done in the postConstruction phase.
 */
unsigned PerfettoPlugin::hookHints() const
{
    return IDobbyPlugin::PostConstructionSync;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds bind mounts for the perfetto producer socket if it exists.
 *
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  startupState    The start-up state of the container (ignored)
 *  @param[in]  rootfsPath      The absolute path to the rootfs of the container.
 *  @param[in]  jsonData        The parsed json data from the container spec file.
 *
 *  @return true on success, false on failure.
 */
bool PerfettoPlugin::postConstruction(const ContainerId& id,
                                      const std::shared_ptr<IDobbyStartState>& startupState,
                                      const std::string& rootfsPath,
                                      const Json::Value& jsonData)
{
    AI_LOG_FN_ENTRY();

    const char* name = getenv("PERFETTO_PRODUCER_SOCK_NAME");
    if (name == nullptr)
    {
        name = mDefaultPerfettoSockPath.c_str();
    }

    if (access(name, F_OK) != 0)
    {
        AI_LOG_WARN("missing perfetto producer socket @ '%s', is traced running?",
                    name);
    }
    else
    {
        const unsigned mountFlags = (MS_BIND | MS_NOSUID | MS_NODEV | MS_NOEXEC);

        // mount the socket within the container
        if (!startupState->addMount(name, mDefaultPerfettoSockPath, "bind", mountFlags))
        {
            AI_LOG_ERROR("failed to add bind mount for '%s'", name);
        }
        else
        {
            // and add a path to ensure no doubt as to the location for
            // the containered app
            startupState->addEnvironmentVariable(std::string("PERFETTO_PRODUCER_SOCK_NAME=") +
                                                 mDefaultPerfettoSockPath);
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

