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
 * File:   DobbyRdkPluginDependencySolver.h
 *
 */
#ifndef DOBBYRDKPLUGINDEPENDENCYSOLVER_H
#define DOBBYRDKPLUGINDEPENDENCYSOLVER_H

#include <string>
#include <vector>

#include <boost/graph/adjacency_list.hpp>

// -----------------------------------------------------------------------------
/**
 *  @class DobbyRdkPluginDependencySolver
 *  @brief Class that tracks dependencies between plugins.
 *
 *  It can be used to get the order in which the plugins should be launched.
 *
 */
class DobbyRdkPluginDependencySolver
{
public:
    bool addPlugin(const std::string &name);
    bool addDependency(const std::string &pluginName, const std::string &dependencyName);

    std::vector<std::string> getOrderOfDependency() const;
    std::vector<std::string> getReversedOrderOfDependency() const;

private:
    using VertexIndexProperty = boost::property<boost::vertex_index_t, std::size_t>;
    using VertexNameProperty = boost::property<boost::vertex_name_t, std::string, VertexIndexProperty>;
    using Graph = boost::adjacency_list<boost::setS, boost::vecS, boost::directedS, VertexNameProperty>;
    using VertexDescriptor = Graph::vertex_descriptor;
    using StringVertexDescriptorMap = std::map<std::string, VertexDescriptor>;

    Graph mDependencyGraph;
    StringVertexDescriptorMap mDescriptorMap;
};

#endif // !defined(DOBBYRDKPLUGINDEPENDENCYSOLVER_H)
