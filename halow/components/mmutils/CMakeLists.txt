# Copyright 2024-2026 Morse Micro
# SPDX-License-Identifier: Apache-2.0

set(ROOT_DIR ${MM_IOT_SDK}/framework/src/mmutils)

set(inc
    ${ROOT_DIR})
set(src
    ${ROOT_DIR}/mmutils_wlan.c
)

if(EDGEZ_RF_ONLY_MODE)
    # The lower driver uses only inline/type declarations from mmutils.h. Do
    # not compile the WLAN/RSN helper implementation into an RF-only image.
    add_library(mmutils INTERFACE)
    target_include_directories(mmutils INTERFACE ${inc})
else()
    add_library(mmutils STATIC ${src})
    add_dependencies(mmutils morselib)
    target_link_libraries(mmutils PUBLIC morselib)
    target_include_directories(mmutils INTERFACE ${inc})
endif()
