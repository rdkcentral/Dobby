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

#include "MappedId.h"

#include <Logging.h>

/**
 * @brief Constructor - called when plugin object is constructed.
 *        For plugins that require user_id and group_id fetching.
 */
MappedId::MappedId(const std::shared_ptr<rt_dobby_schema>& containerConfig)
    : mContainerConfig(containerConfig)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets user id
 *
 *
 *  @return if found user id, if not found 0
 */
uid_t MappedId::forUser() const
{
    AI_LOG_FN_ENTRY();

    uid_t tmp_uid = 0;

    // Firstly get uid/gid from process
    if (mContainerConfig->process
        && mContainerConfig->process->user)
    {
        if (mContainerConfig->process->user->uid_present)
        {
            tmp_uid = mContainerConfig->process->user->uid;
        }
    }

    // Then map it inside container
    tmp_uid = getMappedId(tmp_uid,
        mContainerConfig->linux->uid_mappings,
        mContainerConfig->linux->uid_mappings_len);

    AI_LOG_FN_EXIT();
    return tmp_uid;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets a group id
 *
 *
 *  @return if found group id, if not found 0
 */
gid_t MappedId::forGroup() const
{
    AI_LOG_FN_ENTRY();

    gid_t tmp_gid = 0;

    // Firstly get uid/gid from process
    if (mContainerConfig->process
        && mContainerConfig->process->user)
    {
        if (mContainerConfig->process->user->gid_present)
        {
            tmp_gid = mContainerConfig->process->user->gid;
        }
    }

    // Then map it inside container
    tmp_gid = getMappedId(tmp_gid,
        mContainerConfig->linux->gid_mappings,
        mContainerConfig->linux->gid_mappings_len);

    AI_LOG_FN_EXIT();
    return tmp_gid;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets userId or groupId based on mappings
 *
 *  @param[in]  id          Id we want to map
 *  @param[in]  mapping     Mapping that should be used
 *  @param[in]  mapping_len Length of mapping
 *
 *  @return if found mapped id, if not found initial id
 */
uint32_t MappedId::getMappedId(uint32_t id, rt_defs_id_mapping **mapping, size_t mapping_len) const
{
    AI_LOG_FN_ENTRY();

    uint32_t tmp_id = id;

    // get id of the container inside host
    for (size_t i = 0; i < mapping_len; i++)
    {
        // No need to check if container_id, size or host_id is present as all those fields
        // are required ones, this means that if mapping point exists it has all 3 of those

        // Check if id is higher than mapping one, if not it is not the mapping we are looking for
        if (id >= mapping[i]->container_id)
        {
            uint32_t shift = id - mapping[i]->container_id;
            // Check if id is inside this mapping
            if (shift < mapping[i]->size)
            {
                // Shift host as much as ID was shifted
                tmp_id = mapping[i]->host_id + shift;
            }
        }
    }

    if (tmp_id == id)
    {
        AI_LOG_WARN("Mapping not found for id '%d'", id);
    }

    AI_LOG_FN_EXIT();
    return tmp_id;
}
