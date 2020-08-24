/*
 * File: OpenCDMPlugin.h
 *
 */
#ifndef PERFETTOPLUGIN_H
#define PERFETTOPLUGIN_H

#include <IDobbyPlugin.h>
#include <PluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <string>
#include <memory>

// -----------------------------------------------------------------------------
/**
 * @class PerfettoPlugin
 * @brief Dobby plugin for granting access to system perfetto tracing in the
 * container.
 *
 * For now this just bind mounts in the standard perfetto socket used for IPC.
 *
 */
class PerfettoPlugin : public PluginBase
{
public:
    PerfettoPlugin(const std::shared_ptr<IDobbyEnv> &env,
                   const std::shared_ptr<IDobbyUtils> &utils);
    ~PerfettoPlugin() final;

public:
    std::string name() const final;

    unsigned hookHints() const final;

public:
    bool postConstruction(const ContainerId& id,
                          const std::shared_ptr<IDobbyStartState>& startupState,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) final;

private:
    const std::string mName;
    const std::shared_ptr<IDobbyUtils> mUtilities;

    const std::string mDefaultPerfettoSockPath;

};

#endif // !defined(PERFETTOPLUGIN_H)
