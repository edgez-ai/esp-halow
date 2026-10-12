# Copyright 2024-2026 Morse Micro
# SPDX-License-Identifier: Apache-2.0

set(src "mmosal_shim_freertos_esp32.c"
	"mmhal_core.c"
	"mmhal_os.c"
	"mmhal_wlan_binaries.c"
)

if(CONFIG_HALOW_MESH_MODE)
    if(TARGET edgez_halow_mesh_platform)
        # The protected platform archive provides the matching Hostap crypto
        # implementation. Consumers must not compile Hostap from mm-iot-sdk.
        set(MM_IOT_EDGEZ_ESP_HALOW_MESH_CCM_SOURCES)
    elseif(NOT DEFINED MM_IOT_EDGEZ_ESP_HALOW_MESH_CCM_SOURCES)
        message(FATAL_ERROR "EdgeZ mesh mode requires the private CCM adapter")
    else()
        list(APPEND src "crypto_mbedtls_mm.c")
    endif()
    foreach(_mesh_ccm_source IN LISTS MM_IOT_EDGEZ_ESP_HALOW_MESH_CCM_SOURCES)
        if(NOT EXISTS "${_mesh_ccm_source}")
            message(FATAL_ERROR "Missing EdgeZ mesh CCM source: ${_mesh_ccm_source}")
        endif()
    endforeach()
    list(APPEND src ${MM_IOT_EDGEZ_ESP_HALOW_MESH_CCM_SOURCES})
else()
    list(APPEND src "crypto_mbedtls_mm.c")
endif()

if(CONFIG_HALOW_TRANSPORT_SPI)
	list(APPEND src "mmhal_wlan.c")
endif()

idf_build_get_property(ARCH IDF_TARGET_ARCH)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/${ARCH}")
    message(WARNING "Architecture ${ARCH} missing morselib debug macro mappings. Stubbing with noops.")
    set(inc "include/unknown" ".")
else()
    set(inc "include/${ARCH}" ".")
endif()

if(CONFIG_HALOW_TRANSPORT_SPI)
    foreach(pin MM_RESET_N MM_SPI_SCK MM_SPI_MOSI MM_SPI_MISO MM_SPI_CS)
        if(CONFIG_${pin} LESS 0)
            list(APPEND _mm_unset_pins ${pin})
        endif()
    endforeach()
    if(_mm_unset_pins)
        string(REPLACE ";" " " _mm_unset_pins_str "${_mm_unset_pins}")
        # FATAL_ERROR here blocks set-target and menuconfig
        add_custom_target(mm_pin_config_error ALL
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red --bold
                "ERROR: the following MM pins must be set to a valid pin number: ${_mm_unset_pins_str}. Set them via menuconfig or use a sdkconfig.defaults found in managed_components/morsemicro__halow/configs"
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM
        )
    endif()

    # Check if only MM_WAKE is defined OR only MM_BUSY is defined
    if((NOT (CONFIG_MM_WAKE LESS 0) AND CONFIG_MM_BUSY LESS 0) OR (CONFIG_MM_WAKE LESS 0 AND NOT (CONFIG_MM_BUSY LESS 0)))
        add_custom_target(_mm_ps_pins_error ALL
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red --bold
                "ERROR: MM_WAKE and MM_BUSY pins need to either both be defined and routed for power saving, or both need to be disabled (-1 default definition) to mark this as intentional and forgo power saving"
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM
        )
    endif()
endif()

add_library(shims STATIC ${src})
target_include_directories(shims PUBLIC ${inc})
if(EDGEZ_RF_ONLY_MODE)
    # The platform implementations consume public mmhal/mmosal declarations;
    # provide those headers directly rather than inheriting them by linking
    # the complete reference morselib.
    target_include_directories(shims PRIVATE
        ${MM_IOT_SDK}/framework/morselib/include)
endif()
if(CONFIG_HALOW_MESH_MODE)
    target_include_directories(shims PUBLIC
        ${MM_IOT_EDGEZ_INCLUDE_DIRS}
        ${EDGEZ_RF_API_INCLUDE_DIRS})
    if(NOT EDGEZ_HALOW_MESH_SOURCE_BUILD)
        target_link_libraries(shims PUBLIC edgez_halow_mesh_core)
    endif()
endif()

target_link_libraries(shims PUBLIC
	idf::log
	idf::driver
	idf::mbedtls
	idf::esp_timer
)

if(EDGEZ_RF_ONLY_MODE)
    target_link_libraries(shims PUBLIC morsefirmware mmutils)
    add_dependencies(shims morsefirmware)
else()
    target_link_libraries(shims PUBLIC
        morsefirmware
        morselib
        mmutils
    )
    add_dependencies(shims morselib morsefirmware)
endif()
