#
#    Copyright (c) 2017-2018 Nest Labs, Inc.
#    All rights reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

#
#    Description:
#      This Makefile provides variables that are common bo the standard and "No
#      RTOS" build variants.
#
#      This primarily involves selecting which source files to compile based on
#      the selected BUILD_FEATURE_*s.
#

VPATH = \
    src \
    src/compiler/$(ToolVendor)/$(ToolProduct)

nlplatform_sources = \
    cxx-symbols.c \
    eabi.c \
    nlcrc.c \
    nlfault.c \
    nlflash.c \
    nlgpio_button.c \
    nlstubs.c \
    nltime.c \

ifneq ($(BUILD_FEATURE_NO_SPI_FLASH),1)
nlplatform_sources += nlflash_spi.c nlfs.c
endif

ifeq ($(BUILD_FEATURE_NL_PROFILE),1)
nlplatform_sources += nlprofile.c
endif

ifeq ($(BUILD_FEATURE_NL_TRACE),1)
nlplatform_sources += nltrace.c
endif

ifneq ($(NL_FEATURE_SIMULATEABLE_HW),1)
nlplatform_sources += nlreset_info.c nlwatchpoint.c
endif

ifeq ($(BUILD_FEATURE_RAM_CONSOLE),1)
nlplatform_sources += nlram_console.c
endif

ifeq ($(BUILD_FEATURE_NL_PLATFORM_SPI_IPC),1)
nlplatform_sources += nlspi_ipc.c
endif

ifeq ($(BUILD_FEATURE_SW_TIMER),1)
nlplatform_sources += nlswtimer.c
endif

# Add any cpu specific files
ifneq ($(CpuVendor),)
ifneq ($(CpuName),)

# To reduce forking of fault.c, which is still fairly actively being
# developed and new features being added to regularly that are not
# Cpu dependent, have cortex-m4f share the cortex-m3 copy.
ifeq ($(CpuName), cortex-m4f)
CpuNameLocal = cortex-m3
else
CpuNameLocal = $(CpuName)
endif

VPATH += src/cpu/$(CpuVendor)/$(CpuNameLocal)
nlplatform_sources += \
    fault.c \
    nlmpu.c \

ifeq ($(CpuNameLocal), cortex-m0)
nlplatform_sources += builtin.c
endif
endif
endif

nlplatform_includes = \
    $(NlAssertIncludePaths) \
    $(NlBacktraceIncludePaths) \
    $(NlBinaryUtilsIncludePaths) \
    $(NlBreadcrumbsIncludePaths) \
    $(NlEnvIncludePaths) \
    $(NlERIncludePaths) \
    $(NlPlatformIncludePaths) \
    $(NlPlatformSocIncludePaths) \
    $(NlUtilitiesIncludePaths) \
    $(CMSISIncludePaths) \
    $(FreeRTOSIncludePaths) \

ifeq ($(BUILD_FEATURE_SW_CRC),1)
nlplatform_includes += $(NlCrcIncludePaths)
endif
