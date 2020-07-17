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
#include <TerminateHandler.h>
#include <Logging.h>

#include <pthread.h>

#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


class TerminateHandlerTest : public ::testing::Test
{
public:
    void SetUp()
    {
        const AICommon::diag_printer_t diagPrinter =
            std::bind(&TerminateHandlerTest::diagLogger,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      std::placeholders::_3,
                      std::placeholders::_4,
                      std::placeholders::_5);

        setenv("AI_LOG_LEVEL", "debug", 1);
        setenv("AI_LOG_CHANNELS", "d", 1);

        AICommon::initLogging(diagPrinter);

        pthread_setname_np(pthread_self(), "TestTermHandler");
    }

    void TearDown()
    {
        AICommon::termLogging();
    }

public:
    static void diagLogger(int level, const char *file, const char *func,
                           int line, const char *message)
    {
        const char *file_ = strrchr(file, '/');
        if (file_) file_++;
        else       file_ = file;

        // diag printer installed to log to stderr, the format of this message
        // should match what is in the regex of the death tests
        // [ref: https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md]
        fprintf(stderr, "<< DIAG|%d|%s|%s|%d|%s >>", level, file_, func, line, message);
        fflush(stderr);
    }

};



void* f1(void*)
{
    std::set_terminate(AICommon::TerminateHandler);
    throw std::range_error("Monkeys did it");
}

/*TEST_F(TerminateHandlerTest, uncaughtExceptionTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    ASSERT_DEATH(
        {
            pthread_t t;
            pthread_create(&t, NULL, f1, NULL);
            pthread_join(t, NULL);
        },
        R"REGEX(<< DIAG\|0\|TerminateHandler\.cpp\|TerminateHandler\|[0-9]+\|terminate called in thread 'TestTermHandler' after throwing an instance of 'std::range_error' >>)REGEX"
        R"REGEX(<< DIAG\|0\|TerminateHandler\.cpp\|TerminateHandler\|[0-9]+\|  what\(\):  'Monkeys did it' >>)REGEX"
    );
}*/


void* f2(void*)
{
    std::set_terminate(AICommon::TerminateHandler);
    std::terminate();
}

TEST_F(TerminateHandlerTest, callTerminateTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    ASSERT_DEATH(
        {
            pthread_t t;
            pthread_create(&t, NULL, f2, NULL);
            pthread_join(t, NULL);
        },
        R"REGEX(<< DIAG\|0\|TerminateHandler\.cpp\|TerminateHandler\|[0-9]+\|terminate called in thread 'TestTermHandler' without an active exception >>)REGEX"
    );
}


void f3()
{
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

TEST_F(TerminateHandlerTest, destructRunningThreadTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    ASSERT_DEATH(
        {
            std::set_terminate(AICommon::TerminateHandler);
            if (1)
            {
                std::thread t = std::thread(f3);
            }
            ;
        },
        R"REGEX(<< DIAG\|0\|TerminateHandler\.cpp\|TerminateHandler\|[0-9]+\|terminate called in thread 'TestTermHandler' without an active exception >>)REGEX"
    );
}

