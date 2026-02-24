# SPDX-License-Identifier: BSD-3-Clause-Clear

# This CMake toolchain file describes how to build for Dragino RaspberryPi 3B+

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
SET(CMAKE_CROSSCOMPILING 1)


set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_SIZE       aarch64-linux-gnu-size)


set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


set(SMTC_HAL_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_C_FLAGS_INIT "\
-mcpu=cortex-a53 -mtune=cortex-a53 -fno-builtin \
-fno-unroll-loops -ffast-math -ftree-vectorize -fomit-frame-pointer \
-fdata-sections -ffunction-sections -falign-functions=4 \
-D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=600 \
"
)
