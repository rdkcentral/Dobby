/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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
 * File:   DobbyRdkPluginDependencySolver.cpp
 *
 */
#include "DobbyRdkPluginDependencySolver.h"

#include <Logging.h>

#include <boost/graph/topological_sort.hpp>

// -----------------------------------------------------------------------------
/**
 * @brief Adds a plugin to the solver.
 *
 * Each plugin must be known to the solver - added by this method before its dependencies
 * are tracked.
 *
 * @param[in]   name   Name of the plugin
 *
 * @return True if the plugin has been added successfully,
 * false otherwise (the plugin had already been added).
 *
 */
bool DobbyRdkPluginDependencySolver::addPlugin(const std::string &name)
{
    AI_LOG_FN_ENTRY();

    if (mDescriptorMap.count(name) != 0)
    {
        AI_LOG_WARN("Plugin %s already added to the solver", name.c_str());
        return false;
    }

    // Each plugin is represented by a vertex of a directed graph.
    const VertexDescriptor descriptor = boost::add_vertex(mDependencyGraph);
    // Store the name of the plugin as a property of the vertex.
    boost::put(boost::vertex_name_t{}, mDependencyGraph, descriptor, name);
    mDescriptorMap.insert(StringVertexDescriptorMap::value_type{ name, descriptor });

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 * @brief Adds a dependency between two plugins to the solver.
 *
 * If plugin A depends on plugin B, then the call is:
 * @code addDependency(A, B); @endcode
 *
 * @param[in]   pluginName       The name of the plugin which depends on @p dependencyName.
 * @param[in]   dependencyName   The name of the plugin on which @p pluginName depends.
 *
 * @return True if the dependency has been added successfully,
 * false otherwise (one of the plugins had not been added to the solver).
 *
 */
bool DobbyRdkPluginDependencySolver::addDependency(const std::string &pluginName, const std::string &dependencyName)
{
    AI_LOG_FN_ENTRY();

    if (mDescriptorMap.count(pluginName) == 0)
    {
        AI_LOG_ERROR("Plugin %s unknown", pluginName.c_str());
        return false;
    }
    if (mDescriptorMap.count(dependencyName) == 0)
    {
        AI_LOG_ERROR("Plugin %s unknown", dependencyName.c_str());
        return false;
    }

    // Our dependency relation is represented by a directed edge (pluginName->dependencyName) of the graph.
    boost::add_edge(mDescriptorMap.at(pluginName), mDescriptorMap.at(dependencyName), mDependencyGraph);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 * @brief Gets the names of the plugins in order of their dependency.
 *
 * "Order of dependency" here means that if plugin A depends on plugin B, plugin B will be placed
 * before plugin A in the returned vector, e.g. with this code:
 * @code
 * DobbyRdkPluginDependencySolver solver;
 * solver.addPlugin("AppServices");
 * solver.addPlugin("Networking");
 * solver.addPlugin("IPC");
 * solver.addDependency("AppServices", "Networking");
 * solver.addDependency("Networking", "IPC");
 *
 * const std::vector<std::string> inOrder = solver.getOrderOfDependency();
 * @endcode,
 * @code inOrder @endcode is @code { "IPC", "Networking", "AppServices" } @endcode.
 *
 * If there is a dependency cycle, an error message is printed and the function returns an empty vector.
 *
 * @return Names of the plugins in order of dependency. Empty vector if no plugins have been added or a
 * dependency cycle has been detected. Vector with plugin names in the same order in which they were added
 * if no dependencies have been added).
 */
std::vector<std::string> DobbyRdkPluginDependencySolver::getOrderOfDependency() const
{
    AI_LOG_FN_ENTRY();

    std::vector<std::string> namesInOrder;
    std::vector<VertexDescriptor> descriptorsInOrder;
    try
    {
        // If edge (u, v) appears in the graph, a topological sort will put v before u. This is our order of dependency.
        boost::topological_sort(mDependencyGraph, std::back_inserter(descriptorsInOrder));
    }
    catch(const boost::not_a_dag &e)
    {
        // Thrown when a graph is not a directed acyclic graph (DAG). In our case that means we have a dependency cycle.
        AI_LOG_ERROR("Dependency cycle detected");
        return namesInOrder;
    }

    // boost::topological_sort returns vertex descriptors. Convert them to our plugin names.
    const auto nameMap = boost::get(boost::vertex_name, mDependencyGraph);
    std::transform(descriptorsInOrder.begin(), descriptorsInOrder.end(), std::back_inserter(namesInOrder),
                   [&nameMap](const VertexDescriptor &d) -> std::string { return nameMap[d]; });

    AI_LOG_FN_EXIT();
    return namesInOrder;
}

/**
 * @brief Gets the names of the plugins in reversed order of their dependency.
 *
 * "Reversed order of dependency" here means that if plugin A depends on plugin B, plugin B will be placed
 * after plugin A in the returned vector, e.g. with this code:
 * @code
 * DobbyRdkPluginDependencySolver solver;
 * solver.addPlugin("AppServices");
 * solver.addPlugin("Networking");
 * solver.addPlugin("IPC");
 * solver.addDependency("AppServices", "Networking");
 * solver.addDependency("Networking", "IPC");
 *
 * const std::vector<std::string> inReversedOrder = solver.getReversedOrderOfDependency();
 * @endcode,
 * @code inOrder @endcode is @code { "AppServices", "Networking", "IPC" } @endcode.
 *
 * If there is a dependency cycle, an error message is printed and the function returns an empty vector.
 *
 * @return Names of the plugins in reversed order of dependency. Empty vector if no plugins have been added or a
 * dependency cycle has been detected. Vector with plugin names in the same order in which they were added
 * if no dependencies have been added).
 */
std::vector<std::string> DobbyRdkPluginDependencySolver::getReversedOrderOfDependency() const
{
    AI_LOG_FN_ENTRY();

    std::vector<std::string> namesInReversedOrder = getOrderOfDependency();
    std::reverse(namesInReversedOrder.begin(), namesInReversedOrder.end());

    AI_LOG_FN_EXIT();
    return namesInReversedOrder;
}
