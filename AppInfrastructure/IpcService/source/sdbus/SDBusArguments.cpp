/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
//  SDBusArguments.cpp
//  IpcService
//
//

#include "SDBusArguments.h"

#include <Logging.h>

#include <systemd/sd-bus.h>


using namespace AI_IPC;


// -----------------------------------------------------------------------------
/*!
    \class SDBusVariantVisitor
    \brief Object used to marshall DictDataType types into dbus variant
    containers.

 */
class SDBusVariantVisitor : public boost::static_visitor<>
{
public:
    explicit SDBusVariantVisitor(sd_bus_message *msg)
        : mMsg(msg)
    {
    }

private:
    sd_bus_message* mMsg;

public:
    #define BASIC_VARIANT_TYPE_OPERATOR(T, type)                                    \
        void operator()(const T &arg)                                               \
        {                                                                           \
            char sig[2] = { type, '\0' };                                           \
            int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_VARIANT, sig); \
            if (rc < 0)                                                             \
                throw std::system_error(-rc, std::generic_category());              \
            rc = sd_bus_message_append_basic(mMsg, type, &arg);                     \
            if (rc < 0)                                                             \
                throw std::system_error(-rc, std::generic_category());              \
            rc = sd_bus_message_close_container(mMsg);                              \
            if (rc < 0)                                                             \
                throw std::system_error(-rc, std::generic_category());              \
        }

    BASIC_VARIANT_TYPE_OPERATOR(uint8_t, SD_BUS_TYPE_BYTE)
    BASIC_VARIANT_TYPE_OPERATOR(int16_t, SD_BUS_TYPE_INT16)
    BASIC_VARIANT_TYPE_OPERATOR(uint16_t, SD_BUS_TYPE_UINT16)
    BASIC_VARIANT_TYPE_OPERATOR(int32_t, SD_BUS_TYPE_INT32)
    BASIC_VARIANT_TYPE_OPERATOR(uint32_t, SD_BUS_TYPE_UINT32)
    BASIC_VARIANT_TYPE_OPERATOR(int64_t, SD_BUS_TYPE_INT64)
    BASIC_VARIANT_TYPE_OPERATOR(uint64_t, SD_BUS_TYPE_UINT64)

    #undef BASIC_VARIANT_TYPE_OPERATOR

    void operator()(const bool &arg)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_VARIANT, "b");
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        int val = arg ? 1 : 0;
        rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_BOOLEAN, &val);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        rc = sd_bus_message_close_container(mMsg);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const UnixFd &arg)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_VARIANT, "h");
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        int fd = arg.fd();
        rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_UNIX_FD, &fd);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        rc = sd_bus_message_close_container(mMsg);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const std::string &arg)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_VARIANT, "s");
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_STRING, arg.c_str());
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        rc = sd_bus_message_close_container(mMsg);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const DbusObjectPath &arg)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_VARIANT, "o");
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_OBJECT_PATH, arg.objectPath.c_str());
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());

        rc = sd_bus_message_close_container(mMsg);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category());
    }

};


// -----------------------------------------------------------------------------
/*!
    \class SDBusArgsVisitor
    \brief

 */
class SDBusArgsVisitor : public boost::static_visitor<>
{
public:
    explicit SDBusArgsVisitor(sd_bus_message *msg)
        : mMsg(msg)
    {
    }

private:
    sd_bus_message* mMsg;

public:
    #define BASIC_TYPE_OPERATOR(type, sig)                                      \
        void operator()(const type &arg)                                        \
        {                                                                       \
            int rc = sd_bus_message_append_basic(mMsg, sig, &arg);              \
            if (rc != 0)                                                        \
                throw std::system_error(-rc, std::generic_category());          \
        }

    BASIC_TYPE_OPERATOR(uint8_t, SD_BUS_TYPE_BYTE)
    BASIC_TYPE_OPERATOR(int16_t, SD_BUS_TYPE_INT16)
    BASIC_TYPE_OPERATOR(uint16_t, SD_BUS_TYPE_UINT16)
    BASIC_TYPE_OPERATOR(int32_t, SD_BUS_TYPE_INT32)
    BASIC_TYPE_OPERATOR(uint32_t, SD_BUS_TYPE_UINT32)
    BASIC_TYPE_OPERATOR(int64_t, SD_BUS_TYPE_INT64)
    BASIC_TYPE_OPERATOR(uint64_t, SD_BUS_TYPE_UINT64)

    #undef BASIC_TYPE_OPERATOR

    void operator()(const bool &arg)
    {
        int value = arg ? 1 : 0;
        int rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_BOOLEAN, &value);
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const AI_IPC::UnixFd &arg)
    {
        int fd = arg.fd();
        int rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_UNIX_FD, &fd);
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const std::string &arg)
    {
        int rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_STRING, arg.c_str());
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const AI_IPC::DbusObjectPath& arg)
    {
        int rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_OBJECT_PATH, arg.objectPath.c_str());
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

public:
    #define ARRAY_BASIC_TYPE_OPERATOR(type, sig)                                                        \
        void operator()(const std::vector< type > &arg)                                                 \
        {                                                                                               \
            int rc = sd_bus_message_append_array(mMsg, sig, arg.data(), (arg.size() * sizeof(type)));   \
            if (rc != 0)                                                                                \
                throw std::system_error(-rc, std::generic_category());                                  \
        }

    ARRAY_BASIC_TYPE_OPERATOR(uint8_t, SD_BUS_TYPE_BYTE);
    ARRAY_BASIC_TYPE_OPERATOR(int16_t, SD_BUS_TYPE_INT16);
    ARRAY_BASIC_TYPE_OPERATOR(uint16_t, SD_BUS_TYPE_UINT16);
    ARRAY_BASIC_TYPE_OPERATOR(int32_t, SD_BUS_TYPE_INT32);
    ARRAY_BASIC_TYPE_OPERATOR(uint32_t, SD_BUS_TYPE_UINT32);
    ARRAY_BASIC_TYPE_OPERATOR(int64_t, SD_BUS_TYPE_INT64);
    ARRAY_BASIC_TYPE_OPERATOR(uint64_t, SD_BUS_TYPE_UINT64);

    #undef ARRAY_BASIC_TYPE_OPERATOR


    void operator()(const std::vector<bool> &args)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_ARRAY, "b");
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());

        for (const int arg : args)
        {
            rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_BOOLEAN, &arg);
            if (rc != 0)
                throw std::system_error(-rc, std::generic_category());
        }

        rc = sd_bus_message_close_container(mMsg);
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const std::vector<AI_IPC::UnixFd> &args)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_ARRAY, "h");
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());

        for (const auto &arg : args)
        {
            int fd = arg.fd();

            rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_UNIX_FD, &fd);
            if (rc != 0)
                throw std::system_error(-rc, std::generic_category());
        }

        rc = sd_bus_message_close_container(mMsg);
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const std::vector<std::string> &args)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_ARRAY, "s");
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());

        for (const auto &arg : args)
        {
            rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_STRING, arg.c_str());
            if (rc != 0)
                throw std::system_error(-rc, std::generic_category());
        }

        rc = sd_bus_message_close_container(mMsg);
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

    void operator()(const std::vector<AI_IPC::DbusObjectPath>& args)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_ARRAY, "o");
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());

        for (const auto &arg : args)
        {
            rc = sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_OBJECT_PATH, arg.objectPath.c_str());
            if (rc != 0)
                throw std::system_error(-rc, std::generic_category());
        }

        rc = sd_bus_message_close_container(mMsg);
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }

public:
    void operator() (const std::map<std::string, AI_IPC::DictDataType> &dict)
    {
        int rc = sd_bus_message_open_container(mMsg, SD_BUS_TYPE_ARRAY, "{sv}");
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());

        SDBusVariantVisitor variantVisitor(mMsg);

        for (const auto &entry : dict)
        {
            sd_bus_message_open_container(mMsg, SD_BUS_TYPE_DICT_ENTRY, "sv");

            sd_bus_message_append_basic(mMsg, SD_BUS_TYPE_STRING, entry.first.c_str());
            boost::apply_visitor(variantVisitor, entry.second);

            sd_bus_message_close_container(mMsg);
        }

        rc = sd_bus_message_close_container(mMsg);
        if (rc != 0)
            throw std::system_error(-rc, std::generic_category());
    }


};

// -----------------------------------------------------------------------------
/*!
    \internal

    Copies the values in \a args into the sd-bus message object \a msg.

 */
void SDBusArguments::marshallArgs(sd_bus_message *msg,
                                  const VariantList &args)
{
    if (args.empty())
        return;

    SDBusArgsVisitor visitor(msg);
    try
    {
        for (const auto &arg : args)
        {
            boost::apply_visitor(visitor, arg);
        }
    }
    catch (const std::exception& e)
    {
        AI_LOG_ERROR("exception thrown processing sd-bus args - %s", e.what());
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads a basic value and returns it.

 */
template<typename T>
static T readBasicType(sd_bus_message *msg, char type)
{
    T value;
    sd_bus_message_read_basic(msg, type, &value);
    return value;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads a basic value and returns it.

 */
template<typename T>
static std::vector<T> readBasicArray(sd_bus_message *msg, char type)
{
    const T *values = nullptr;
    size_t size = 0;

    int rc = sd_bus_message_read_array(msg, type,
                                       reinterpret_cast<const void**>(&values),
                                       &size);
    if (rc < 0)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to read array of type '%c'", type);
        throw std::system_error(-rc, std::generic_category());
    }

    if (!values || !size)
        return std::vector<T>();
    else
        return std::vector<T>(values, values + (size / sizeof(T)));
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads an array of strings or object paths from the message

 */
static std::vector<std::string> readStringArray(sd_bus_message *msg, char type)
{
    std::vector<std::string> values;
    const char sig[2] = { type, '\0' };

    int rc = sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, sig);
    if (rc < 0)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to enter array of type '%s'", sig);
        throw std::system_error(-rc, std::generic_category());
    }

    const char *str = nullptr;
    while ((rc = sd_bus_message_read_basic(msg, type, &str)) > 0)
    {
        if (str)
            values.emplace_back(str);
    }

    rc = sd_bus_message_exit_container(msg);
    if (rc < 0)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to exit container");
        throw std::system_error(-rc, std::generic_category());
    }

    return values;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads an array of strings or object paths from the message

 */
static std::vector<AI_IPC::UnixFd> readUnixFdArray(sd_bus_message *msg, char type)
{
    std::vector<AI_IPC::UnixFd> values;
    const char sig[2] = { type, '\0' };

    int rc = sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, sig);
    if (rc < 0)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to enter array of type '%s'", sig);
        throw std::system_error(-rc, std::generic_category());
    }

    int fd;
    while ((rc = sd_bus_message_read_basic(msg, type, &fd)) > 0)
    {
        if (fd >= 0)
            values.emplace_back(fd);
    }

    rc = sd_bus_message_exit_container(msg);
    if (rc < 0)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to exit container");
        throw std::system_error(-rc, std::generic_category());
    }

    return values;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads a basic value and returns it.

 */
static AI_IPC::Variant readArray(sd_bus_message *msg, char type)
{
    switch (type)
    {
        case SD_BUS_TYPE_BYTE:
            return readBasicArray<uint8_t>(msg, type);
    //    case SD_BUS_TYPE_INT16:
    //        return readBasicArray<int16_t>(msg, type);
        case SD_BUS_TYPE_UINT16:
            return readBasicArray<uint16_t>(msg, type);
        case SD_BUS_TYPE_INT32:
            return readBasicArray<int32_t>(msg, type);
        case SD_BUS_TYPE_UINT32:
            return readBasicArray<uint32_t>(msg, type);
    //    case SD_BUS_TYPE_INT64:
    //        return readBasicArray<int64_t>(msg, type);
        case SD_BUS_TYPE_UINT64:
            return readBasicArray<uint64_t>(msg, type);

    //    case SD_BUS_TYPE_BOOLEAN:
    //      {
    //            std::vector<int> ints = readBasicArray<int>(msg, type);
    //
    //           std::vector<bool> values;
    //           values.reserve(ints.size());
    //            for (const int &v : ints)
    //                values.emplace_back((v != 0));
    //             return values;
    //         }


        case SD_BUS_TYPE_UNIX_FD:
            {
                return readUnixFdArray(msg, type);
            }

        case SD_BUS_TYPE_STRING:
            {
                return readStringArray(msg, type);
            }

        case SD_BUS_TYPE_OBJECT_PATH:
            {
                std::vector<std::string> strs = readStringArray(msg, type);

                std::vector<DbusObjectPath> values;
                values.reserve(strs.size());
                for (const std::string &s : strs)
                    values.emplace_back(s);
                return values;
            }

        default:
            {
                const char signature[3] = { SD_BUS_TYPE_ARRAY, type, '\0' };
                AI_LOG_WARN("unsupported dbus array with type '%s'", signature);

                sd_bus_message_skip(msg, signature);
                return AI_IPC::Variant();
            }

    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads a dictionary from the message.

 */
static std::map<std::string, DictDataType> readDictionary(sd_bus_message *msg,
                                                          const char *content)
{
    // TODO:

    AI_LOG_ERROR("demarshalling dictionaries is not yet supported");

    std::string sig;
    sig += "a";
    sig += content;

    AI_LOG_ERROR("skipping '%s'", sig.c_str());
    sd_bus_message_skip(msg, sig.c_str());

    return std::map<std::string, DictDataType>();
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the arguments stored in the \a msg sd-bus message object as a list
    of variants.

 */
static AI_IPC::VariantList demarshallArgs(sd_bus_message *msg)
{
    AI_IPC::VariantList args;

    while (!sd_bus_message_at_end(msg, true))
    {
        int rc;
        char type;
        const char *content = nullptr;

        // get the type
        rc = sd_bus_message_peek_type(msg, &type, &content);
        if (rc < 0)
        {
            AI_LOG_WARN("failed to get the dbus arg type");
            break;
        }

        // read the type
        switch (type)
        {
            case SD_BUS_TYPE_BYTE:
                args.emplace_back(readBasicType<uint8_t>(msg, type));
                break;
            case SD_BUS_TYPE_INT16:
                args.emplace_back(readBasicType<int16_t>(msg, type));
                break;
            case SD_BUS_TYPE_UINT16:
                args.emplace_back(readBasicType<uint16_t>(msg, type));
                break;
            case SD_BUS_TYPE_INT32:
                args.emplace_back(readBasicType<int32_t>(msg, type));
                break;
            case SD_BUS_TYPE_UINT32:
                args.emplace_back(readBasicType<uint32_t>(msg, type));
                break;
            case SD_BUS_TYPE_INT64:
                args.emplace_back(readBasicType<int64_t>(msg, type));
                break;
            case SD_BUS_TYPE_UINT64:
                args.emplace_back(readBasicType<uint64_t>(msg, type));
                break;

            case SD_BUS_TYPE_BOOLEAN:
                {
                    int value;
                    sd_bus_message_read_basic(msg, SD_BUS_TYPE_BOOLEAN, &value);
                    args.emplace_back(static_cast<bool>(value));
                }
                break;

            case SD_BUS_TYPE_UNIX_FD:
                {
                    int value;
                    sd_bus_message_read_basic(msg, SD_BUS_TYPE_UNIX_FD, &value);
                    args.emplace_back(UnixFd(value));
                }
                break;
            case SD_BUS_TYPE_STRING:
                {
                    const char *value = nullptr;
                    sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &value);
                    args.emplace_back(std::string(value ? value : ""));
                }
                break;

            case SD_BUS_TYPE_OBJECT_PATH:
                {
                    const char *value = nullptr;
                    sd_bus_message_read_basic(msg, SD_BUS_TYPE_OBJECT_PATH, &value);
                    args.emplace_back(DbusObjectPath(value ? value : ""));
                }
                break;

            case SD_BUS_TYPE_ARRAY:
                if (content)
                {
                    if (content[0] == SD_BUS_TYPE_DICT_ENTRY_BEGIN)
                        args.emplace_back(readDictionary(msg, content));
                    else
                        args.emplace_back(readArray(msg, *content));
                }
                break;

            default:
                {
                    std::string signature;
                    signature += char(type);
                    if (content)
                        signature += content;

                    AI_LOG_WARN("unsupported argument type '%s'", signature.c_str());
                    sd_bus_message_skip(msg, signature.c_str());
                }
                break;
        }
    }

    return args;
}

// -----------------------------------------------------------------------------
/*!
    Returns the arguments stored in the \a msg sd-bus message object as a list
    of variants.

    If there was an error parsing the message then an empty variant list is
    returned.

 */
AI_IPC::VariantList SDBusArguments::demarshallArgs(sd_bus_message *msg)
{
    try
    {
        return ::demarshallArgs(msg);
    }
    catch (const std::exception &e)
    {
        AI_LOG_ERROR("failed to demarshall dbus message (%s)", e.what());
        return AI_IPC::VariantList();
    }
}
