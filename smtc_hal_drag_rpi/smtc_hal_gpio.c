/*!
 * \file      smtc_hal_gpio.c
 *
 * \brief     GPIO Hardware Abstraction Layer implementation
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2021. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */
#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type
#include <stdlib.h>   // exit

#include "smtc_hal_gpio.h"
#include "smtc_hal_mcu.h"
#include "smtc_hal_dbg_trace.h"
#include <pigpio.h>

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */
#define OFF 3

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

typedef struct hal_gpio_s
{
    const hal_gpio_irq_t *irq;
    hal_gpio_irq_mode_t  irq_mode;
    bool                 blocked;
    bool                 pending;
} gpio_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/*!
 * Conversion arrays for pigpio.h settings
 */
static const uint8_t modes[] = {OFF, RISING_EDGE, FALLING_EDGE, EITHER_EDGE};
static const uint8_t pulls[] = {PI_PUD_OFF, PI_PUD_UP, PI_PUD_DOWN};

/*!
 * Array holding attached gpio context
 *
 * Note: global and static arrays are automatically initialized to 0
 */
static gpio_t gpio[P_NUM];

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*!
 * GPIO initialize
 */
void gpio_init( const hal_gpio_pin_names_t pin, const uint32_t value,
                const uint32_t pull_mode, const uint32_t io_mode );

/*!
 * GPIO IRQ callback
 */
void gpio_irq_callback( int gpio, int level, uint32_t tick );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

//
// MCU input pin Handling
//

void hal_gpio_init_in( const hal_gpio_pin_names_t pin, const hal_gpio_pull_mode_t pull_mode,
                       const hal_gpio_irq_mode_t irq_mode, hal_gpio_irq_t* irq )
{
    if( irq != NULL )
    {
        irq->pin = pin;
    }

    gpio_init( pin, PI_CLEAR, pulls[pull_mode], PI_INPUT );

    gpio[pin - 0x2u].irq_mode = irq_mode;

    hal_gpio_irq_attach(irq);
}

void hal_gpio_init_out( const hal_gpio_pin_names_t pin, const uint32_t value )
{
    gpio_init(pin, value, PI_PUD_OFF, PI_OUTPUT);
}

void hal_gpio_irq_deinit(void)
{
    for (size_t i = 0; i < P_NUM; i++)
    {
        if (gpio[i].irq != NULL)
        {
            if (gpioSetISRFunc(gpio[i].irq->pin, 0, 0, NULL) != 0)
            {
                // no reset to avoid error-looping
                mcu_panic_trace();
            }
        }
    }
}

void hal_gpio_irq_attach( const hal_gpio_irq_t* irq )
{
    if ((irq == NULL) || (irq->callback == NULL))
    {
        return;
    }

    uint8_t irq_mode = gpio[irq->pin - 0x2u].irq_mode;
    if (irq_mode == BSP_GPIO_IRQ_MODE_OFF)
    {
        return;
    }

    gpio[irq->pin - 0x2u].irq = irq;
    if (gpioSetISRFunc(irq->pin, modes[irq_mode], 0, gpio_irq_callback) != 0)
    {
        mcu_panic();
    }
}

void hal_gpio_irq_detach( const hal_gpio_irq_t* irq )
{
    if (irq == NULL)
    {
        return;
    }

    if (gpioSetISRFunc(irq->pin, 0, 0, NULL) != 0)
    {
        mcu_panic();
    }
    gpio[irq->pin - 0x2u].irq = NULL;
}

void hal_gpio_irq_enable( void )
{
    for (size_t i = 0; i < P_NUM; i++)
    {
        gpio[i].blocked = false;
        if (gpio[i].pending && (gpio[i].irq != NULL) && (gpio[i].irq->callback != NULL))
        {
            gpio[i].irq->callback(gpio[i].irq->context);
            gpio[i].pending = false;
        }
    }
}

void hal_gpio_irq_disable( void )
{
    for (size_t i = 0; i < P_NUM; i++)
    {
        gpio[i].blocked = true;
    }
}

//
// MCU pin state control
//

void hal_gpio_set_value( const hal_gpio_pin_names_t pin, const uint32_t value )
{
    if (gpioWrite(pin, (value != 0) ? PI_SET : PI_CLEAR) != 0)
    {
        mcu_panic();
    }
}

uint32_t hal_gpio_get_value( const hal_gpio_pin_names_t pin )
{
    int value = gpioRead(pin);
    if (value == PI_BAD_GPIO)
    {
        mcu_panic();
    }
    return value;
}

void hal_gpio_clear_pending_irq( const hal_gpio_pin_names_t pin )
{
    for (size_t i = 0; i < P_NUM; i++)
    {
        gpio[i].pending = false;
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

void gpio_init(const hal_gpio_pin_names_t pin, const uint32_t value,
                   const uint32_t pull_mode, const uint32_t io_mode)
{
    hal_gpio_set_value(pin, value);
    if (gpioSetPullUpDown(pin, pull_mode) != 0)
    {
        mcu_panic();
    }
    if (gpioSetMode(pin, io_mode) != 0)
    {
        mcu_panic();
    }
}

void gpio_irq_callback( int pin, int level, uint32_t tick )
{
    uint8_t index = pin - 0x2u;

    if (gpio[index].blocked)
    {
        gpio[index].pending = true;
        return;
    }

    if ((gpio[index].irq != NULL) && (gpio[index].irq->callback != NULL))
    {
        gpio[index].irq->callback(gpio[index].irq->context);
    }
}

/* --- EOF ------------------------------------------------------------------ */
