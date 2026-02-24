# SPDX-License-Identifier: BSD-3-Clause-Clear

cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/smtc_hal_drag_rpi/cmake_drpi_toolchain.cmake)

project(lbm_drag_rpi
    DESCRIPTION "LoRa Basics Modem examples for the Dragino HAT for Raspberry Pi 3B+"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Warray-bounds=0)

# Set default build type
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Disable fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# Enable ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Generate a compile_commands.json for IDEs / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

################################################################################
# Project Options

set(APP "" CACHE STRING "The example to build")
set(APPS periodical_uplink porting_tests)
set_property(CACHE APP PROPERTY STRINGS ${APPS})
if(APP STREQUAL "")
    message(FATAL_ERROR "You need to define an -DAPP= from the list ${APPS}")
endif()

set(APP_REGION "" CACHE STRING "The region to use with the app")

option(APP_DEBUG "Debug the example app" OFF)

option(APP_TRACE "choose to enable or disable application trace print (default: trace is ON)" ON)

################################################################################
# First build the HAL that might set useful variables

add_subdirectory(${SMTC_HAL_DIR})

################################################################################
# Set values and load LBM lib

# TODO: How to do that properly?
add_subdirectory(../lbm_lib lbm_lib)

################################################################################
# Example libs and executable

# Those libraries depend on lbm_lib
add_subdirectory(smtc_modem_hal)
add_subdirectory(radio_hal)

add_executable(lbm_example.elf)

add_subdirectory(main_examples)


string(TOUPPER ${APP} APP_UPPER)
target_compile_definitions(lbm_example.elf PRIVATE MAKEFILE_APP=${APP_UPPER})

if(NOT APP_REGION STREQUAL "")
    target_compile_definitions(lbm_example.elf PRIVATE MODEM_EXAMPLE_REGION=${APP_REGION})
endif()

if(APP_DEBUG)
    target_compile_definitions(smtc_hal PRIVATE HW_DEBUG_PROBE=1)
endif()

if(APP_TRACE)
    target_compile_definitions(smtc_hal PUBLIC HAL_DBG_TRACE=1)
else()
    target_compile_definitions(smtc_hal PUBLIC HAL_DBG_TRACE=0)
endif()

# need for sx127x compilation
if(RADIO_FAMILY STREQUAL sx127x)
    target_link_libraries(smtc_hal PRIVATE ${radio_driver_library})
endif()

# All this is derived from what LBM enabled

string(TOUPPER ${LBM_RADIO} RADIO_UPPER)
target_compile_definitions(smtc_modem_hal_implem PUBLIC ${RADIO_UPPER})
target_compile_definitions(smtc_hal PUBLIC ${RADIO_UPPER})

string(TOUPPER ${RADIO_FAMILY} RADIO_FAMILY_UPPER)
target_compile_definitions(smtc_modem_hal_implem PUBLIC ${RADIO_FAMILY_UPPER})
target_compile_definitions(smtc_hal PUBLIC ${RADIO_FAMILY_UPPER})

if(NOT LBM_NUMBER_OF_STACKS STREQUAL "1")
    target_compile_definitions(smtc_hal PUBLIC MULTISTACK)
endif()

if(LBM_RELAY_TX)
    target_compile_definitions(smtc_modem_hal_implem PUBLIC USE_RELAY_TX)
endif()

if(LBM_RELAY_RX)
    target_compile_definitions(smtc_modem_hal_implem PUBLIC USE_RELAY_RX)
endif()


if(LBM_PERF_TEST)
    target_compile_definitions(lbm_example.elf PUBLIC PERF_TEST_ENABLED)
endif()

################################################################################

target_sources(lbm_example.elf PRIVATE
    main.c
    $<TARGET_OBJECTS:smtc_hal>
)

target_link_libraries(lbm_example.elf PRIVATE
    lora_basics_modem_core
    radio_hal
    smtc_modem_hal_implem
    m c
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../")
include(GetGitRevisionDescription)

git_version(GIT_VERSION)
target_compile_definitions(lbm_example.elf PUBLIC -DGIT_VERSION=\"${GIT_VERSION}\")

get_git_head_revision(GIT_REFSPEC GIT_SHA1 "ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR")
target_compile_definitions(lbm_example.elf PUBLIC -DGIT_COMMIT=\"${GIT_SHA1}\")

git_commit_date(GIT_DATE)
target_compile_definitions(lbm_example.elf PUBLIC -DGIT_DATE=\"${GIT_DATE}\")

string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H-%M")
target_compile_definitions(lbm_example.elf PUBLIC -DBUILD_DATE=\"${BUILD_DATE}\")


add_custom_command(
    TARGET lbm_example.elf PRE_LINK
    COMMAND ${CMAKE_SIZE} ${CMAKE_BINARY_DIR}/lbm_lib/smtc_modem_core/liblora_basics_modem_core.a
    COMMENT "Displays library liblora_basics_modem_core.a size details"
)

set(SSH "rpi0" CACHE STRING "The ssh target to copy the executable to")

add_custom_target(flash
    # needs scp
    DEPENDS lbm_example.elf
    COMMAND powershell.exe -c \"scp -p ${CMAKE_CURRENT_BINARY_DIR}/lbm_example.elf ${SSH}:.\"
    USES_TERMINAL
)
