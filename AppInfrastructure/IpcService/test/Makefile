################################################################################
## Purpose   : Makefile for IpcService tests
## Copyright : 2015+ Sky UK
################################################################################

SKY_ROOT := $(shell cd ../../.. ; pwd)

include ${SKY_ROOT}/AppInfrastructure/MakeExtras/build_tests.mk

#COMPONENT_PREBUILT_SHARED_SUBLIBS+=boost_system

ifndef NO_INCLUDE
    include build/component.mk
    include ${SKY_ROOT}/Components/MakeRules/make.mk
endif
