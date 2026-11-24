cmake_minimum_required(VERSION 3.7)
project(StorageHelperL1Test)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME}
        StorageHelperTest.cpp
        ../../../../rdkPlugins/Storage/source/StorageHelper.cpp
        ../../../../AppInfrastructure/Logging/source/Logging.cpp
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ../../../../rdkPlugins/Storage/source
        ../../../../rdkPlugins/Common/include
        ../../../../AppInfrastructure/Common/include
        ../../../../AppInfrastructure/Logging/include
)

# Keep only the helper methods exercised here; StorageHelper.cpp also contains
# privileged loop-device and filesystem-image operations that are not needed by
# this L1 test.
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,--gc-sections")
target_link_libraries(${PROJECT_NAME} GTest::GTest GTest::Main pthread)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
