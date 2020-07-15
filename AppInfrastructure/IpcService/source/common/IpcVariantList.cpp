/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
//
//  IpcVariantList.cpp
//  IpcService
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "IpcCommon.h"
#include "IpcVariantList.h"

#include <Logging.h>



/**
 *  @brief Called for each arg and entry in the AI_IPC::VariantList list
 *
 *  This function takes an iterator to a AI_IPC::VariantList and checks whether
 *  the variant stored in the current position matches the templated arg. If it
 *  does the variant's value is stored in the arg, if not the result flag is
 *  set to false.
 *
 *  @param[in]   it      The interator that points to the entry in the variant list
 *  @param[out]  arg     Pointer to value to store the arg in (if the types match)
 *  @param[out]  result  Pointer to a bool that will be set to false if the
 *                       variant value couldn't be assigned to the arg.
 */
template <typename T>
void AI_IPC::processVal(bool *result, const AI_IPC::VariantList::const_iterator & it, T * arg)
{
#ifdef __GXX_RTTI
    // If we have RTTI perform sanity check the type of the variant against the
    // real type
    if (it->type() != typeid(T))
    {
        AI_LOG_ERROR("argument has invalid type");
        *result = false;
    }
    else
#endif
    {
        // Write the boost variant type to a real value
        try
        {
            *arg = boost::get<T>(*it);
        }
        catch (boost::bad_get & e)
        {
            AI_LOG_ERROR("boost::bad_get exception '%s'", e.what());
            *result = false;
        }
    }
}


#define IPC_PROCESS_VAL_IMPL(T) \
    template void AI_IPC::processVal<T>(bool *result, const AI_IPC::VariantList::const_iterator & it, T *arg)

IPC_PROCESS_VAL_IMPL(bool);
IPC_PROCESS_VAL_IMPL(uint8_t);
IPC_PROCESS_VAL_IMPL(int16_t);
IPC_PROCESS_VAL_IMPL(int32_t);
IPC_PROCESS_VAL_IMPL(uint32_t);
IPC_PROCESS_VAL_IMPL(int64_t);
IPC_PROCESS_VAL_IMPL(uint64_t);
IPC_PROCESS_VAL_IMPL(std::string);
IPC_PROCESS_VAL_IMPL(AI_IPC::DbusObjectPath);
IPC_PROCESS_VAL_IMPL(AI_IPC::IpcFileDescriptor);

IPC_PROCESS_VAL_IMPL(std::vector<uint8_t>);
IPC_PROCESS_VAL_IMPL(std::vector<uint16_t>);
IPC_PROCESS_VAL_IMPL(std::vector<int32_t>);
IPC_PROCESS_VAL_IMPL(std::vector<uint32_t>);
IPC_PROCESS_VAL_IMPL(std::vector<uint64_t>);
IPC_PROCESS_VAL_IMPL(std::vector<std::string>);
IPC_PROCESS_VAL_IMPL(std::vector<AI_IPC::DbusObjectPath>);
IPC_PROCESS_VAL_IMPL(std::vector<AI_IPC::IpcFileDescriptor>);

template void AI_IPC::processVal<std::map<std::string, AI_IPC::DictDataType>>(bool *result, const AI_IPC::VariantList::const_iterator & it, std::map<std::string, AI_IPC::DictDataType> *arg);
