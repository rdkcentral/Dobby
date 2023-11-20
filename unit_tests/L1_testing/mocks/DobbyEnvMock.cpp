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

#include "DobbyEnvMock.h"

DobbyEnvImpl *DobbyEnv::impl = nullptr;

DobbyEnv::DobbyEnv()
{
}

DobbyEnv::~DobbyEnv()
{
}

DobbyEnv::DobbyEnv(const std::shared_ptr<const IDobbySettings>& settings)
{
}

void DobbyEnv::setImpl(DobbyEnvImpl* newImpl)
{
    impl = newImpl;
}

DobbyEnv* DobbyEnv::getInstance()
{
    static DobbyEnv* instance = nullptr;
    if(nullptr == instance)
    {
       instance = new DobbyEnv();
    }
    return instance;
}

std::string DobbyEnv::workspaceMountPath()
{
   EXPECT_NE(impl, nullptr);

   return impl->workspaceMountPath();
}

std::string DobbyEnv::flashMountPath()
{
   EXPECT_NE(impl, nullptr);

   return impl->flashMountPath();
}

std::string DobbyEnv::pluginsWorkspacePath()
{
   EXPECT_NE(impl, nullptr);

   return impl->pluginsWorkspacePath();
}

uint16_t DobbyEnv::platformIdent()
{
   EXPECT_NE(impl, nullptr);

   return impl->platformIdent();
}
