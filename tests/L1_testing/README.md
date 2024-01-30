# Dobby L1 Test
Main objective for L1_testing is checking the api’s functionality in Dobby component

## Environment Setup
Dobby test runs on Ubuntu

### Required Packages
Below Packages need to be install
automake libtool autotools-dev software-properties-common build-essential cmake libsystemd-dev libctemplate-dev libjsoncpp-dev
libdbus-1-dev libnl-3-dev libnl-route-3-dev libsystemd-dev libyajl-dev libcap-dev libboost-dev libgtest-dev lcov clang

## Running tests
To Running the tests, need to be build the Dobby source and Test codes.
1. Get the Dobby Repo then follow below steps
```command
   cd Dobby
   mkdir build
   cd build
```
   ### Build all optional plugins. Newly developed plugins should be added to this list
```command
   cmake -DCMAKE_TOOLCHAIN_FILE=tests/L1_testing/gcc-with-coverage.cmake  -DRDK_PLATFORM=DEV_VM -DCMAKE_INSTALL_PREFIX:PATH=/usr -DENABLE_DOBBYL1TEST=ON -DCMAKE_BUILD_TYPE=Debug -DPLUGIN_TESTPLUGIN=ON -DPLUGIN_GPU=ON -DPLUGIN_LOCALTIME=ON -DPLUGIN_RTSCHEDULING=ON -DPLUGIN_HTTPPROXY=ON -DPLUGIN_APPSERVICES=ON -DPLUGIN_IONMEMORY=ON -DPLUGIN_DEVICEMAPPER=ON -DPLUGIN_OOMCRASH=ON -DLEGACY_COMPONENTS=ON -DRDK=ON -DUSE_SYSTEMD=ON .. ..
   make -j $(nproc)
```
   ### Run dobby L1test
```command
          sudo $GITHUB_WORKSPACE/build/tests/L1_testing/tests/DobbyTest/DobbyL1Test
          sudo $GITHUB_WORKSPACE/build/tests/L1_testing/tests/DobbyUtilsTest/DobbyUtilsL1Test
          sudo $GITHUB_WORKSPACE/build/tests/L1_testing/tests/DobbyManagerTest/DobbyManagerL1Test
```
```command
   ###If want coverage report, run the below command
   lcov -c
          -o coverage.info
          -d $GITHUB_WORKSPACE
          &&
          lcov
          -r coverage.info
          '11/*'
          '/usr/include/*'
          '*/tests/L1_testing/tests/*'
          -o filtered_coverage.info
          &&
          genhtml
          -o coverage
          -t "dobby coverage"
          filtered_coverage.info
```
## Writing tests
### To write new test:
1. Create new folder and create new test file under the tests/L1_testing/tests/DobbyXXXXTest/XXXXTest.cpp
2. Create new CMakeLists.txt under this tests/L1_testing/tests/DobbyXXXXTest/ folder
3. Add add_subdirectory(DobbyXXXXTest) in tests/L1_testing/tests/CMakeLists,txt
4. Add the test cases using gtest framework
### To write the mock
1. Instead of incuding actual header file. Create a new heade file with same name in tests/L1_testing/mocks/XXXX.h
    #### Example:
     If test requires DobbyContainer.h header file.
     - Create [DobbyContainer.h](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/DobbyContainer.h) file in [tests/L1_testing/mocks/](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/)
     - Add the [DobbyContainer class](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/DobbyContainer.h#L44) and required function methods.
     - Create new class [DobbyContainerImpl](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/DobbyContainer.h#L35) in this file and declared all methods as virtual
     - Create an object for this Dobby container mock class and store it in [static member varaible(impl)](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/DobbyContainer.h#L48) of DobbyContainer
     - Create [DobbyContainerMock.h](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/DobbyContainerMock.h#L26) for creating the mock functions using gmock (MOCK_METHOD). so gmock will create the definition for those methods
     - [From test file](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/tests/DobbyTest/DaemonDobbyTests.cpp#L114) need to create the object for mock and assign that value to impl member variable.
     - Create [DobbyContainerMock.cpp](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/DobbyContainerMock.cpp) and add definition for DobbyContainer class. Here need to call DobbyContainerImpl class methods using [impl](https://github.com/rdkcentral/Dobby/blob/master/tests/L1_testing/mocks/DobbyContainerMock.cpp#L76) member variable

Once added the new test, use above commands to build and run the test.
   Otherwise if add the changes in github, .github/workflows/build.yml will build and run the tests then give the results with coverage report.

