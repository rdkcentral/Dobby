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
   cmake -DRDK_PLATFORM=DEV_VM -DCMAKE_TOOLCHAIN_FILE=unit_tests/L1_testing/gcc-with-coverage.cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DENABLE_DOBBYL1TEST=ON -DCMAKE_BUILD_TYPE=Debug -DLEGACY_COMPONENTS=ON -DUSE_SYSTEMD=ON -DPLUGIN_TESTPLUGIN=ON -DPLUGIN_GPU=ON -DPLUGIN_LOCALTIME=ON -DPLUGIN_RTSCHEDULING=ON -DPLUGIN_HTTPPROXY=ON -DPLUGIN_APPSERVICES=ON -DPLUGIN_IONMEMORY=ON -DPLUGIN_DEVICEMAPPER=ON -DPLUGIN_OOMCRASH=ON ..
   make -j $(nproc)
```
   ### Run Run dobby L1 unit test
   sudo unit_tests/L1_testing/DobbyL1Test
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
To write new test you need:
1. Create new test file under the unit_tests/L1_testing/tests/XXXXTest.cpp
2. Add the test cases using gtest framework
3. Once added when you run the above commands, It will compile the new test file and run the test. Finally it will give the coverage report.
   Otherwise if you add your changes in github, .github/workflows/build.yml will build and run the tests then give the results with coverage report.
