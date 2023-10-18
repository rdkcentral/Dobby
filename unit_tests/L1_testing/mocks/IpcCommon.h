#ifndef AI_IPC_IPCCOMMON_H
#define AI_IPC_IPCCOMMON_H

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>

namespace AI_IPC
{

typedef std::vector<int> Variant;
typedef std::vector<Variant> VariantList;

class IAsyncReplySenderApiImpl {
public:
    virtual ~IAsyncReplySenderApiImpl() = default;
    virtual bool sendReply(const VariantList& replyArgs) = 0;
};


  /**
     * @brief Remote entry, which can be either a signal or method
     *
     * A remote entry is identified by its type, object, interface and name. A remote instance is what we
     * call here as object, which can be accessed though its interfaces.
     *
     * Both methods and signals are part of object interfaces.
     */
    struct RemoteEntry
    {
        enum Type
        {
           METHOD,
           SIGNAL
        };

        Type type;
        std::string service;
        std::string object;
        std::string interface;
        std::string name;

        bool isValid() const
        {
            if (object.empty() || interface.empty() || name.empty())
                return false;
            if ((type == METHOD) && service.empty())
                return false;
            return true;
        }

        operator bool() const
        {
            return isValid();
        }

    protected:
        RemoteEntry(Type type_)
            : type(type_) {}

        RemoteEntry(Type type_, const std::string& service_, const std::string& object_, const std::string& interface_, const std::string& name_)
            : type(type_), service(service_), object(object_), interface(interface_), name(name_) {}
    };

    /**
     * @brief Method identified by a service, object, interface and method name itself
     */
    struct Method : public RemoteEntry
    {
        Method()
            : RemoteEntry(METHOD) {}

        Method(const std::string& service_, const std::string& object_, const std::string& interface_, const std::string& name_)
            : RemoteEntry(METHOD, service_, object_, interface_, name_) {}

    };

    /**
     * @brief Method identified by object, interface and signal name itself
     *
     * See dbus signal for details
     */
    struct Signal : public RemoteEntry
    {
        Signal()
            : RemoteEntry(SIGNAL) {}

        Signal(const std::string& object_, const std::string& interface_, const std::string& name_)
            : RemoteEntry(SIGNAL, std::string(), object_, interface_, name_) {}
    };

class IAsyncReplyGetter {
public:
    virtual ~IAsyncReplyGetter() = default;

    /**
     * @brief Get reply.
     *
     * @parameter[out]   argList         This is the return value of the method call
     *
     * @returns On success: TRUE
     * @returns On failure: FALSE
     */
    virtual bool getReply(VariantList& argList) = 0;
};

class IAsyncReplySender {
public:
    static IAsyncReplySender& getInstance()
    {
        static IAsyncReplySender instance;
        return instance;
    }

    IAsyncReplySenderApiImpl* impl;

    static bool sendReply(const VariantList& replyArgs)
    {
        return getInstance().impl->sendReply(replyArgs);
    }
};


/**
 * @brief dbus monitor event types.
 */
typedef enum
{
    MethodCallEvent,
    MethodReturnEvent,
    SignalEvent,
    ErrorEvent,
} EventType;


/**
 * @brief Method call handler
 */
typedef std::function<void(std::shared_ptr<IAsyncReplySender>)> MethodHandler;

/**
 * @brief Signal handler
 */
typedef std::function<void(const VariantList&)> SignalHandler;

/**
 * @brief Monitor handler
 */
typedef std::function<void(EventType,
                           unsigned int,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           const VariantList&)> MonitorHandler;


constexpr auto sendReply = &IAsyncReplySender::sendReply;

} // namespace AI_IPC

#endif /* AI_IPC_IPCCOMMON_H */
