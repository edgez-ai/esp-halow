# Copyright 2026 Morse Micro
# SPDX-License-Identifier: Apache-2.0

set(MM_IOT_SDK ${CMAKE_CURRENT_SOURCE_DIR}/mm-iot-sdk)
set(EDGEZ_RF_ONLY_MODE OFF)
if(CONFIG_DAB_RAW_RF_TEST OR CONFIG_HALOW_MESH_MODE)
    set(EDGEZ_RF_ONLY_MODE ON)
endif()

add_subdirectory(firmware)
add_subdirectory(shims)
add_subdirectory(mmpktmem)
add_subdirectory(mmutils)
add_subdirectory(morselib)
add_subdirectory(hostap)
if(NOT CONFIG_DAB_RAW_RF_TEST)
    add_subdirectory(regdb)
endif()
