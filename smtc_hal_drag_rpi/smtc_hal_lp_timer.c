/*!
 * \file      smtc_hal_lp_timer.c
 *
 * \brief     Implements Low Power Timer utilities functions.
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

#include "smtc_hal_lp_timer.h"
#include "smtc_hal_mcu.h"
#include "smtc_hal_rtc.h"

#include <time.h>
#include <signal.h>

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define HAL_LP_TIMER_NB 2  //!< Number of supported low power timers

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

typedef struct hal_lp_timer_s
{
    int signo;
    timer_t handle;
    hal_lp_timer_irq_t tmr_irq;
    bool blocked;
    bool pending;
} lp_timer_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static lp_timer_t lptim[HAL_LP_TIMER_NB];

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

void pl_timer_handler( int sig, siginfo_t *si, void *uc );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void hal_lp_timer_init( hal_lp_timer_id_t id )
{
    // + 1 is for rtc wakeup timer
    int signo = SIGRTMIN + id + 1;
    if (signo > SIGRTMAX)
    {
        mcu_panic();
    }
    lptim[id].signo = signo;

    struct sigaction sa; // establish handler for callback
    sa.sa_sigaction = pl_timer_handler;
    sigemptyset(&sa.sa_mask); // sigs to be blocked during handler execution
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(signo, &sa, NULL) == -1)
    {
        mcu_panic();
    }

    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = signo;
    sev.sigev_value.sival_int = id; // pass id here
    if (timer_create(RT_CLOCK, &sev, &lptim[id].handle) == -1)
    {
        mcu_panic();
    }
}

void hal_lp_timer_deinit( hal_lp_timer_id_t id )
{
    if (timer_delete(lptim[id].handle) == -1)
    {
        // no reset to avoid error-looping
        mcu_panic_trace();
    }

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(lptim[id].signo, &sa, NULL) == -1)
    {
        // no reset to avoid error-looping
        mcu_panic_trace();
    }
}

void hal_lp_timer_start( hal_lp_timer_id_t id, const uint32_t milliseconds, const hal_lp_timer_irq_t* tmr_irq )
{
    lptim[id].tmr_irq = *tmr_irq; // callback assignment

    struct itimerspec its;
    its.it_value.tv_sec = milliseconds / 1000;
    its.it_value.tv_nsec = milliseconds % 1000 * 1000000;
    its.it_interval = ZERO;
    if (timer_settime(lptim[id].handle, 0, &its, NULL) == -1)
    {
        mcu_panic();
    }
}

void hal_lp_timer_stop( hal_lp_timer_id_t id )
{
    lptim[id].tmr_irq = (hal_lp_timer_irq_t){.context = NULL, .callback = NULL};

    struct itimerspec its;
    its.it_value = ZERO;
    its.it_interval = ZERO;
    if (timer_settime(lptim[id].handle, 0, &its, NULL) == -1)
    {
        mcu_panic();
    }
}

void hal_lp_timer_irq_enable( hal_lp_timer_id_t id )
{
    lptim[id].blocked = false;

    if (lptim[id].pending && (lptim[id].tmr_irq.callback != NULL))
    {
        lptim[id].tmr_irq.callback(lptim[id].tmr_irq.context);
        lptim[id].pending = false;
    }
}

void hal_lp_timer_irq_disable( hal_lp_timer_id_t id )
{
    lptim[id].blocked = true;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

void pl_timer_handler( int sig, siginfo_t *si, void *uc )
{
    int id = si->si_value.sival_int;

    if (lptim[id].blocked)
    {
        lptim[id].pending = true;
        return;
    }

    if (lptim[id].tmr_irq.callback != NULL)
    {
        lptim[id].tmr_irq.callback(lptim[id].tmr_irq.context);
    }
}

/* --- EOF ------------------------------------------------------------------ */
