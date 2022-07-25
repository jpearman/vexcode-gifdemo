
# macros to help with windows paths with spaces
sp :=
sp +=
qs = $(subst ?,$(sp),$1)
sq = $(subst $(sp),?,$1)

# default platform and build location
PLATFORM  = vexv5
BUILD     = build

# Project name passed from app
ifeq ("$(origin P)", "command line")
PROJECT   = $(P)
else
PROJECT   = $(notdir $(call sq,$(abspath ${CURDIR})))
endif

# Toolchain path passed from app
ifeq ("$(origin T)", "command line")
TOOLCHAIN = $(T)
endif
ifndef TOOLCHAIN
TOOLCHAIN = ${HOME}/sdk
endif

# Run verbose
ifeq ("$(origin V)", "command line")
BUILD_VERBOSE=$(V)
endif

ifndef BUILD_VERBOSE
BUILD_VERBOSE = 0
endif

ifeq ($(BUILD_VERBOSE),0)
Q = @
else
Q =
endif

# compile and link tools
CC      = clang
CXX     = clang
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
LINK    = arm-none-eabi-ld
ECHO    = @echo
DEFINES = -DVexV5

# mkdir different on windows
ifeq ($(OS),Windows_NT)
$(info windows build for platform $(PLATFORM))
MKDIR_P = mkdir
COPY = copy
else
$(info unix build for platform $(PLATFORM))
MKDIR_P = mkdir -p
COPY = cp
endif

TOOL_INC  = -I"$(TOOLCHAIN)/$(PLATFORM)/clang/7.0.0/include" -I"$(TOOLCHAIN)/$(PLATFORM)/gcc/include"  -I"$(TOOLCHAIN)/$(PLATFORM)/gcc/include/c++/4.9.3"  -I"$(TOOLCHAIN)/$(PLATFORM)/gcc/include/c++/4.9.3/arm-none-eabi/armv7-ar/thumb"
TOOL_LIB  = -L"$(TOOLCHAIN)/$(PLATFORM)/gcc/libs"

CFLAGS_CL = -target thumbv7-none-eabi -fshort-enums -Wno-unknown-attributes -U__INT32_TYPE__ -U__UINT32_TYPE__ -D__INT32_TYPE__=long -D__UINT32_TYPE__='unsigned long' 
CFLAGS_V7 = -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS    = ${CFLAGS_CL} ${CFLAGS_V7} -Os -Wall -Werror=return-type -ansi -std=gnu99 $(DEFINES)
CXX_FLAGS = ${CFLAGS_CL} ${CFLAGS_V7} -Os -Wall -Werror=return-type -fno-rtti -fno-threadsafe-statics -fno-exceptions  -std=gnu++11 -ffunction-sections -fdata-sections $(DEFINES)
LNK_FLAGS = --defsym _HEAP_SIZE=0x800000 -nostdlib -T "$(TOOLCHAIN)/$(PLATFORM)/lscript.ld" -R "$(TOOLCHAIN)/$(PLATFORM)/stdlib_0.lib" -Map="$(BUILD)/$(PROJECT).map" --gc-section -L"$(TOOLCHAIN)/$(PLATFORM)" ${TOOL_LIB}

LIBS =  --start-group -lv5rt -lstdc++ -lc -lm -lgcc --end-group

INC += $(addprefix -I, ${INC_F})
INC += -I"$(TOOLCHAIN)/$(PLATFORM)/include"
INC += ${TOOL_INC}
