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

#include "DobbyRootfsMock.h"

DobbyRootfs::DobbyRootfs()
{
}

#if defined(LEGACY_COMPONENTS)
DobbyRootfs::DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const DobbyBundle>& bundle,const std::shared_ptr<const DobbySpecConfig>& config)
{
}
#endif // LEGACY_COMPONENTS

DobbyRootfs::DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const DobbyBundle>& bundle,const std::shared_ptr<const DobbyBundleConfig>& config)
{
}

DobbyRootfs::~DobbyRootfs()
{
}

void DobbyRootfs::setImpl(DobbyRootfsImpl* newImpl)
{
    impl = newImpl;
}

DobbyRootfs* DobbyRootfs::getInstance()
{
    static DobbyRootfs* instance = nullptr;
    if (nullptr == instance)
    {
       instance = new DobbyRootfs();
    }
    return instance;
}

void DobbyRootfs::setPersistence(bool persist)
{
   EXPECT_NE(impl, nullptr);

    return impl->setPersistence(persist);
}

const std::string& DobbyRootfs::path()
{
   EXPECT_NE(impl, nullptr);

    return impl->path();
}

bool DobbyRootfs::isValid()
{
   EXPECT_NE(impl, nullptr);

    return impl->isValid();
}


