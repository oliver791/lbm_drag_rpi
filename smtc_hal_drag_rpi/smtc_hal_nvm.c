/*!
 * \file      smtc_hal_nvm.c
 *
 * \brief     NVM Hardware Abstraction Layer implementation
 *
 * MIT License
 *
 * Copyright (c) 2024 Alessandro Aimi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdint.h>  // C99 types
#include <stdbool.h> // bool type
#include <fcntl.h>   // open, close
#include <unistd.h>  // lseek, read, write

#include "smtc_hal_nvm.h"
#include "smtc_hal_mcu.h"

#include <string.h> // memcpy
#include <assert.h> // assert

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static const char *restrict pathname = "/tmp/lorawan-dragino-nvm";

int f;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void hal_nvm_write_buffer(uint32_t addr, const uint8_t *buffer, uint32_t size)
{
    if ((f = open(pathname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0)
    {
        mcu_panic();
    }
    if (lseek(f, addr, SEEK_SET) < 0)
    {
        mcu_panic();
    }
    if (write(f, buffer, size) < 0)
    {
        mcu_panic();
    }
    if (close(f) < 0)
    {
        mcu_panic();
    }
}

void hal_nvm_read_buffer(uint32_t addr, uint8_t *buffer, uint32_t size)
{
    if ((f = open(pathname, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0)
    {
        mcu_panic();
    }
    if (lseek(f, addr, SEEK_SET) < 0)
    {
        mcu_panic();
    }
    if (read(f, buffer, size) < 0)
    {
        mcu_panic();
    }
    if (close(f) < 0)
    {
        mcu_panic();
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

/* --- EOF ------------------------------------------------------------------ */
