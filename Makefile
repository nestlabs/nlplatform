#
#    Copyright (c) 2015-2018 Nest Labs, Inc.
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

#
#    Description:
#      This file is the Makefile for the Nest platform library
#

include pre.mak

include common.mak

ARCHIVES = nlplatform

nlplatform_SOURCES = $(nlplatform_sources)

nlplatform_INCLUDES = $(nlplatform_includes)

nlplatform_INCLUDES += $(BuildDirectory)

nlplatform_CPPFLAGS         = -Werror

# Headers to install
nlplatform_HEADERS          = include/nlplatform.h

# These headers we want installed under an additional subdir include/nlplatform/*
PlatformIncludeFiles        = nladc.h \
                              nlclock.h \
                              nlconsole.h \
                              nlcrc.h \
                              nlcrypto.h \
                              nlfault.h \
                              nlflash.h \
                              nlflash_spi.h \
                              nlfs.h \
                              nlgpio.h \
                              nlgpio_button.h \
                              nli2c.h \
                              nlmpu.h \
                              nlpartition.h \
                              nlpwm.h \
                              nlreset_info.h \
                              nlrtc.h \
                              nlspi.h \
                              nlspi_ipc.h \
                              nlspi_slave.h \
                              nltimer.h \
                              nluart.h \
                              nlwatchdog.h \
                              nlwatchpoint.h \
                              nlplatform_diags.h \
                              nlprofile.h \
                              nltime.h \
                              nltrace.h \
                              nlradio.h \
                              arch/nlplatform_arm_cm3.h \
                              arch/nlplatform_arm_cm0.h \
                              spi_flash/at45db041e.h \
                              spi_flash/mx25u1635.h \
                              spi_flash/mx25u3235.h \
                              spi_flash/n25q.h \
                              spi_flash/mx25r1635.h \
                              spi_flash/mx25r3235.h \
                              spi_flash/mx25r6435.h \
                              spi_flash/gd25le16c.h \

ifeq ($(BUILD_FEATURE_RAM_CONSOLE),1)
PlatformIncludeFiles        += nlram_console.h
endif

ifeq ($(BUILD_FEATURE_UNIT_TEST),1)
VPATH                       += test
nlplatform_INCLUDES         += test \
                               $(TestFrameworkIncludePaths)
endif

ifeq ($(BUILD_FEATURE_SW_TIMER),1)
nlplatform_SOURCES          += nlswtimer.c
PlatformIncludeFiles        += nlswtimer.h

ifeq ($(BUILD_FEATURE_UNIT_TEST),1)
nlplatform_SOURCES          += nlswtimer-test.c
PlatformIncludeFiles        += nlswtimer-test.h
endif
endif

PlatformIncludePaths = $(foreach headerfile,$(PlatformIncludeFiles),include/$(headerfile):include/$(NlPlatformNames)/$(headerfile))

nlplatform_HEADERS += $(PlatformIncludePaths)

.DEFAULT_GOAL = all

SubMakefiles  = nortos.mak

include post.mak
