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

    static void setImpl(ContainerIdImpl* newImpl)
    {
        impl = newImpl;
    }

    static ContainerId* getInstance()
    {
        static ContainerId* instance = nullptr;
        if(nullptr == instance)
        {
           instance = new ContainerId();
        }
        return instance;
    }

    static bool isValid()
    {
       return impl->isValid();
    }

    const std::string& str() const
    {
        return mId;
    }

    static const char* c_str()
    {
        return impl->c_str();
    }

    static bool isValidContainerId(const std::string& id)
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


    static ContainerId create(const std::string& s)
    {
       std::string str(s);
       ContainerId id;

      if (isValidContainerId(str))
      {
         id.mId.swap(str);
      }

       return id;
    }

   static ContainerId create(const char* s)
   {
      std::string str(s);
      ContainerId id;

      if (isValidContainerId(str))
      {
         id.mId.swap(str);
      }

      return id;
   }

   static ContainerId create(const char* s, size_t n)
   {
      std::string str(s, n);
      ContainerId id;

      if (isValidContainerId(str))
      {
         id.mId.swap(str);
      }

     return id;
   }

    bool operator==(const ContainerId& rhs) const
    {
        return mId == rhs.mId;
    }

    bool operator!=(const ContainerId& rhs) const
    {
        return mId != rhs.mId;
    }

    bool operator<(const ContainerId& rhs) const
    {
        return mId < rhs.mId;
    }

    bool operator>(const ContainerId& rhs) const
    {
        return mId > rhs.mId;
    }


 };

#endif // !defined(CONTAINERID_H)
