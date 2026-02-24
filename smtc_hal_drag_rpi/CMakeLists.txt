# SPDX-License-Identifier: BSD-3-Clause-Clear

list(APPEND CMAKE_PREFIX_PATH "/usr/aarch64-linux-gnu/usr/local")

find_library(PIGPIO_LIBRARY pigpio REQUIRED)
find_path(PIGPIO_INCLUDE_DIR pigpio.h REQUIRED)
message(STATUS "Library pigpio found: ${PIGPIO_LIBRARY}")

add_library(pigpio SHARED IMPORTED)
set_target_properties(pigpio PROPERTIES
    IMPORTED_LOCATION ${PIGPIO_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${PIGPIO_INCLUDE_DIR}
)


add_library(smtc_hal STATIC
    smtc_hal_nvm.c
    smtc_hal_gpio.c
    smtc_hal_mcu.c
    smtc_hal_rtc.c
    smtc_hal_rng.c
    smtc_hal_spi.c
    smtc_hal_lp_timer.c
    smtc_hal_trace.c
)

target_include_directories(smtc_hal PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(smtc_hal PRIVATE pigpio)
