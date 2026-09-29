/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 Sky UK
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

/**
 * @file DobbyProxyTests.cpp
 *
 * L1 Unit tests for RDKEMW-21185: Add exitcode on container stopped event.
 *
 * Covers:
 *  - registerListenerWithStatus / unregisterListenerWithStatus lifecycle
 *  - onContainerStoppedWithStatusEvent: correct exit-code extraction via
 *    WIFEXITED / WEXITSTATUS, and graceful handling of malformed args
 *  - containerStateChangeThread dispatch: status listeners receive exit code
 *    for ContainerStoppedWithStatus events; standard listeners do NOT
 *  - Standard ContainerStopped events go to standard listeners only
 */

#include "IIpcServiceMock.h"
// Expose private members so we can call onContainerStoppedWithStatusEvent()
// and inspect internal fields directly — the same technique used in
// DaemonDobbyTests.cpp.
#define private public
#include "DobbyProxy.h"
#undef private

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/wait.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// Static impl pointer definitions required by the IIpcService, IAsyncReplySender
// and IpcFileDescriptor shims used across the mock TUs linked into this test.
// ---------------------------------------------------------------------------
AI_IPC::IIpcServiceImpl*         AI_IPC::IIpcService::impl         = nullptr;
AI_IPC::IAsyncReplySenderApiImpl* AI_IPC::IAsyncReplySender::impl  = nullptr;
AI_IPC::IpcFileDescriptorApiImpl* AI_IPC::IpcFileDescriptor::impl  = nullptr;

// ---------------------------------------------------------------------------
// Helper: build an AI_IPC::VariantList from (int32, string, int32)
// ---------------------------------------------------------------------------
static AI_IPC::VariantList makeArgs(int32_t descriptor,
                                    const std::string& id,
                                    int32_t rawStatus)
{
    return AI_IPC::VariantList{
        AI_IPC::Variant(descriptor),
        AI_IPC::Variant(id),
        AI_IPC::Variant(rawStatus)
    };
}

// Build args with only 2 items (missing rawStatus) to trigger parse failure
static AI_IPC::VariantList makeIncompleteArgs(int32_t descriptor,
                                              const std::string& id)
{
    return AI_IPC::VariantList{
        AI_IPC::Variant(descriptor),
        AI_IPC::Variant(id)
    };
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class DobbyProxyTest : public ::testing::Test
{
protected:
    AI_IPC::IpcServiceMock* mIpcServiceMock = nullptr;
    AI_IPC::IIpcService*    mIpcService     = nullptr;
    std::unique_ptr<DobbyProxy> mProxy;

    void SetUp() override
    {
        mIpcServiceMock = new AI_IPC::IpcServiceMock();
        AI_IPC::IIpcService::setImpl(mIpcServiceMock);
        mIpcService = new AI_IPC::IIpcService();

        // DobbyProxy constructor registers several signal handlers and checks
        // that none of the returned registration IDs are empty. Return a
        // non-empty token for every call so the error branch is not hit.
        ON_CALL(*mIpcServiceMock, registerSignalHandler(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return("reg-token"));

        // flush() is called in the destructor — allow it unconditionally.
        ON_CALL(*mIpcServiceMock, flush())
            .WillByDefault(::testing::Return());

        // unregisterHandler() is called in the destructor for each registered
        // signal.  Allow them all.
        ON_CALL(*mIpcServiceMock, unregisterHandler(::testing::_))
            .WillByDefault(::testing::Return(true));

        // isServiceAvailable() is called by isAlive() — default to true so
        // tests that don't exercise isAlive() don't block.
        ON_CALL(*mIpcServiceMock, isServiceAvailable(::testing::_))
            .WillByDefault(::testing::Return(true));

        mProxy = std::make_unique<DobbyProxy>(
            std::shared_ptr<AI_IPC::IIpcService>(mIpcService, [](auto*){}),
            "org.rdk.dobby",
            "/org/rdk/dobby/ctrl1");
    }

    void TearDown() override
    {
        mProxy.reset();
        // IIpcService is owned by the shared_ptr with a no-op deleter above;
        // delete it manually.
        delete mIpcService;
        AI_IPC::IIpcService::setImpl(nullptr);
        delete mIpcServiceMock;
    }
};

// ===========================================================================
// registerListenerWithStatus
// ===========================================================================

/**
 * @test registerListenerWithStatus_ReturnsNonNegativeId
 *
 * Registering a valid status listener must return an ID >= 0.
 */
TEST_F(DobbyProxyTest, registerListenerWithStatus_ReturnsNonNegativeId)
{
    IDobbyProxy::StateChangeWithStatusListener listener =
        [](int32_t, const std::string&, IDobbyProxyEvents::ContainerState,
           int32_t, const void*) {};

    int id = mProxy->registerListenerWithStatus(listener, nullptr);
    EXPECT_GE(id, 0);

    mProxy->unregisterListenerWithStatus(id);
}

/**
 * @test registerListenerWithStatus_ReturnsUniqueIds
 *
 * Two successive registrations must receive different IDs.
 */
TEST_F(DobbyProxyTest, registerListenerWithStatus_ReturnsUniqueIds)
{
    IDobbyProxy::StateChangeWithStatusListener listener =
        [](int32_t, const std::string&, IDobbyProxyEvents::ContainerState,
           int32_t, const void*) {};

    int id1 = mProxy->registerListenerWithStatus(listener, nullptr);
    int id2 = mProxy->registerListenerWithStatus(listener, nullptr);

    EXPECT_GE(id1, 0);
    EXPECT_GE(id2, 0);
    EXPECT_NE(id1, id2);

    mProxy->unregisterListenerWithStatus(id1);
    mProxy->unregisterListenerWithStatus(id2);
}

// ===========================================================================
// unregisterListenerWithStatus
// ===========================================================================

/**
 * @test unregisterListenerWithStatus_RemovesListener
 *
 * After unregistering, the listener must not be called when a
 * ContainerStoppedWithStatus event is dispatched.
 */
TEST_F(DobbyProxyTest, unregisterListenerWithStatus_RemovesListener)
{
    std::atomic<int> callCount{0};

    IDobbyProxy::StateChangeWithStatusListener listener =
        [&callCount](int32_t, const std::string&,
                     IDobbyProxyEvents::ContainerState, int32_t,
                     const void*) { ++callCount; };

    int id = mProxy->registerListenerWithStatus(listener, nullptr);
    ASSERT_GE(id, 0);
    mProxy->unregisterListenerWithStatus(id);

    // Fire a StoppedWithStatus event directly
    int rawStatus = W_EXITCODE(42, 0);   // WIFEXITED true, WEXITSTATUS 42
    mProxy->onContainerStoppedWithStatusEvent(makeArgs(10, "myapp", rawStatus));

    // Give the state-change thread a moment to drain the queue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(callCount.load(), 0)
        << "Listener should not be called after unregister";
}

/**
 * @test unregisterListenerWithStatus_InvalidId_DoesNotCrash
 *
 * Calling unregisterListenerWithStatus with an ID that was never registered
 * must not crash (the implementation logs an error and returns).
 */
TEST_F(DobbyProxyTest, unregisterListenerWithStatus_InvalidId_DoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(mProxy->unregisterListenerWithStatus(9999));
}

// ===========================================================================
// onContainerStoppedWithStatusEvent — argument parsing
// ===========================================================================

/**
 * @test onContainerStoppedWithStatusEvent_NormalExit_ExitCodeExtracted
 *
 * When the raw waitpid status represents a normal exit (WIFEXITED) with
 * exit code N, the listener must receive exitCode == N.
 */
TEST_F(DobbyProxyTest, onContainerStoppedWithStatusEvent_NormalExit_ExitCodeExtracted)
{
    constexpr int32_t kDescriptor = 7;
    const std::string kName       = "youtube";
    constexpr int     kExitValue  = 42;
    int32_t rawStatus             = W_EXITCODE(kExitValue, 0); // normal exit

    std::atomic<bool> called{false};
    int32_t capturedExitCode      = -999;
    int32_t capturedDescriptor    = -1;
    std::string capturedName;

    IDobbyProxy::StateChangeWithStatusListener listener =
        [&](int32_t desc, const std::string& name,
            IDobbyProxyEvents::ContainerState,
            int32_t exitCode, const void*)
        {
            capturedDescriptor = desc;
            capturedName       = name;
            capturedExitCode   = exitCode;
            called             = true;
        };

    int id = mProxy->registerListenerWithStatus(listener, nullptr);
    ASSERT_GE(id, 0);

    mProxy->onContainerStoppedWithStatusEvent(
        makeArgs(kDescriptor, kName, rawStatus));

    // Wait up to 1 s for the background thread to dispatch
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!called && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    ASSERT_TRUE(called) << "Status listener was not called within timeout";
    EXPECT_EQ(capturedDescriptor, kDescriptor);
    EXPECT_EQ(capturedName,       kName);
    EXPECT_EQ(capturedExitCode,   kExitValue);

    mProxy->unregisterListenerWithStatus(id);
}

/**
 * @test onContainerStoppedWithStatusEvent_SignaledProcess_ExitCodeIsMinusOne
 *
 * When the container was killed by a signal (WIFEXITED is false), the
 * listener must receive exitCode == -1.
 */
TEST_F(DobbyProxyTest, onContainerStoppedWithStatusEvent_SignaledProcess_ExitCodeIsMinusOne)
{
    // Simulate a process killed by SIGKILL (signal 9, no core dump)
    int32_t rawStatus = W_EXITCODE(0, SIGKILL); // WIFSIGNALED true, WIFEXITED false

    std::atomic<bool> called{false};
    int32_t capturedExitCode = -999;

    IDobbyProxy::StateChangeWithStatusListener listener =
        [&](int32_t, const std::string&,
            IDobbyProxyEvents::ContainerState, int32_t exitCode, const void*)
        {
            capturedExitCode = exitCode;
            called           = true;
        };

    int id = mProxy->registerListenerWithStatus(listener, nullptr);
    ASSERT_GE(id, 0);

    mProxy->onContainerStoppedWithStatusEvent(makeArgs(1, "container1", rawStatus));

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!called && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    ASSERT_TRUE(called);
    EXPECT_EQ(capturedExitCode, -1)
        << "Signaled container must report exitCode == -1";

    mProxy->unregisterListenerWithStatus(id);
}

/**
 * @test onContainerStoppedWithStatusEvent_BadArgs_ListenerNotCalled
 *
 * When the signal arrives with fewer than 3 arguments (malformed), the
 * implementation must log an error and not enqueue any event, so status
 * listeners must not be called.
 */
TEST_F(DobbyProxyTest, onContainerStoppedWithStatusEvent_BadArgs_ListenerNotCalled)
{
    std::atomic<int> callCount{0};

    IDobbyProxy::StateChangeWithStatusListener listener =
        [&callCount](int32_t, const std::string&,
                     IDobbyProxyEvents::ContainerState, int32_t,
                     const void*) { ++callCount; };

    int id = mProxy->registerListenerWithStatus(listener, nullptr);
    ASSERT_GE(id, 0);

    // Only 2 args — parseVariantList<int32_t,string,int32_t> should fail
    mProxy->onContainerStoppedWithStatusEvent(makeIncompleteArgs(5, "broken"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(callCount.load(), 0)
        << "Listener must not be called when args are malformed";

    mProxy->unregisterListenerWithStatus(id);
}

// ===========================================================================
// Dispatch isolation: ContainerStopped vs ContainerStoppedWithStatus
// ===========================================================================

/**
 * @test standardStopped_DoesNotCallStatusListeners
 *
 * A plain Stopped event (onContainerStoppedEvent) must dispatch only to
 * standard StateChangeListeners, not to StateChangeWithStatusListeners.
 */
TEST_F(DobbyProxyTest, standardStopped_DoesNotCallStatusListeners)
{
    std::atomic<int> statusCallCount{0};

    IDobbyProxy::StateChangeWithStatusListener statusListener =
        [&statusCallCount](int32_t, const std::string&,
                           IDobbyProxyEvents::ContainerState, int32_t,
                           const void*) { ++statusCallCount; };

    int id = mProxy->registerListenerWithStatus(statusListener, nullptr);
    ASSERT_GE(id, 0);

    // Fire a plain Stopped event (2-arg signal, no exit code)
    AI_IPC::VariantList stoppedArgs{
        AI_IPC::Variant(static_cast<int32_t>(3)),
        AI_IPC::Variant(std::string("myapp"))
    };
    mProxy->onContainerStoppedEvent(stoppedArgs);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(statusCallCount.load(), 0)
        << "Status listeners must not fire for plain Stopped events";

    mProxy->unregisterListenerWithStatus(id);
}

/**
 * @test stoppedWithStatus_DoesNotCallStandardListeners
 *
 * A StoppedWithStatus event must not call standard StateChangeListeners.
 */
TEST_F(DobbyProxyTest, stoppedWithStatus_DoesNotCallStandardListeners)
{
    std::atomic<int> standardCallCount{0};

    IDobbyProxy::StateChangeListener standardListener =
        [&standardCallCount](int32_t, const std::string&,
                             IDobbyProxyEvents::ContainerState,
                             const void*) { ++standardCallCount; };

    int id = mProxy->registerListener(standardListener, nullptr);
    ASSERT_GE(id, 0);

    int32_t rawStatus = W_EXITCODE(0, 0); // normal exit, code 0
    mProxy->onContainerStoppedWithStatusEvent(makeArgs(5, "target", rawStatus));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(standardCallCount.load(), 0)
        << "Standard listeners must not fire for StoppedWithStatus events";

    mProxy->unregisterListener(id);
}

/**
 * @test stoppedWithStatus_MultipleStatusListeners_AllCalled
 *
 * All registered status listeners must be invoked for a single
 * ContainerStoppedWithStatus event.
 */
TEST_F(DobbyProxyTest, stoppedWithStatus_MultipleStatusListeners_AllCalled)
{
    std::atomic<int> count1{0}, count2{0};

    IDobbyProxy::StateChangeWithStatusListener l1 =
        [&count1](int32_t, const std::string&,
                  IDobbyProxyEvents::ContainerState, int32_t,
                  const void*) { ++count1; };

    IDobbyProxy::StateChangeWithStatusListener l2 =
        [&count2](int32_t, const std::string&,
                  IDobbyProxyEvents::ContainerState, int32_t,
                  const void*) { ++count2; };

    int id1 = mProxy->registerListenerWithStatus(l1, nullptr);
    int id2 = mProxy->registerListenerWithStatus(l2, nullptr);
    ASSERT_GE(id1, 0);
    ASSERT_GE(id2, 0);

    int32_t rawStatus = W_EXITCODE(1, 0);
    mProxy->onContainerStoppedWithStatusEvent(makeArgs(9, "dual", rawStatus));

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while ((count1 == 0 || count2 == 0) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 1);

    mProxy->unregisterListenerWithStatus(id1);
    mProxy->unregisterListenerWithStatus(id2);
}

/**
 * @test stoppedWithStatus_CbParams_ForwardedToListener
 *
 * The cbParams pointer registered with registerListenerWithStatus must be
 * forwarded unchanged to each listener invocation.
 */
TEST_F(DobbyProxyTest, stoppedWithStatus_CbParams_ForwardedToListener)
{
    int sentinel = 0xDEAD;
    const void* capturedParams = nullptr;
    std::atomic<bool> called{false};

    IDobbyProxy::StateChangeWithStatusListener listener =
        [&](int32_t, const std::string&,
            IDobbyProxyEvents::ContainerState, int32_t,
            const void* params)
        {
            capturedParams = params;
            called         = true;
        };

    int id = mProxy->registerListenerWithStatus(listener, &sentinel);
    ASSERT_GE(id, 0);

    int32_t rawStatus = W_EXITCODE(0, 0);
    mProxy->onContainerStoppedWithStatusEvent(makeArgs(2, "paramtest", rawStatus));

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!called && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    ASSERT_TRUE(called);
    EXPECT_EQ(capturedParams, static_cast<const void*>(&sentinel));

    mProxy->unregisterListenerWithStatus(id);
}
