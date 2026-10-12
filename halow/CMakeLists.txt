# Copyright 2025 Robert Carey
# Copyright 2025-2026 Morse Micro
# SPDX-License-Identifier: Apache-2.0

set(src)
set(inc)
# Component dependency discovery runs before Kconfig values are available.
# Declare USB unconditionally; USB sources remain transport-selective.
set(priv_reqs usb)

if(CONFIG_DAB_RAW_RF_TEST)
    set(reqs)
else()
    set(reqs
        esp_netif
        esp_wifi
    )
endif()

include(${CMAKE_CURRENT_LIST_DIR}/components/edgez-rf-api/component.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/components/edgez-rf-morsemicro/component.cmake)

set(EDGEZ_HALOW_MESH_SOURCE_BUILD OFF)
if(DEFINED ENV{EDGEZ_HALOW_MESH_SOURCE_ROOT} AND
   NOT "$ENV{EDGEZ_HALOW_MESH_SOURCE_ROOT}" STREQUAL "")
    set(EDGEZ_HALOW_MESH_SOURCE_ROOT
        "$ENV{EDGEZ_HALOW_MESH_SOURCE_ROOT}")
    cmake_path(ABSOLUTE_PATH EDGEZ_HALOW_MESH_SOURCE_ROOT NORMALIZE)
    if(NOT EXISTS "${EDGEZ_HALOW_MESH_SOURCE_ROOT}/ports/esp_idf/component.cmake")
        message(FATAL_ERROR
            "EDGEZ_HALOW_MESH_SOURCE_ROOT is not an edgez-halow-mesh checkout: ${EDGEZ_HALOW_MESH_SOURCE_ROOT}")
    endif()
    set(EDGEZ_HALOW_MESH_SOURCE_BUILD ON)
    message(STATUS
        "Using EdgeZ HaLow mesh source checkout: ${EDGEZ_HALOW_MESH_SOURCE_ROOT}")
    include("${EDGEZ_HALOW_MESH_SOURCE_ROOT}/ports/esp_idf/component.cmake")
    set(EDGEZ_HALOW_MESH_PREBUILT_ROOT "${EDGEZ_HALOW_MESH_SOURCE_ROOT}")
    set(EDGEZ_HOSTAP_ROOT "${MM_IOT_EDGEZ_MM_IOT_HOSTAP_DIR}")
    list(APPEND src
        ${MM_IOT_EDGEZ_COMMON_SOURCES}
        ${MM_IOT_EDGEZ_ESP_IDF_SOURCES})
    list(APPEND inc
        ${MM_IOT_EDGEZ_INCLUDE_DIRS}
        ${EDGEZ_RF_API_INCLUDE_DIRS})
    if(CONFIG_HALOW_EDGEZ_MQTT_RELAY)
        list(APPEND src ${MM_IOT_EDGEZ_ESP_IDF_MQTT_SOURCE})
    endif()
else()
    # Production consumers use the protected release archives. Diagnostics
    # are intentionally available only through the explicit source override.
    set(EDGEZ_HALOW_MESH_PREBUILT_ROOT
        ${CMAKE_CURRENT_LIST_DIR}/components/edgez-halow-mesh-prebuilt)
    set(EDGEZ_HALOW_MESH_PREBUILT_LIBRARY
        ${EDGEZ_HALOW_MESH_PREBUILT_ROOT}/lib/libedgez_halow_mesh.a)
    set(EDGEZ_HALOW_MESH_PLATFORM_LIBRARY
        ${EDGEZ_HALOW_MESH_PREBUILT_ROOT}/lib/libedgez_halow_mesh_platform.a)
    if(NOT EXISTS "${EDGEZ_HALOW_MESH_PREBUILT_LIBRARY}" OR
       NOT EXISTS "${EDGEZ_HALOW_MESH_PLATFORM_LIBRARY}")
        message(FATAL_ERROR
            "Missing EdgeZ ESP32-P4 prebuilt libraries under: ${EDGEZ_HALOW_MESH_PREBUILT_ROOT}/lib")
    endif()
    set(MM_IOT_EDGEZ_INCLUDE_DIRS
        "${EDGEZ_HALOW_MESH_PREBUILT_ROOT}/include"
        ${EDGEZ_RF_API_INCLUDE_DIRS})
endif()
set(MM_IOT_EDGEZ_ESP_HALOW_USB_DIR
    "${CMAKE_CURRENT_LIST_DIR}/transport/usb")
set(MM_IOT_EDGEZ_ESP_HALOW_MORSELIB_USB_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/components/morselib/mm8108_usb_transport.c")
if(NOT EDGEZ_HALOW_MESH_SOURCE_BUILD)
    set(MM_IOT_EDGEZ_ESP_HALOW_MESH_CCM_SOURCES)
    set(MM_IOT_EDGEZ_MM_IOT_MMPKTMEM_DIR
        "${CMAKE_CURRENT_LIST_DIR}/components/mm-iot-sdk/framework/src/mmpktmem")
endif()

# ESP-IDF evaluates component files once in CMake script mode to discover
# dependencies. Imported build targets are only legal during real configure.
if(NOT EDGEZ_HALOW_MESH_SOURCE_BUILD AND NOT CMAKE_SCRIPT_MODE_FILE)
    add_library(edgez_halow_mesh_core STATIC IMPORTED GLOBAL)
    set_target_properties(edgez_halow_mesh_core PROPERTIES
        IMPORTED_LOCATION "${EDGEZ_HALOW_MESH_PREBUILT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${EDGEZ_HALOW_MESH_PREBUILT_ROOT}/include;${EDGEZ_RF_API_INCLUDE_DIRS}"
    )
    add_library(edgez_halow_mesh_platform STATIC IMPORTED GLOBAL)
    set_target_properties(edgez_halow_mesh_platform PROPERTIES
        IMPORTED_LOCATION "${EDGEZ_HALOW_MESH_PLATFORM_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${EDGEZ_HALOW_MESH_PREBUILT_ROOT}/include;${EDGEZ_RF_API_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES edgez_halow_mesh_core)
endif()

# The raw-RF test deliberately omits the esp-netif/mmwlan adapter. It exposes
# only the private RF-boundary API and the transport headers needed by the test
# application. Normal firmware keeps the existing adapter unchanged.
if(CONFIG_DAB_RAW_RF_TEST)
    list(APPEND inc
        ${MM_IOT_EDGEZ_INCLUDE_DIRS}
        ${CMAKE_CURRENT_LIST_DIR}/components/mm-iot-sdk/framework/morselib/include
        ${CMAKE_CURRENT_LIST_DIR}/components/mm-iot-sdk/framework/morselib/src/internal
        "."
    )
    list(APPEND priv_reqs esp_timer esp_system heap)
else()
    list(APPEND inc ${MM_IOT_EDGEZ_INCLUDE_DIRS} ".")
endif()

if(CONFIG_HALOW_TRANSPORT_USB)
	list(APPEND src
		${CMAKE_CURRENT_LIST_DIR}/transport/usb/mm8108_usb.c
		${CMAKE_CURRENT_LIST_DIR}/transport/usb/usb_host_service.c
	)
	list(APPEND inc ${MM_IOT_EDGEZ_ESP_HALOW_USB_DIR})
	list(APPEND priv_reqs usb)
endif()

if(CONFIG_HALOW_MESH_MODE AND NOT CONFIG_DAB_RAW_RF_TEST)
	list(APPEND src
		${CMAKE_CURRENT_LIST_DIR}/edgez_halow_platform.c
	)
	list(APPEND inc
		${MM_IOT_EDGEZ_INCLUDE_DIRS})
	list(APPEND priv_reqs esp_timer esp_system heap)

endif()

idf_component_register(
	INCLUDE_DIRS
	${inc}
	SRCS
	${src}
	REQUIRES
	${reqs}
	PRIV_REQUIRES
	${priv_reqs}
)

if(EDGEZ_HALOW_MESH_SOURCE_BUILD)
    idf_build_set_property(LINK_OPTIONS
        "-Wl,--allow-multiple-definition" APPEND)
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE
        EDGEZ_MORSE_PUBLIC_ADAPTER=1
        EDGEZ_FULL_HOSTAP_RUNTIME=1
        EDGEZ_DIAGNOSTICS_ENABLED=1)
    # The source checkout carries the complete HaLow Hostap implementation,
    # while ESP-IDF also links its 2.4 GHz supplicant for the optional gateway.
    # Both trees retain upstream public symbol names. The protected release
    # archives are post-processed; source-debug builds need the equivalent
    # linker tolerance so each caller resolves against its own archive first.
    target_link_options(${COMPONENT_TARGET} PUBLIC
        "LINKER:--allow-multiple-definition")
endif()

add_subdirectory(components)
if(CONFIG_HALOW_MESH_DEBUG_LOG)
    # The application-facing Kconfig name is intentionally vendor neutral;
    # translate it only at the locally compiled Morse UMAC boundary. Hostap is
    # supplied by the protected platform archive in this consumer build.
    target_compile_definitions(morse_rf_boundary PRIVATE
        CONFIG_MM_MESH_DEBUG_LOG=1
        printk=printf)
endif()
if(CONFIG_DAB_RAW_RF_TEST OR CONFIG_HALOW_MESH_MODE)
    add_dependencies(${COMPONENT_TARGET}
        mmpktmem shims mmutils morse_rf_boundary edgez_umac_runtime edgez_mesh_sae
        morsefirmware)
    target_link_libraries(${COMPONENT_TARGET} PUBLIC
        edgez_umac_runtime edgez_mesh_sae morse_rf_boundary
        mmpktmem shims mmutils morsefirmware)
    if(NOT EDGEZ_HALOW_MESH_SOURCE_BUILD)
        target_link_libraries(${COMPONENT_TARGET} PUBLIC
            edgez_halow_mesh_platform)
    endif()
    if(NOT CONFIG_DAB_RAW_RF_TEST)
        add_dependencies(${COMPONENT_TARGET} regdb)
        target_link_libraries(${COMPONENT_TARGET} PUBLIC regdb)
    endif()
else()
    add_dependencies(${COMPONENT_TARGET} mmpktmem shims mmutils morselib regdb)
    target_link_libraries(${COMPONENT_TARGET} PUBLIC
        edgez_mesh_sae mmpktmem shims mmutils morselib regdb)
    if(NOT EDGEZ_HALOW_MESH_SOURCE_BUILD)
        target_link_libraries(${COMPONENT_TARGET} PUBLIC
            edgez_halow_mesh_platform)
    endif()
endif()
