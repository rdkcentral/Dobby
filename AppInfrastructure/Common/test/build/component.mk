# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2013 Sky UK
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
## Function     : Build unit tests for Common
################################################################################
ifndef __AppInfraCommonTests_COMPONENT_MK__
__AppInfraCommonTests_COMPONENT_MK__ := DEFINED
################################################################################

AppInfraCommonTests_OPTIONS+=-std=c++0x

########### Set MAIN component #################################################

SKY_MAIN_COMPONENT ?= AppInfraCommonTests

########### Module Path ########################################################

AppInfraCommonTests_PATH := ${SKY_ROOT}/AppInfrastructure/Common/test

########### Module Sources #####################################################

include ${SKY_ROOT}/Components/MakeRules/platform.mk

AppInfraCommonTests_SRC := $(wildcard ${AppInfraCommonTests_PATH}/source/*.cpp)

########### Module Options #####################################################

SKY_OPTIONS += -DDISABLE_TERMINATE_LOG_TO_STDERR=1

########### Module Dependencies ################################################

# boost include makefile can do this BUT it will need to set "VPATH"
AppInfraCommonTests_OPTIONS += -I${SKY_ROOT}/AppInfrastructure/3rdParty/boost/include
AppInfraCommonTests_OPTIONS += -I${SKY_ROOT}/AppInfrastructure/Public

include ${SKY_ROOT}/AppInfrastructure/Common/lib/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/Logger/lib/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/3rdParty/googletest/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/TestInfrastructure/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/3rdParty/boost/build/component.mk
include ${SKY_ROOT}/AppInfrastructure/3rdParty/valgrind/build/component.mk

########### Export Module ######################################################

SKY_COMPONENT := AppInfraCommonTests

include ${SKY_ROOT}/Components/MakeRules/export.mk

linuxX86_tests_invoke: linuxX86_standard_tests_invoke
linuxX64_tests_invoke: linuxX64_standard_tests_invoke

################################################################################
endif # ifndef __AppInfraCommonTests_COMPONENT_MK__
################################################################################
