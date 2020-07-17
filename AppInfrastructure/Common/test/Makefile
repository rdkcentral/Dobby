################################################################################
## Purpose   : Makefile for Common tests
## Copyright : 2013+ Sky UK
################################################################################

SKY_ROOT := $(shell cd ../../.. ; pwd)

include ${SKY_ROOT}/AppInfrastructure/MakeExtras/build_tests.mk

ifndef NO_INCLUDE
    include build/component.mk
    include ${SKY_ROOT}/Components/MakeRules/make.mk
endif
