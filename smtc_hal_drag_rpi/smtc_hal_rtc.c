/*!
 * \file      smtc_hal_rtc.c
 *
 * \brief     RTC Hardware Abstraction Layer implementation
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

#include <time.h>
#include <signal.h>
#include "smtc_hal_rtc.h"

#include "smtc_hal_mcu.h"

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

static struct timespec rtc_starttime;
static timer_t         rtc_tid;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

void rtc_wakeup_timer_handler( int sig, siginfo_t *si, void *uc );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void hal_rtc_init( void )
{
    clock_gettime(RT_CLOCK, &rtc_starttime);

    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    if (timer_create(RT_CLOCK, &sev, &rtc_tid) == -1)
    {
        mcu_panic();
    }

    struct sigaction sa;
    sa.sa_sigaction = rtc_wakeup_timer_handler;
    sigemptyset(&sa.sa_mask); // sigs to be blocked during handler execution
    sa.sa_flags = SA_SIGINFO; // | SA_RESTART;
    if (sigaction(SIGRTMIN, &sa, NULL) == -1)
    {
        mcu_panic();
    }
}

void hal_rtc_deinit( void )
{
    if (timer_delete(rtc_tid) == -1)
    {
        // no reset to avoid error-looping
        mcu_panic_trace();
    }
}

uint32_t hal_rtc_get_time_s( void )
{
    struct timespec now;
    clock_gettime(RT_CLOCK, &now);

    return now.tv_sec - rtc_starttime.tv_sec - (now.tv_nsec < rtc_starttime.tv_nsec);
}

uint32_t hal_rtc_get_time_ms( void )
{
    struct timespec now;
    clock_gettime(RT_CLOCK, &now);

    return (now.tv_sec - rtc_starttime.tv_sec) * 1e3 + (now.tv_nsec - rtc_starttime.tv_nsec) / 1e6 + .5;
}

void hal_rtc_wakeup_timer_set_ms( const int32_t milliseconds )
{
    struct itimerspec its;
    its.it_value.tv_sec = milliseconds / 1000;
    its.it_value.tv_nsec = milliseconds % 1000 * 1000000;
    its.it_interval = ZERO;
    if (timer_settime(rtc_tid, 0, &its, NULL) == -1)
    {
        mcu_panic();
    }
}

void hal_rtc_wakeup_timer_stop( void )
{
    struct itimerspec its;
    its.it_value = ZERO;
    its.it_interval = ZERO;
    if (timer_settime(rtc_tid, 0, &its, NULL) == -1)
    {
        mcu_panic();
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

void rtc_wakeup_timer_handler( int sig, siginfo_t *si, void *uc )
{
    hal_mcu_wakeup( );
}

/* --- EOF ------------------------------------------------------------------ */
