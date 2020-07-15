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
/*
 * DbusMessageParser.cpp
 *
 *  Created on: 9 Jun 2015
 *      Author: riyadh
 */

#include "IpcCommon.h"
#include "DbusMessageParser.h"

#include <Logging.h>

#include <dbus/dbus.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>

#include <vector>
#include <string>

namespace {

template <typename T, typename dbusT>
std::vector<T> parsePrimitiveArray(DBusMessageIter& arrayIter)
{
    AI_LOG_FN_ENTRY();

    std::vector<T> values;

    // For empty arrays the arg type will be invalid, check first before
    // iterating
    if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        do
        {
            dbusT value;
            dbus_message_iter_get_basic( &arrayIter, &value );
            values.push_back(value);
        }
        while(dbus_message_iter_next( &arrayIter ));
    }

    AI_LOG_DEBUG( "Received array size: %zu", values.size() );

    AI_LOG_FN_EXIT();

    return values;
}

std::vector<AI_IPC::UnixFd> parseArrayUnixFd(DBusMessageIter& arrayIter)
{
    AI_LOG_FN_ENTRY();

    std::vector<AI_IPC::IpcFileDescriptor> values;

    if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        do
        {
            int fd = -1;
            dbus_message_iter_get_basic( &arrayIter, &fd );
            if (fd >= 0)
            {
                values.push_back( AI_IPC::IpcFileDescriptor(fd) );
                if (close(fd) != 0)
                    AI_LOG_SYS_ERROR( errno, "Failed to close returned fd" );
            }

        } while(dbus_message_iter_next( &arrayIter ));
    }

    AI_LOG_DEBUG( "Received DBUS_TYPE_UNIX_FD array size: %zu", values.size() );

    AI_LOG_FN_EXIT();

    return values;
}

std::vector<AI_IPC::DbusObjectPath> parseArrayObjectPaths(DBusMessageIter& arrayIter)
{
    AI_LOG_FN_ENTRY();

    std::vector<AI_IPC::DbusObjectPath> values;

    if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        do
        {
            const char *cstr;
            dbus_message_iter_get_basic( &arrayIter, &cstr );
            AI_IPC::DbusObjectPath dbusObjectPath(cstr);
            values.push_back(dbusObjectPath);
        } while(dbus_message_iter_next( &arrayIter ));
    }

    AI_LOG_DEBUG( "Received DBUS_TYPE_OBJECT_PATH array size: %zu", values.size() );

    AI_LOG_FN_EXIT();

    return values;
}

std::vector<std::string> parseArrayString(DBusMessageIter& arrayIter)
{
    AI_LOG_FN_ENTRY();

    std::vector<std::string> values;

    if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        do
        {
            const char *cstr;
            dbus_message_iter_get_basic( &arrayIter, &cstr );
            values.push_back(std::string(cstr));
        } while(dbus_message_iter_next( &arrayIter ));
    }

    AI_LOG_DEBUG( "Received DBUS_TYPE_STRING array size: %zu", values.size() );

    AI_LOG_FN_EXIT();

    return values;
}

bool getDictValue(DBusMessageIter *dictIter, AI_IPC::DictDataType& dictValue)
{
    bool error = false;

    if (DBUS_TYPE_BYTE == dbus_message_iter_get_arg_type(dictIter))
    {
        uint8_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_BYTE %d", value);
    }
    else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(dictIter))
    {
        dbus_bool_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value ? true : false;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_BOOLEAN %s", value ? "true" : "false");
    }
    else if (DBUS_TYPE_INT16 == dbus_message_iter_get_arg_type(dictIter))
    {
        int16_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_INT16 %d", value);
    }
    else if (DBUS_TYPE_UINT16 == dbus_message_iter_get_arg_type(dictIter))
    {
        uint16_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_UINT16 %u", value);
    }
    else if (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(dictIter))
    {
        int32_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_INT32 %d", value);
    }
    else if (DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type(dictIter))
    {
        uint32_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_UINT32 %u", value);
    }
    else if (DBUS_TYPE_INT64 == dbus_message_iter_get_arg_type(dictIter))
    {
        int64_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_INT64 %" PRId64 "", value);
    }
    else if (DBUS_TYPE_UINT64 == dbus_message_iter_get_arg_type(dictIter))
    {
        uint64_t value;
        dbus_message_iter_get_basic( dictIter, &value );
        dictValue = value;
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_UINT64 %" PRIu64 "", value);
    }
    else if (DBUS_TYPE_UNIX_FD == dbus_message_iter_get_arg_type(dictIter))
    {
        int fd = -1;
        dbus_message_iter_get_basic( dictIter, &fd );
        if (fd >= 0)
        {
            dictValue = AI_IPC::IpcFileDescriptor(fd);
            if (close(fd) != 0)
                AI_LOG_SYS_ERROR(errno, "Failed to close returned file descriptor");
        }
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_UNIX_FD %d", fd);
    }
    else if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(dictIter))
    {
        char* cstr;
        dbus_message_iter_get_basic( dictIter, &cstr );
        dictValue = std::string(cstr);
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_STRING %s", cstr);
    }
    else if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(dictIter))
    {
        char* cstr;
        dbus_message_iter_get_basic( dictIter, &cstr );
        dictValue = AI_IPC::DbusObjectPath(cstr);
        AI_LOG_DEBUG( "Received dictionary value of type DBUS_TYPE_OBJECT_PATH %s", cstr);
    }
    else if (DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(dictIter))
    {
        DBusMessageIter valueIter;
        dbus_message_iter_recurse(dictIter, &valueIter);
        getDictValue(&valueIter, dictValue);
    }
    else
    {
        AI_LOG_ERROR( "Unsupported dbus data type detected for dict entry: %u", dbus_message_iter_get_arg_type(dictIter) );
        error = true;
    }

    return !error;
}

std::map<std::string, AI_IPC::DictDataType> parseDict(DBusMessageIter& arrayIter)
{
    AI_LOG_FN_ENTRY();

    std::map<std::string, AI_IPC::DictDataType> dict;

    if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        do
        {
            DBusMessageIter dictIter;
            dbus_message_iter_recurse(&arrayIter, &dictIter);

            const char *dictKey;
            AI_IPC::DictDataType dictValue;

            dbus_message_iter_get_basic( &dictIter, &dictKey );

            AI_LOG_DEBUG( "Received dictionary key %s", dictKey);

            dbus_message_iter_next(&dictIter);

            if (getDictValue(&dictIter, dictValue))
            {
                dict[std::string(dictKey)] = dictValue;
            }
            else
            {
                AI_LOG_ERROR( "unable to get dict value for key: %s", dictKey );
            }
        } while(dbus_message_iter_next( &arrayIter ));
    }
    else
    {
        AI_LOG_ERROR( "Invalid iterator type");
    }

    AI_LOG_DEBUG( "Received DBUS_TYPE_DICT array size: %zu", dict.size() );

    AI_LOG_FN_EXIT();

    return dict;
}

bool parseArray(DBusMessageIter& iter, AI_IPC::VariantList& argList)
{
    AI_LOG_FN_ENTRY();

    int res = true;

    DBusMessageIter arrayIter;

    dbus_message_iter_recurse( &iter, &arrayIter );

    int arrayArgType = dbus_message_iter_get_element_type( &iter );

    switch ( arrayArgType )
    {
        case DBUS_TYPE_BYTE:
            argList.push_back( parsePrimitiveArray<uint8_t,unsigned char>(arrayIter) );
            break;

        case DBUS_TYPE_UINT16:
            argList.push_back( parsePrimitiveArray<uint16_t,dbus_uint16_t>(arrayIter) );
            break;

        case DBUS_TYPE_INT32:
            argList.push_back( parsePrimitiveArray<int32_t,dbus_int32_t>(arrayIter) );
            break;

        case DBUS_TYPE_UINT32:
            argList.push_back( parsePrimitiveArray<uint32_t,dbus_uint32_t>(arrayIter) );
            break;

        case DBUS_TYPE_UINT64:
            argList.push_back( parsePrimitiveArray<uint64_t,dbus_uint64_t>(arrayIter) );
            break;

        case DBUS_TYPE_UNIX_FD:
            argList.push_back( parseArrayUnixFd(arrayIter) );
            break;

        case DBUS_TYPE_OBJECT_PATH:
            argList.push_back( parseArrayObjectPaths(arrayIter) );
            break;

        case DBUS_TYPE_STRING:
            argList.push_back( parseArrayString(arrayIter) );
            break;

        case DBUS_TYPE_DICT_ENTRY:
            argList.push_back( parseDict(arrayIter) );
            break;

        default:
            AI_LOG_ERROR( "Found invalid array element type: %d", arrayArgType );
            res = false;
    }

    AI_LOG_FN_EXIT();

    return res;
}

}

using namespace AI_IPC;

DbusMessageParser::DbusMessageParser(DBusMessage* msg)
    : mDbusMsg(msg)
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

bool DbusMessageParser::parseMsg()
{
    AI_LOG_FN_ENTRY();

    bool res = true;

    DBusMessageIter iter;

    bool continueLoop = true;

    if ( dbus_message_iter_init( mDbusMsg, &iter ) ) // TRUE if the message has arguments
    {
        while ( continueLoop == true )
        {
            int argType = dbus_message_iter_get_arg_type( &iter );

            switch ( argType )
            {
                case DBUS_TYPE_BYTE:
                {
                    uint8_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(value);
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_BYTE: %u", value );
                }
                break;

                case DBUS_TYPE_BOOLEAN:
                {
                    dbus_bool_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back( value ? true : false );
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_BOOLEAN: %s", value ? "true" : "false" );
                }
                break;

                case DBUS_TYPE_INT16:
                {
                    int16_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(value);
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_INT16: %hu", value );
                }
                break;

                case DBUS_TYPE_UINT16:
                {
                    uint16_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(value);
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_UINT16: %hu", value );
                }
                break;

                case DBUS_TYPE_INT32:
                {
                    int32_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(value);
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_INT32: %d", value );
                }
                break;

                case DBUS_TYPE_UINT32:
                {
                    uint32_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(value);
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_UINT32: %u", value );
                }
                break;

                case DBUS_TYPE_INT64:
                {
                    int64_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(value);
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_UINT64: %" PRIu64 "", value );
                }
                break;

                case DBUS_TYPE_UINT64:
                {
                    uint64_t value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(value);
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_UINT64: %" PRIu64 "", value );
                }
                break;

                case DBUS_TYPE_UNIX_FD:
                {
                    int fd = -1;
                    dbus_message_iter_get_basic( &iter, &fd );
                    if (fd >= 0)
                    {
                        mArgList.push_back( AI_IPC::IpcFileDescriptor(fd) );
                        if (close(fd) != 0)
                            AI_LOG_SYS_ERROR( errno, "Failed to close returned file descriptor" );
                    }
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_UNIX_FD: %d", fd);
                }
                break;

                case DBUS_TYPE_STRING:
                {
                    char* value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(std::string(value));
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_STRING: %s", value );
                }
                break;

                case DBUS_TYPE_OBJECT_PATH:
                {
                    char* value;
                    dbus_message_iter_get_basic( &iter, &value );
                    mArgList.push_back(std::string(value));
                    AI_LOG_DEBUG( "Received value type DBUS_TYPE_OBJECT_PATH: '%s'", value );
                }
                break;

                case DBUS_TYPE_ARRAY:
                {
                    res = parseArray(iter, mArgList);
                    if( !res )
                    {
                        AI_LOG_ERROR( "Unable to parse array element" );
                        continueLoop = false;
                    }
                }
                break;

                case DBUS_TYPE_INVALID:
                {
                    AI_LOG_DEBUG( "Reached end of iterator list" );
                    continueLoop = false;
                }
                break;

                default:
                    AI_LOG_ERROR( "Found invalid argument type: %d", argType );
                    res = false;
                    break;
            }

            if ( dbus_message_iter_next( &iter ) == FALSE )
            {
                AI_LOG_DEBUG( "Reached end of iterator list" );
                continueLoop = false;
            }
        }
    }

    AI_LOG_FN_EXIT();

    return res;
}

VariantList DbusMessageParser::getArgList()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();

    return mArgList;
}
