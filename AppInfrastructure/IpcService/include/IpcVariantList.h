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
 * IpcVariantList.h
 *
 *  Created on: 
 *      Author: 
 */

#ifndef AI_IPC_IPCVARIANTLIST_H
#define AI_IPC_IPCVARIANTLIST_H

#include "IpcFileDescriptor.h"

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>

// The boost variant class has a variable that triggers the -Wshadow
// gcc warning. However it is benign, therefore we temporary disable
// the warning here
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Waddress"
#if __GNUC__ > 5
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
#include <boost/variant.hpp>
#pragma GCC diagnostic pop


namespace AI_IPC
{

/**
 * @brief Structure to store Unix FD
 */
using UnixFd = IpcFileDescriptor;

/**
 * @brief Structure to store dbus object path
 */
struct DbusObjectPath
{
    bool operator==(const DbusObjectPath& rhs) const { return objectPath == rhs.objectPath; }

    explicit DbusObjectPath(const char* obj) : objectPath(obj) {}
    explicit DbusObjectPath(const std::string& obj)  : objectPath(obj) { }
    DbusObjectPath(const DbusObjectPath& other) : objectPath(other.objectPath) { }

    std::string objectPath;
};

/**
 * @brief Dictionary data type
 *
 * Note: the maximum number of data types supported by a boost vairant is configured through the boost config variable BOOST_VARIANT_LIMIT_TYPES
 *
 *
 *                                [Conventional name] [ASCII]     [type-code Encoding]
 **/
typedef boost::variant<uint8_t,     // BYTE           y(121)     Unsigned 8-bit integer
                       bool,        // BOOLEAN        b(98)      Boolean value: 0 is false, 1 is true, any other value allowed by the marshalling format is invalid
                       int16_t,     // INT16          n(110)     Signed (two's complement) 16-bit integer
                       uint16_t,    // UINT16         q(113)     Unsigned 16-bit integer
                       int32_t,     // INT32          i(105)     Signed (two's complement) 32-bit integer
                       uint32_t,    // UINT32         u(117)     Unsigned 32-bit integer
                       int64_t,     // INT64          x(120)     Signed (two's complement) 64-bit integer (mnemonic: x and t are the first characters in "sixty" not already used for something more common)
                       uint64_t,    // UINT64         t(116)     Unsigned 64-bit integer
                       UnixFd,      // UNIX_FD        h(104)     Unsigned 32-bit integer representing an index into an out-of-band array of file descriptors, transferred via some platform-specific mechanism (mnemonic: h for handle)
                       std::string, // STRING         s(115)     No extra constraints
                       DbusObjectPath   // OBJECT_PATH    o(111)     Must be a syntactically valid object path
                       //double,      // DOUBLE         d(100)     IEEE 754 double-precision floating point
                       > DictDataType;

/**
 * @brief Supported data type
 */
typedef boost::variant<uint8_t,
                       bool,
                       int16_t,
                       uint16_t,
                       int32_t,
                       uint32_t,
                       int64_t,
                       uint64_t,
                       UnixFd,
                       std::string,
                       DbusObjectPath,
                       std::vector< uint8_t >,
                       std::vector< uint16_t >,
                       std::vector< int32_t >,
                       std::vector< uint32_t >,
                       std::vector< uint64_t >,
                       std::vector< UnixFd >,
                       std::vector< DbusObjectPath >,
                       std::vector< std::string >,
                       std::map<std::string, DictDataType>
                     > Variant;

/**
 * @brief Type used for signal and method arguments as well as for method return value.
 */
typedef std::vector<Variant> VariantList;



// Gubbins required for functional variadic templates, see below
struct _pass { template<typename ...T> _pass(T...) {} };

/**
 *  @brief Extracts the args from method call (in a type safe way)
 *
 *  This code may be too clever for it's own good and maybe disappears up it's
 *  own arse ... it uses variadic templates to set the types for all the args
 *  and accepts pointers to said args.  If there is any problem extracting the
 *  args - due to incorrect number of args or the wrong type - then false is
 *  returned.
 *
 *  Variadic templates are pretty messy, I found the following links helpful
 *  for figuring out to iterate over all the args
 *    - https://en.wikipedia.org/wiki/Variadic_template
 *    - http://stackoverflow.com/questions/12030538/calling-a-function-for-each-variadic-template-argument-and-an-array
 *
 *  @param[in]   sender   The sender object that contains the arguments
 *  @param[in]   args...  Pointers to values that will have their value set
 *                        from the argument list
 *
 *  @return true if all the args were read correctly, otherwise false.
 */
template<typename... Ts>
bool parseVariantList(const AI_IPC::VariantList& returns, Ts *...args)
{
    // Check the number of args in the list matches the number of templated args
    const size_t numArgs = sizeof...(args);
    if (returns.size() != numArgs)
    {
        // AI_LOG_ERROR("invalid number of args (expected:%zu, actual:%zu)",
        //              numArgs, returns.size());
        return false;
    }

    // Get an iterator and point it to the first arg
    AI_IPC::VariantList::const_iterator it = returns.begin();

    // tbh I don't really know how the following works, but the upshot is that
    // processVal() will get called for each argument in order
    bool result = true;
    int unused[]{(processVal(&result, it++, args), 1)...};
    _pass{unused};

    return result;
}

template <typename T>
void processVal(bool *result, const AI_IPC::VariantList::const_iterator & it, T * arg);



} // namespace AI_IPC

#endif // AI_IPC_IPCVARIANTLIST_H

