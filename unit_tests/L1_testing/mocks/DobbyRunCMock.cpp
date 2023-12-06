/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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

#include "DobbyRunCMock.h"

std::pair<pid_t, pid_t>DobbyRunC::create(const ContainerId &id,
                                   const std::shared_ptr<const DobbyBundle> &bundle,
                                   const std::shared_ptr<const IDobbyStream> &console,
                                   const std::list<int> &files, /*= std::list<int>()*/
                                          const std::string& customConfigPath /*= ""*/) const
{
   EXPECT_NE(impl, nullptr);

    return impl->create( id, bundle, console, files, customConfigPath);
}

bool DobbyRunC::destroy(const ContainerId& id, const std::shared_ptr<const IDobbyStream>& console, bool force /*= false*/) const
{
   EXPECT_NE(impl, nullptr);

    return impl->destroy( id, console, force);
}

bool DobbyRunC::start(const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) const
{
   EXPECT_NE(impl, nullptr);

    return impl->start( id, console);
}

bool DobbyRunC::killCont(const ContainerId& id, int signal, bool all) const
{
   EXPECT_NE(impl, nullptr);

    return impl->killCont( id, signal, all);
}

bool DobbyRunC::pause(const ContainerId &id) const
{
   EXPECT_NE(impl, nullptr);

    return impl->pause( id );
}

bool DobbyRunC::resume(const ContainerId &id) const
{
   EXPECT_NE(impl, nullptr);

    return impl->resume( id );
}

std::pair<pid_t, pid_t> DobbyRunC::exec(const ContainerId &id,
                                 const std::string &options,
                                 const std::string &command) const
{
   EXPECT_NE(impl, nullptr);

    return impl->exec( id, options, command);
}

DobbyRunC::ContainerStatus DobbyRunC::state(const ContainerId& id) const
{
   EXPECT_NE(impl, nullptr);

    return impl->state(id);
}

std::list<DobbyRunC::ContainerListItem> DobbyRunC::list() const
{
   EXPECT_NE(impl, nullptr);

    return impl->list();
}

const std::string DobbyRunC::getWorkingDir() const
{
   EXPECT_NE(impl, nullptr);

    return impl->getWorkingDir();
}

DobbyRunC::DobbyRunC(const std::shared_ptr<IDobbyUtils>& utils,
                     const std::shared_ptr<const IDobbySettings> &settings)
{
}

DobbyRunC::DobbyRunC()
{
}

DobbyRunC::~DobbyRunC()
{
}

DobbyRunC* DobbyRunC::getInstance()
{
    static DobbyRunC* instance = nullptr;
    if (nullptr == instance)
    {
        instance = new DobbyRunC();
    }
    return instance;
}

void DobbyRunC::setImpl(DobbyRunCImpl* newImpl)
{
    impl = newImpl;
}

