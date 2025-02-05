/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 Sky UK
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

#include "DobbyHibernateMock.h"

DobbyHibernate::Error DobbyHibernate::HibernateProcess(const pid_t pid, const uint32_t timeout, const std::string &locator, const std::string &dumpDirPath, CompressionAlg compression)
{
    EXPECT_NE(impl, nullptr);
    return impl->HibernateProcess(pid, timeout, locator, dumpDirPath, compression);

}

DobbyHibernate::Error DobbyHibernate::WakeupProcess(const pid_t pid, const uint32_t timeout, const std::string &locator)
{
    EXPECT_NE(impl, nullptr);
    return impl->WakeupProcess(pid, timeout, locator);
}

void DobbyHibernate::setImpl(DobbyHibernateImpl* newImpl)
{
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

const std::string DobbyHibernate::DFL_LOCATOR  = "";
const uint32_t DobbyHibernate::DFL_TIMEOUTE_MS = 0;