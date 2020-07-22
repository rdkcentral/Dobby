# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2015 Sky UK
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

################################################################################
## Function     : Build unit tests for IpcService
################################################################################
ifndef __IpcServiceTest_COMPONENT_MK__
__IpcServiceTest_COMPONENT_MK__ := DEFINED
################################################################################

IpcServiceTest_OPTIONS+=-std=c++0x

########### Set MAIN component #################################################

SKY_MAIN_COMPONENT ?= IpcServiceTest

########### Module Path ########################################################

IpcServiceTest_PATH := ${SKY_ROOT}/AppInfrastructure/IpcService/test

########### Module Sources #####################################################

include ${SKY_ROOT}/Components/MakeRules/platform.mk

IpcServiceTest_SRC	:= $(wildcard ${IpcServiceTest_PATH}/source/*.cpp)

########### Module Dependencies ################################################

include ${SKY_ROOT}/AppInfrastructure/3rdParty/dbus/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/IpcService/lib/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/3rdParty/googletest/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/TestInfrastructure/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/PackageManager/lib/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/FakeDMS/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/3rdParty/openssl/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/AppLogger/stub/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/MongooseWrapper/lib/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/3rdParty/mongoose/build/component.mk


IpcServiceTest_OPTIONS += -I${SKY_ROOT}/AppInfrastructure/Public
IpcServiceTest_OPTIONS += -I${SKY_ROOT}/AppInfrastructure/DbusServer/lib/include
IpcServiceTest_OPTIONS += -I${SKY_ROOT}/AppInfrastructure/PackageManager/lib/include
IpcServiceTest_OPTIONS += -I${SKY_ROOT}/AppInfrastructure/FakeDMS/system/include
IpcServiceTest_OPTIONS += -I${SKY_ROOT}/AppInfrastructure/IpcService/lib/source

SKY_EXTRA_BIN_LINK += -lpthread

########### Export Module ######################################################

SKY_COMPONENT := IpcServiceTest

include ${SKY_ROOT}/Components/MakeRules/export.mk

linuxX86_tests_invoke: linuxX86_standard_tests_invoke
linuxX64_tests_invoke: linuxX64_standard_tests_invoke

################################################################################
endif # ifndef __IpcServiceTest_COMPONENT_MK__
################################################################################
