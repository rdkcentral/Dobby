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

#include "ContainerIdMock.h"

void ContainerId::setImpl(ContainerIdImpl* newImpl)
{
    impl = newImpl;
}

ContainerId* ContainerId::getInstance()
{
    static ContainerId* instance = nullptr;
    if(nullptr == instance)
    {
       instance = new ContainerId();
    }
    return instance;
}

bool ContainerId::isValid()
{
   EXPECT_NE(impl, nullptr);

   return impl->isValid();
}

const std::string& ContainerId::str() const
{
    return mId;
}

const char* ContainerId::c_str()
{
   EXPECT_NE(impl, nullptr);

    return impl->c_str();
}

bool ContainerId::isValidContainerId(const std::string& id)
{
    if (id.empty() || (id.size() > 128))
    {
        return false;
    }

    unsigned alphaCount = 0;
    for (const char c : id)
    {
        if (!isalnum(c) && (c != '.') && (c != '-') && (c != '_'))
        {
            return false;
        }

        if (isalpha(c))
        {
            alphaCount++;
        }
    }

    if (id.find("..") != std::string::npos)
    {
       return false;
    }

    return (alphaCount > 0);
}


ContainerId ContainerId::create(const std::string& s)
{
   std::string str(s);
   ContainerId id;

  if (isValidContainerId(str))
  {
     id.mId.swap(str);
  }

   return id;
}

ContainerId ContainerId::create(const char* s)
{
  std::string str(s);
  ContainerId id;

  if (isValidContainerId(str))
  {
     id.mId.swap(str);
  }

  return id;
}

ContainerId ContainerId::create(const char* s, size_t n)
{
  std::string str(s, n);
  ContainerId id;

  if (isValidContainerId(str))
  {
     id.mId.swap(str);
  }

 return id;
}

bool ContainerId::operator==(const ContainerId& rhs) const
{
    return mId == rhs.mId;
}

bool ContainerId::operator!=(const ContainerId& rhs) const
{
    return mId != rhs.mId;
}

bool ContainerId::operator<(const ContainerId& rhs) const
{
    return mId < rhs.mId;
}

bool ContainerId::operator>(const ContainerId& rhs) const
{
    return mId > rhs.mId;
}

