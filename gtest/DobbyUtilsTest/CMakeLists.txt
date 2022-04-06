cmake_minimum_required(VERSION 3.8)
project(DobbyUtilsTest)

set(CMAKE_CXX_STANDARD 11)

include(CTest)
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_library(Utils SHARED
            STATIC
            ../../utils/source/DobbyUtils.cpp
            ../../utils/source/ContainerId.cpp
            ../../utils/source/DobbyTimer.cpp
            ../../AppInfrastructure/Logging/source/Logging.cpp
            )

target_include_directories(Utils
                PUBLIC
                ../../utils/include
                ../../utils/source
                ../../AppInfrastructure/Logging/include
                ../../AppInfrastructure/Common/include
                )

file(GLOB TESTS tests/*.cpp)

add_executable(utilsTest ${TESTS})
add_test(NAME AllTestsInUtils COMMAND utilsTest)

target_link_libraries(utilsTest Utils ${GTEST_LIBRARIES} pthread)

install(TARGETS utilsTest DESTINATION bin)
