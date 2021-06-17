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
 * File: MappedId.h
 *
 */
#ifndef MAPPEDID_H
#define MAPPEDID_H

#include <memory>
#include <sys/types.h>

#include "rt_dobby_schema.h"

/**
 *  @brief Class that encapsulates fetching user_id and group_id for given container
 */
class MappedId
{
public:
    explicit MappedId(const std::shared_ptr<rt_dobby_schema>& containerConfig);

    uid_t forUser() const;
    gid_t forGroup() const;

private:
    uint32_t getMappedId(uint32_t id, rt_defs_id_mapping **mapping, size_t mapping_len) const;

private:
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
};

#endif // !defined(MAPPEDID_H)
