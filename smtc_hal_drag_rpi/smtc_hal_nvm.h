/*!
 * \file      smtc_hal_nvm.h
 *
 * \brief     NVM Hardware Abstraction Layer definition
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
#ifndef __SMTC_HAL_NVM_H__
#define __SMTC_HAL_NVM_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

/**
 * @brief Writes the given buffer to the NVM at the specified address.
 *
 * @param [in] addr    NVM address to write to
 * @param [in] buffer  Pointer to the buffer to be written.
 * @param [in] size    Size of the buffer to be written.
 */
void hal_nvm_write_buffer( uint32_t addr, const uint8_t* buffer, uint32_t size );

/**
 * @brief Reads the NVM at the specified address to the given buffer.
 *
 * @param [in]  addr    NVM address to read from
 * @param [out] buffer  Pointer to the buffer to be written with read data.
 * @param [in]  size    Size of the buffer to be read.
 */
void hal_nvm_read_buffer( uint32_t addr, uint8_t* buffer, uint32_t size );

#ifdef __cplusplus
}
#endif

#endif  // __SMTC_HAL_NVM_H__

/* --- EOF ------------------------------------------------------------------ */
