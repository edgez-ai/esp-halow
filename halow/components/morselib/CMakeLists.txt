# Copyright 2025-2026 Morse Micro
# SPDX-License-Identifier: Apache-2.0

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(ROOT_DIR ${MM_IOT_SDK}/framework/morselib)

set(INC
    ${ROOT_DIR}/include
    ${ROOT_DIR}/src
    ${ROOT_DIR}/src/internal
    ${ROOT_DIR}/src/umac/rc/mmrc_osal
    ${ROOT_DIR}/mmrc/src/core
)
set(SRCS
    ${ROOT_DIR}/mmrc/src/core/mmrc.c
    ${ROOT_DIR}/src/common/consbuf.c
    ${ROOT_DIR}/src/common/common.c
    ${ROOT_DIR}/src/common/mac_address.c
    ${ROOT_DIR}/src/common/mmpkt.c
    ${ROOT_DIR}/src/common/mmpkt_list.c
    ${ROOT_DIR}/src/dot11/dot11_utils.c
    ${ROOT_DIR}/src/umac/ate/umac_ate.c
    ${ROOT_DIR}/src/umac/ba/umac_ba.c
    ${ROOT_DIR}/src/umac/connection/umac_connection.c
    ${ROOT_DIR}/src/umac/config/umac_config.c
    ${ROOT_DIR}/src/umac/core/umac_core.c
    ${ROOT_DIR}/src/umac/core/umac_evtloop.c
    ${ROOT_DIR}/src/umac/core/umac_evtq.c
    ${ROOT_DIR}/src/umac/core/umac_timeout.c
    ${ROOT_DIR}/src/umac/data/umac_data.c
    ${ROOT_DIR}/src/umac/datapath/umac_datapath_sta.c
    ${ROOT_DIR}/src/umac/data/umac_sta_data.c
    ${ROOT_DIR}/src/umac/datapath/datapath_defrag.c
    ${ROOT_DIR}/src/umac/datapath/umac_datapath.c
    ${ROOT_DIR}/src/umac/frames/action.c
    ${ROOT_DIR}/src/umac/frames/association.c
    ${ROOT_DIR}/src/umac/frames/authentication.c
    ${ROOT_DIR}/src/umac/frames/deauthentication.c
    ${ROOT_DIR}/src/umac/frames/disassociation.c
    ${ROOT_DIR}/src/umac/frames/frame_constructor.c
    ${ROOT_DIR}/src/umac/frames/probe_request.c
    ${ROOT_DIR}/src/umac/frames/probe_response.c
    ${ROOT_DIR}/src/umac/frames/utils.c
    ${ROOT_DIR}/src/umac/health_check/umac_health_check.c
    ${ROOT_DIR}/src/umac/ies/aid_request.c
    ${ROOT_DIR}/src/umac/ies/twt_ie.c
    ${ROOT_DIR}/src/umac/ies/s1g_capabilities.c
    ${ROOT_DIR}/src/umac/ies/s1g_operation.c
    ${ROOT_DIR}/src/umac/ies/s1g_tim.c
    ${ROOT_DIR}/src/umac/ies/ies_common.c
    ${ROOT_DIR}/src/umac/ies/vendor_ie.c
    ${ROOT_DIR}/src/umac/ies/wmm.c
    ${ROOT_DIR}/src/umac/ies/morse_ie.c
    ${ROOT_DIR}/src/umac/interface/umac_interface.c
    ${ROOT_DIR}/src/umac/keys/connection_keys.c
    ${ROOT_DIR}/src/umac/keys/umac_keys.c
    ${ROOT_DIR}/src/umac/ps/umac_ps.c
    ${ROOT_DIR}/src/umac/rc/mmrc_osal/mmrc_osal.c
    ${ROOT_DIR}/src/umac/rc/umac_rc.c
    ${ROOT_DIR}/src/umac/regdb/umac_regdb.c
    ${ROOT_DIR}/src/umac/scan/umac_scan.c
    ${ROOT_DIR}/src/umac/scan/hw_scan.c
    ${ROOT_DIR}/src/umac/stats/umac_stats.c
    ${ROOT_DIR}/src/umac/supplicant_shim/bip.c
    ${ROOT_DIR}/src/umac/supplicant_shim/ccmp.c
    ${ROOT_DIR}/src/umac/supplicant_shim/config.c
    ${ROOT_DIR}/src/umac/supplicant_shim/driver.c
    ${ROOT_DIR}/src/umac/supplicant_shim/eloop.c
    ${ROOT_DIR}/src/umac/supplicant_shim/morse_dpp_event.c
    ${ROOT_DIR}/src/umac/supplicant_shim/random.c
    ${ROOT_DIR}/src/umac/supplicant_shim/supplicant_core.c
    ${ROOT_DIR}/src/umac/twt/umac_twt.c
    ${ROOT_DIR}/src/umac/umac.c
    ${ROOT_DIR}/src/umac/umac_mmdrv_shim.c
    ${ROOT_DIR}/src/umac/wnm_sleep/umac_wnm_sleep.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/mm6108.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/pager_if_hw.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/pager_if.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/pageset.c
    ${ROOT_DIR}/src/driver/morse_driver/mm8108/mm8108.c
    ${ROOT_DIR}/src/driver/morse_driver/mm8108/yaps-hw.c
    ${ROOT_DIR}/src/driver/morse_driver/mm8108/yaps.c
    ${ROOT_DIR}/src/driver/beacon/beacon.c
    ${ROOT_DIR}/src/driver/driver.c
    ${ROOT_DIR}/src/driver/driver_task.c
    ${ROOT_DIR}/src/driver/puff/puff.c
    ${ROOT_DIR}/src/driver/morse_crc/morse_crc.c
    ${ROOT_DIR}/src/driver/morse_driver/command.c
    ${ROOT_DIR}/src/driver/morse_driver/coredump.c
    ${ROOT_DIR}/src/driver/morse_driver/event.c
    ${ROOT_DIR}/src/driver/morse_driver/ext_host_table.c
    ${ROOT_DIR}/src/driver/morse_driver/firmware.c
    ${ROOT_DIR}/src/driver/morse_driver/firmware_mbin.c
    ${ROOT_DIR}/src/driver/morse_driver/hw.c
    ${ROOT_DIR}/src/driver/morse_driver/ps.c
    ${ROOT_DIR}/src/driver/morse_driver/skbq.c
)

if(CONFIG_HALOW_TRANSPORT_USB)
    list(APPEND SRCS ${MM_IOT_EDGEZ_ESP_HALOW_MORSELIB_USB_SOURCE})
else()
    list(APPEND SRCS
        ${ROOT_DIR}/src/driver/transport/sdio.c
        ${ROOT_DIR}/src/driver/transport/sdio_spi.c
    )
endif()

# Experimental lower-MAC/RF-only archive. Keep this source list explicit so a
# future production switch cannot accidentally pull the reference UMAC or
# hostap back across the boundary. The archive is built beside the current
# morselib image but is not linked into the running firmware yet.
set(RF_BOUNDARY_SRCS
    ${ROOT_DIR}/src/common/common.c
    ${ROOT_DIR}/src/common/mac_address.c
    ${ROOT_DIR}/src/common/mmpkt.c
    ${ROOT_DIR}/src/common/mmpkt_list.c
    ${ROOT_DIR}/src/dot11/dot11_utils.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/mm6108.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/pager_if_hw.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/pager_if.c
    ${ROOT_DIR}/src/driver/morse_driver/mm6108/pageset.c
    ${ROOT_DIR}/src/driver/morse_driver/mm8108/mm8108.c
    ${ROOT_DIR}/src/driver/morse_driver/mm8108/yaps-hw.c
    ${ROOT_DIR}/src/driver/morse_driver/mm8108/yaps.c
    ${ROOT_DIR}/src/driver/beacon/beacon.c
    ${ROOT_DIR}/src/driver/driver.c
    ${ROOT_DIR}/src/driver/driver_task.c
    ${ROOT_DIR}/src/driver/puff/puff.c
    ${ROOT_DIR}/src/driver/morse_crc/morse_crc.c
    ${ROOT_DIR}/src/driver/morse_driver/command.c
    ${ROOT_DIR}/src/driver/morse_driver/coredump.c
    ${ROOT_DIR}/src/driver/morse_driver/event.c
    ${ROOT_DIR}/src/driver/morse_driver/ext_host_table.c
    ${ROOT_DIR}/src/driver/morse_driver/firmware.c
    ${ROOT_DIR}/src/driver/morse_driver/firmware_mbin.c
    ${ROOT_DIR}/src/driver/morse_driver/hw.c
    ${ROOT_DIR}/src/driver/morse_driver/ps.c
    ${ROOT_DIR}/src/driver/morse_driver/skbq.c
    ${EDGEZ_RF_MORSEMICRO_HOST_SOURCE}
)

if(CONFIG_HALOW_TRANSPORT_USB)
    list(APPEND RF_BOUNDARY_SRCS ${MM_IOT_EDGEZ_ESP_HALOW_MORSELIB_USB_SOURCE})
else()
    list(APPEND RF_BOUNDARY_SRCS
        ${ROOT_DIR}/src/driver/transport/sdio.c
        ${ROOT_DIR}/src/driver/transport/sdio_spi.c
    )
endif()

foreach(_rf_source IN LISTS RF_BOUNDARY_SRCS)
    if(_rf_source MATCHES "/morselib/src/umac/" OR
       _rf_source MATCHES "/framework/src/hostap/")
        message(FATAL_ERROR
            "RF-only boundary must not compile Morse UMAC/hostap source: ${_rf_source}")
    endif()
endforeach()
unset(_rf_source)

if(CONFIG_HALOW_AP_MODE OR CONFIG_HALOW_MESH_MODE)
    list(APPEND SRCS
        ${ROOT_DIR}/src/umac/ap/umac_ap.c
        ${ROOT_DIR}/src/umac/datapath/umac_datapath_ap.c
        ${ROOT_DIR}/src/umac/supplicant_shim/driver_ap.c
        ${ROOT_DIR}/src/umac/supplicant_shim/supplicant_core_ap.c
    )
endif()

if(NOT EDGEZ_RF_ONLY_MODE)
    add_library(libmorse STATIC ${SRCS} ${INC})
    target_include_directories(libmorse PUBLIC ${INC})
    if(CONFIG_HALOW_MESH_MODE)
        target_include_directories(libmorse PRIVATE ${MM_IOT_EDGEZ_INCLUDE_DIRS})
        target_compile_definitions(libmorse PRIVATE
            CONFIG_MESH=1
            CONFIG_MM_HALOW_MESH_JOIN_ONLY=1
            MESH_PREQ_INTERVAL_MS=${CONFIG_HALOW_MESH_PREQ_INTERVAL_MS}
            MMDRV_TASK_STACK_SIZE_WORDS=2048
        )
    endif()
    if(CONFIG_HALOW_TRANSPORT_USB)
        target_include_directories(libmorse PRIVATE ${MM_IOT_EDGEZ_ESP_HALOW_USB_DIR})
    endif()

    if(NOT CONFIG_HALOW_AP_MODE AND NOT CONFIG_HALOW_MESH_MODE)
        target_compile_definitions(libmorse PUBLIC MMWLAN_AP_DISABLED=1)
    endif()

    add_dependencies(libmorse mmhostap)
    target_link_libraries(libmorse PRIVATE mmutils mmhostap)
endif()

if(EDGEZ_RF_ONLY_MODE)
    add_library(morse_rf_boundary STATIC ${RF_BOUNDARY_SRCS} ${INC})
    idf_build_get_property(RF_BOUNDARY_ARCH IDF_TARGET_ARCH)
    target_include_directories(morse_rf_boundary PRIVATE
        ${INC}
        ${MM_IOT_EDGEZ_INCLUDE_DIRS}
        ${EDGEZ_RF_MORSEMICRO_INCLUDE_DIRS}
        ${CMAKE_CURRENT_LIST_DIR}/../shims/include/${RF_BOUNDARY_ARCH}
    )
    target_compile_definitions(morse_rf_boundary PRIVATE
        MMDRV_TASK_STACK_SIZE_WORDS=2048
    )
    if(CONFIG_HALOW_TRANSPORT_USB)
        target_include_directories(morse_rf_boundary PRIVATE
            ${MM_IOT_EDGEZ_ESP_HALOW_USB_DIR}
        )
    endif()
    target_link_libraries(morse_rf_boundary PRIVATE mmutils)
    add_dependencies(morse_rf_boundary morsefirmware)
endif()

# EdgeZ-owned upper half of the experimental raw-RF split. The production
# component already compiles the public Morse adapter, so building this helper
# there would compile the same source twice with different target properties.
if(EDGEZ_RF_ONLY_MODE)
    add_library(edgez_umac_runtime STATIC
        ${EDGEZ_RF_MORSEMICRO_ADAPTER_SOURCES}
    )
    target_include_directories(edgez_umac_runtime PRIVATE
        ${INC}
        ${MM_IOT_EDGEZ_INCLUDE_DIRS}
        ${EDGEZ_RF_MORSEMICRO_INCLUDE_DIRS}
        ${MM_IOT_SDK}/framework/src/mmregdb
        ${CMAKE_CURRENT_LIST_DIR}/../shims/include/${RF_BOUNDARY_ARCH}
    )
    target_link_libraries(edgez_umac_runtime PRIVATE mmutils)
    if(NOT EDGEZ_HALOW_MESH_SOURCE_BUILD)
        target_link_libraries(edgez_umac_runtime PRIVATE
            edgez_halow_mesh_core)
    endif()
endif()

if(NOT EDGEZ_RF_ONLY_MODE)
set(PROTECTED_SYMS_FILE "${MM_IOT_SDK}/framework/tools/metadata/protected_syms.txt")
file(STRINGS "${PROTECTED_SYMS_FILE}" _raw_syms
     REGEX "^[^#].*")
list(TRANSFORM _raw_syms STRIP)
list(FILTER _raw_syms EXCLUDE REGEX "^$")

set(protected_syms)
foreach(sym IN LISTS _raw_syms)
    list(APPEND protected_syms -p "${sym}")
endforeach()

string(REGEX REPLACE "objcopy(\.exe)?$" "" TOOLCHAIN_BASE "${CMAKE_OBJCOPY}")

if(WIN32)
	set(AR "${TOOLCHAIN_BASE}ar.exe")
	set(RANLIB "${TOOLCHAIN_BASE}ranlib.exe")
else()
	set(AR "${TOOLCHAIN_BASE}ar")
	set(RANLIB "${TOOLCHAIN_BASE}ranlib")
endif()

set(LIBMORSE  "$<TARGET_FILE:libmorse>")
set(LIBHOSTAP "$<TARGET_FILE:mmhostap>")
set(MANGLER "${MM_IOT_SDK}/framework/tools/buildsystem/librarymangler.py")
set(MERGESCRIPT "${CMAKE_CURRENT_BINARY_DIR}/libmorse.ar")
set(MERGERUNNER "${CMAKE_CURRENT_BINARY_DIR}/libmorse.ar.cmake")
set(LIBMORSEMANG "${CMAKE_CURRENT_BINARY_DIR}/libmorse.a")

file(GENERATE
    OUTPUT "${MERGESCRIPT}"
    CONTENT
    "CREATE \"${LIBMORSEMANG}\"
    ADDLIB \"${LIBMORSE}\"
    ADDLIB \"${LIBHOSTAP}\"
    SAVE
    END")

file(GENERATE
    OUTPUT "${MERGERUNNER}"
    CONTENT
    "execute_process(
        COMMAND \"${AR}\" -M
        INPUT_FILE \"${MERGESCRIPT}\"
        WORKING_DIRECTORY \"${CMAKE_CURRENT_BINARY_DIR}\"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR \"ar -M failed with exit code ${result}\")
    endif()")

add_custom_command(
    OUTPUT ${LIBMORSEMANG}
    COMMAND ${CMAKE_COMMAND} -E echo "Creating mangled libmorse"
    COMMAND ${CMAKE_COMMAND} -P "${MERGERUNNER}"
    COMMAND "${RANLIB}" "${LIBMORSEMANG}"
    COMMAND "${Python3_EXECUTABLE}" "${MANGLER}" -t "${TOOLCHAIN_BASE}" -m "${CMAKE_CURRENT_BINARY_DIR}" ${protected_syms} "${LIBMORSEMANG}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS libmorse mmhostap
)

add_custom_target(morselib_build DEPENDS "${LIBMORSEMANG}")
add_library(morselib INTERFACE)

target_include_directories(morselib INTERFACE ${INC})
target_link_libraries(morselib INTERFACE
    "-Wl,--start-group"
    libmorse
    mmhostap
    shims
    mmpktmem
    mmutils
    "-Wl,--end-group")
endif()
