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
   cmake -DCMAKE_TOOLCHAIN_FILE=unit_tests/L1_testing/gcc-with-coverage.cmake  -DRDK_PLATFORM=DEV_VM -DCMAKE_INSTALL_PREFIX:PATH=/usr -DENABLE_DOBBYL1TEST=ON -DCMAKE_BUILD_TYPE=Debug -DPLUGIN_TESTPLUGIN=ON -DPLUGIN_GPU=ON -DPLUGIN_LOCALTIME=ON -DPLUGIN_RTSCHEDULING=ON -DPLUGIN_HTTPPROXY=ON -DPLUGIN_APPSERVICES=ON -DPLUGIN_IONMEMORY=ON -DPLUGIN_DEVICEMAPPER=ON -DPLUGIN_OOMCRASH=ON -DLEGACY_COMPONENTS=ON -DRDK=ON -DUSE_SYSTEMD=ON .. ..
   make -j $(nproc)
```
   ### Run dobby L1 unit test
```command
          sudo $GITHUB_WORKSPACE/build/unit_tests/L1_testing/tests/DobbyTest/DobbyL1Test
          sudo $GITHUB_WORKSPACE/build/unit_tests/L1_testing/tests/DobbyUtilsTest/DobbyUtilsL1Test
          sudo $GITHUB_WORKSPACE/build/unit_tests/L1_testing/tests/DobbyManagerTest/DobbyManagerL1Test
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
          '*/unit_tests/L1_testing/tests/*'
          -o filtered_coverage.info
          &&
          genhtml
          -o coverage
          -t "dobby coverage"
          filtered_coverage.info
```
## Writing tests
To write new test:
1. Create new folder and create new test file under the unit_tests/L1_testing/tests/DobbyXXXXTest/XXXXTest.cpp
2. Create new CMakeLists.txt under this unit_tests/L1_testing/tests/DobbyXXXXTest/ folder
3. Add add_subdirectory(DobbyXXXXTest) in unit_tests/L1_testing/tests/CMakeLists,txt
4. Add the test cases using gtest framework
5. Once added the new test, use above commands to build and run the test.
   Otherwise if add the changes in github, .github/workflows/build.yml will build and run the tests then give the results with coverage report.
