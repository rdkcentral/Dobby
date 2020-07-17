################################################################################
## Function     : Build unit tests for IpcService
## Copyright    : Sky UK 2015
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
