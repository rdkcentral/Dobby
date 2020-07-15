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
 * IpcUtilities.cpp
 *
 *  Created on: 9 Jun 2015
 *      Author: riyadh
 */

#include "IpcUtilities.h"
#include "IpcCommon.h"
#include "IpcService.h"
#include "IpcVariantList.h"
#include "DbusEventDispatcher.h"
#include "DbusMessageParser.h"

#include <Logging.h>
#include <Common/Interface.h>

#include <dbus/dbus.h>

#include <string>
#include <exception>
#include <stdexcept>
#include <list>
#include <map>
#include <cinttypes>

namespace
{

class DictValueVisitor
    : public boost::static_visitor<>
{
public:

    DictValueVisitor(DBusMessageIter* msgIter) : mDictEntryIter(msgIter) {}

    void operator()(const uint8_t& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_BYTE, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        AI_LOG_DEBUG( "append dict value uint8_t %d", arg );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_BYTE, &arg))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type uint8_t");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const bool& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_BOOLEAN, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        dbus_bool_t data =  (arg == true) ? TRUE : FALSE;

        AI_LOG_DEBUG( "append dict value boolean %d", data );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_BOOLEAN, &data))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type boolean");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const int16_t& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_INT16, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        dbus_int16_t data = (dbus_int16_t)arg;

        AI_LOG_DEBUG( "append dict value int16_t %d", data );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_INT16, &data))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type int16_t");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const uint16_t& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_UINT16, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        dbus_uint16_t data = (dbus_uint16_t)arg;

        AI_LOG_DEBUG( "append dict value uint16_t %u", data );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_UINT16, &data))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type uint16_t");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const int32_t& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_INT32, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        dbus_int32_t data = (dbus_int32_t)arg;

        AI_LOG_DEBUG( "append dict value int32_t %d", data );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_INT32, &data))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type int32_t");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const uint32_t& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_UINT32, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        dbus_uint32_t data = (dbus_uint32_t)arg;

        AI_LOG_DEBUG( "append dict value uint32_t %u", data );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_UINT32, &data))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type uint32_t");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const int64_t& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_INT64, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        dbus_int64_t data = (dbus_int64_t)arg;

        // this may give a warning on x64 build that dbus_int64_t is of type long long int instead of long it
        // the reason is that if there is installed libdbus-1-dev:i386 (this is for the Ubuntu case), then
        // it could link against x86 version instead of x64
        AI_LOG_DEBUG( "append dict value int64_t %" PRId64 "", int64_t(data) );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_INT64, &data))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type int64_t");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }


    void operator()(const uint64_t& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_UINT64, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        dbus_uint64_t data = (dbus_uint64_t)arg;

        AI_LOG_DEBUG( "append dict value uint64_t %" PRIu64 "", uint64_t(data) );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_UINT64, &data))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type uint64_t");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    /*void operator()(const double& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_DOUBLE, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_DOUBLE, &arg))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type double");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }*/

    void operator()(const AI_IPC::UnixFd& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_UNIX_FD, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        AI_LOG_DEBUG( "append dict value unix fd" );

        int fd = arg.fd();
        if (fd < 0)
        {
            throw std::runtime_error("attempting to append invalid file descriptor");
        }

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_UNIX_FD, &fd))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type unix fd");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const std::string& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_STRING, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        const char *cstr = arg.c_str();

        AI_LOG_DEBUG( "append dict value string %s", cstr );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_STRING, &cstr))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type std::string");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

    void operator()(const AI_IPC::DbusObjectPath& arg)
    {
        DBusMessageIter iterDictValue;
        char sig[2] = { DBUS_TYPE_OBJECT_PATH, '\0' };

        dbus_message_iter_open_container(mDictEntryIter, DBUS_TYPE_VARIANT, sig, &iterDictValue);

        const char *cstr = arg.objectPath.c_str();

        AI_LOG_DEBUG( "append dict value dbus object path %s", cstr );

        if (!dbus_message_iter_append_basic(&iterDictValue, DBUS_TYPE_OBJECT_PATH, &cstr))
        {
            dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);

            throw std::runtime_error("dbus dict entry iterator append failed for type object");
        }

        dbus_message_iter_close_container(mDictEntryIter, &iterDictValue);
    }

private:

    DBusMessageIter *mDictEntryIter;
};


class DbusArgVisitor
    : public boost::static_visitor<>
{
public:

    DbusArgVisitor(DBusMessage *dbusMsg, DBusMessageIter& msgIter) : mDbusMsg(dbusMsg), mIter(msgIter) {}

    void operator()(const uint8_t& arg)
    {
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_BYTE, (unsigned char*)&arg))
        {
            throw std::runtime_error("dbus iterator append failed for type uint8_t");
        }
    }

    void operator()(const bool& arg)
    {
        dbus_bool_t value =  (arg == true) ? TRUE : FALSE;
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_BOOLEAN, &value))
        {
            throw std::runtime_error("dbus iterator append failed for type boolean");
        }
    }

    void operator()(const int16_t& arg)
    {
        dbus_int16_t value = (dbus_int16_t)arg;
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_INT16, &value))
        {
            throw std::runtime_error("dbus iterator append failed for type int16_t");
        }
    }

    void operator()(const uint16_t& arg)
    {
        dbus_uint16_t value = (dbus_uint16_t)arg;
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_UINT16, &value))
        {
            throw std::runtime_error("dbus iterator append failed for type uint16_t");
        }
    }

    void operator()(const int32_t& arg)
    {
        dbus_int32_t value = (dbus_int32_t)arg;
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_INT32, &value))
        {
            throw std::runtime_error("dbus iterator append failed for type int32_t");
        }
    }

    void operator()(const uint32_t& arg)
    {
        dbus_uint32_t value = (dbus_uint32_t)arg;
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_UINT32, &value))
        {
            throw std::runtime_error("dbus iterator append failed for type uint32_t");
        }
    }

    void operator()(const int64_t& arg)
    {
        dbus_int64_t value = (dbus_int64_t)arg;
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_INT64, &value))
        {
            throw std::runtime_error("dbus iterator append failed for type int64_t");
        }
    }

    void operator()(const uint64_t& arg)
    {
        dbus_uint64_t value = (dbus_uint64_t)arg;
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_UINT64, &value))
        {
            throw std::runtime_error("dbus iterator append failed for type uint64_t");
        }
    }

    /*void operator()(const double& arg)
    {
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_DOUBLE, &arg))
        {
            throw std::runtime_error("dbus iterator append failed for type double");
        }
    }*/

    void operator()(const AI_IPC::UnixFd& arg)
    {
        const int fd = arg.fd();
        if (fd < 0)
        {
            throw std::runtime_error("attempting to append invalid file descriptor");
        }

        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_UNIX_FD, &fd))
        {
            throw std::runtime_error("dbus iterator append failed for type unix fd");
        }
    }

    void operator()(const std::string& arg)
    {
        const char *cstr = arg.c_str();
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_STRING, &cstr))
        {
            throw std::runtime_error("dbus iterator append failed for type std::string");
        }
    }

    void operator()(const AI_IPC::DbusObjectPath& arg)
    {
        const char *cstr = arg.objectPath.c_str();
        if (!dbus_message_iter_append_basic(&mIter, DBUS_TYPE_OBJECT_PATH, &cstr))
        {
            throw std::runtime_error("dbus iterator append failed for type AI_IPC::DbusObject");
        }
    }

private:
    template<typename T, typename dbusT>
    void appendPrimitiveArray(const std::vector<T> & vec, const char *sig, int type)
    {
        DBusMessageIter subIter;

        if ( dbus_message_iter_open_container(&mIter, DBUS_TYPE_ARRAY, sig, &subIter) )
        {
            if( vec.empty() )
            {
                // Empty vector, therefore we use dbus_message_iter_append_fixed_array as it
                // seems to correctly send an empty array
                const dbusT *v_ARRAY = NULL;

                if( !dbus_message_iter_append_fixed_array(&subIter, type, &v_ARRAY, 0) )
                {
                    dbus_message_iter_close_container (&mIter, &subIter);
                    throw std::runtime_error("dbus_message_iter_append_fixed_array failed for primitive vector type");
                }
            }
            else
            {
                for ( auto ii = vec.begin(); ii != vec.end(); ++ii )
                {
                    dbusT value = *ii;

                    if( !dbus_message_iter_append_basic (&subIter, type, &value) )
                    {
                        dbus_message_iter_close_container (&mIter, &subIter);
                        throw std::runtime_error("dbus_message_iter_append_basic failed for primitive vector type");
                    }
                }
            }

            dbus_message_iter_close_container (&mIter, &subIter);
        }
        else
        {
            throw std::runtime_error("dbus_message_iter_open_container failed for primitive vector type");
        }
    }

    void append_variant(DBusMessageIter *iter, const AI_IPC::DictDataType& value)
    {
        DictValueVisitor dictEntryVisitor(iter);
        boost::apply_visitor(dictEntryVisitor, value);
    }

    void appendDictEntry(DBusMessageIter *dict, const std::string& key, const AI_IPC::DictDataType& value)
    {
        DBusMessageIter subIter;

        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &subIter);

        const char *cstr = key.c_str();

        dbus_message_iter_append_basic(&subIter, DBUS_TYPE_STRING, &cstr);

        AI_LOG_DEBUG( "append dict key %s", cstr);

        append_variant(&subIter, value);

        dbus_message_iter_close_container(dict, &subIter);
    }


public:

    void operator()(const std::vector<uint8_t>& byteVec)
    {
        appendPrimitiveArray<uint8_t, unsigned char>(byteVec, DBUS_TYPE_BYTE_AS_STRING, DBUS_TYPE_BYTE);
    }

    void operator()(const std::vector<uint16_t>& uint16Vec)
    {
        appendPrimitiveArray<uint16_t, dbus_uint16_t>(uint16Vec, DBUS_TYPE_UINT16_AS_STRING, DBUS_TYPE_UINT16);
    }

    void operator()(const std::vector<int32_t>& int32Vec)
    {
        appendPrimitiveArray<int32_t, dbus_int32_t>(int32Vec, DBUS_TYPE_INT32_AS_STRING, DBUS_TYPE_INT32);
    }

    void operator()(const std::vector<uint32_t>& uint32Vec)
    {
        appendPrimitiveArray<uint32_t, dbus_uint32_t>(uint32Vec, DBUS_TYPE_UINT32_AS_STRING, DBUS_TYPE_UINT32);
    }

    void operator()(const std::vector<uint64_t>& uint64Vec)
    {
        appendPrimitiveArray<uint64_t, dbus_uint64_t>(uint64Vec, DBUS_TYPE_UINT64_AS_STRING, DBUS_TYPE_UINT64);
    }

    void operator()(const std::vector<AI_IPC::UnixFd>& unixFdVec)
    {
        DBusMessageIter subIter;

        if ( dbus_message_iter_open_container(&mIter, DBUS_TYPE_ARRAY, DBUS_TYPE_UNIX_FD_AS_STRING, &subIter) )
        {
            for ( auto ii = unixFdVec.begin(); ii != unixFdVec.end(); ++ii )
            {
                const int fd = ii->fd();
                if (fd < 0)
                {
                    dbus_message_iter_close_container (&mIter, &subIter);
                    throw std::runtime_error("attempting to append invalid file descriptor from vector");
                }

                if( !dbus_message_iter_append_basic( &subIter, DBUS_TYPE_UNIX_FD, &fd ) )
                {
                    dbus_message_iter_close_container (&mIter, &subIter);
                    throw std::runtime_error("dbus_message_iter_append_basic failed for type std::vector<unixfd>");
                }
            }

            dbus_message_iter_close_container (&mIter, &subIter);
        }
        else
        {
            throw std::runtime_error("dbus_message_iter_open_container failed for type std::vector<unixfd>");
        }
    }

    void operator()(const std::vector<AI_IPC::DbusObjectPath>& objectPathVec)
    {
        DBusMessageIter subIter;

        if ( dbus_message_iter_open_container(&mIter, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH_AS_STRING, &subIter) )
        {
            for ( auto ii = objectPathVec.begin(); ii != objectPathVec.end(); ++ii )
            {
                const char *cstr = ii->objectPath.c_str();
                if( !dbus_message_iter_append_basic( &subIter, DBUS_TYPE_OBJECT_PATH, &cstr ) )
                {
                    dbus_message_iter_close_container (&mIter, &subIter);
                    throw std::runtime_error("dbus_message_iter_append_basic failed for type std::vector<DbusObjectPath>");
                }
            }

            dbus_message_iter_close_container (&mIter, &subIter);
        }
        else
        {
            throw std::runtime_error("dbus_message_iter_open_container failed for type std::vector<DbusObjectPath>");
        }
    }

    void operator()(const std::vector<std::string>& strVec)
    {
        DBusMessageIter subIter;

        if ( dbus_message_iter_open_container(&mIter, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &subIter) )
        {
            // For string vectors we can't use dbus_message_iter_append_fixed_array() as that only works for
            // fixed sized data.
            for ( auto ii = strVec.begin(); ii != strVec.end(); ++ii )
            {
                const char *cstr = ii->c_str();
                if( !dbus_message_iter_append_basic(&subIter, DBUS_TYPE_STRING, &cstr) )
                {
                    dbus_message_iter_close_container(&mIter, &subIter);
                    throw std::runtime_error("dbus_message_iter_append_basic failed for type std::vector<std::string>");
                }
            }

            dbus_message_iter_close_container(&mIter, &subIter);
        }
        else
        {
            throw std::runtime_error("dbus_message_iter_open_container failed for type std::vector<std::string>");
        }
    }

    void operator() (const std::map<std::string, AI_IPC::DictDataType>& dict)
    {
        DBusMessageIter subIter;

        if ( dbus_message_iter_open_container(&mIter, DBUS_TYPE_ARRAY, "{sv}", &subIter) )
        {
            for(const auto& dictEntry : dict)
            {
                appendDictEntry(&subIter, dictEntry.first, dictEntry.second);
            }

            dbus_message_iter_close_container(&mIter, &subIter);
        }
        else
        {
            throw std::runtime_error("dbus_message_iter_open_container not implemented for type std::map<std::string, AI_IPC::DictDataType>");
        }
    }

private:


    DBusMessage *mDbusMsg;
    DBusMessageIter& mIter;

};

}

namespace AI_IPC
{

bool appendArgsToDbusMsg( DBusMessage *msg, const VariantList& varArgs )
{
    AI_LOG_FN_ENTRY();

    bool res = true;

    if ( !varArgs.empty() )
    {
        DBusMessageIter iterArgs;

        dbus_message_iter_init_append(msg, &iterArgs);

        try
        {
            for( auto ii = varArgs.begin(); ii != varArgs.end(); ++ii )
            {
                DbusArgVisitor visitor(msg, iterArgs);
                AI_IPC::Variant v = *ii;
                boost::apply_visitor(visitor, v );
            }
        }
        catch(const std::exception& e)
        {
            AI_LOG_ERROR( "Dbus emit signal error occurred: %s.", e.what() );
            res = false;
        }
    }

    AI_LOG_FN_EXIT();

    return res;
}

bool operator==(const RemoteEntry& lhs, const RemoteEntry& rhs)
{
    return (lhs.type == rhs.type) && (lhs.object == rhs.object) && (lhs.interface == rhs.interface) && (lhs.name == rhs.name);
}

}
