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
#ifndef THUNDERSECURITYAGENT_H
#define THUNDERSECURITYAGENT_H

#include <string>
#include <vector>
#include <chrono>
#include <mutex>

class ThunderSecurityAgent
{
public:
    explicit ThunderSecurityAgent(const std::string &socketAddr,
                                  const std::chrono::milliseconds &defaultTimeout = std::chrono::milliseconds(1000));
    ~ThunderSecurityAgent();

    bool open();
    void close();
    bool isOpen() const;
    std::string getToken(const std::string &bearerUrl);

private:
    bool openNoLock();
    void closeNoLock();

    bool send(uint16_t id, const std::string &data) const;
    bool recv(uint16_t *id, std::string *data) const;

    static std::vector<uint8_t> constructMessage(uint16_t id,
                                                 const std::string &data);
    static bool deconstructMessage(const uint8_t *buf, size_t bufLength,
                                   uint16_t *id, std::string *data);

private:
    const std::string mSocketPath;
    std::chrono::milliseconds mTimeout;
    mutable std::mutex mLock;
    int mSocket;
};
#endif // THUNDERSECURITYAGENT_H