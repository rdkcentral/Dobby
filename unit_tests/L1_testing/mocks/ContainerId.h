/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#ifndef CONTAINERID_H
#define CONTAINERID_H

#include <cstdint>
#include <string>


class ContainerIdImpl {
public:
    virtual ~ContainerIdImpl() = default;
    virtual bool isValid() const = 0;
    virtual  const char* c_str() const =0;
};

class ContainerId {

protected:
    static ContainerIdImpl* impl;

public:
    std::string mId;

    ContainerId() = default;
    ContainerId(ContainerId&&) = default;
    ContainerId(const ContainerId&) = default;
    ContainerId& operator=(const ContainerId&) = default;
    ContainerId& operator=(ContainerId&&) = default;
    ~ContainerId() = default;

    static void setImpl(ContainerIdImpl* newImpl);
    static ContainerId* getInstance();
    static bool isValid();
    const std::string& str() const;
    static const char* c_str();
    static bool isValidContainerId(const std::string& id);
    static ContainerId create(const std::string& s);
    static ContainerId create(const char* s);
    static ContainerId create(const char* s, size_t n);
    bool operator==(const ContainerId& rhs) const;
    bool operator!=(const ContainerId& rhs) const;
    bool operator<(const ContainerId& rhs) const;
    bool operator>(const ContainerId& rhs) const;

 };

#endif // !defined(CONTAINERID_H)
