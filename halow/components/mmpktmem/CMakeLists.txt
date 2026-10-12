# Copyright 2024-2026 Morse Micro
# SPDX-License-Identifier: Apache-2.0

set(ROOT_DIR ${MM_IOT_SDK}/framework/src/mmpktmem)

if(CONFIG_HALOW_MESH_MODE)
    if(NOT DEFINED MM_IOT_EDGEZ_MM_IOT_MMPKTMEM_DIR OR
       NOT EXISTS "${MM_IOT_EDGEZ_MM_IOT_MMPKTMEM_DIR}/mmpktmem_heap.c")
        message(FATAL_ERROR
            "EdgeZ mesh mode requires the private mmpktmem source tree")
    endif()
    set(ROOT_DIR ${MM_IOT_EDGEZ_MM_IOT_MMPKTMEM_DIR})
    message(STATUS "Using EdgeZ private mmpktmem tree: ${ROOT_DIR}")
endif()

set(inc
    ${ROOT_DIR})
set(src
    ${ROOT_DIR}/mmpktmem_heap.c
)

add_compile_definitions(MMPKTMEM_TX_POOL_N_BLOCKS=CONFIG_MMPKTMEM_TX_POOL_N_BLOCKS)
add_compile_definitions(MMPKTMEM_RX_POOL_N_BLOCKS=CONFIG_MMPKTMEM_RX_POOL_N_BLOCKS)

add_library(mmpktmem STATIC ${src})
target_include_directories(mmpktmem PUBLIC ${inc})

if(EDGEZ_RF_ONLY_MODE)
    idf_build_get_property(ARCH IDF_TARGET_ARCH)
    target_include_directories(mmpktmem PRIVATE
        ${MM_IOT_SDK}/framework/morselib/include
        ${CMAKE_CURRENT_LIST_DIR}/../shims/include/${ARCH}
        ${CMAKE_CURRENT_LIST_DIR}/../shims)
    add_dependencies(mmpktmem mmutils)
    target_link_libraries(mmpktmem PUBLIC mmutils)
else()
    add_dependencies(mmpktmem mmutils morselib)
    target_link_libraries(mmpktmem PUBLIC mmutils morselib)
endif()
