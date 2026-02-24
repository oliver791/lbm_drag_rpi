/*!
 * \file      main.c
 *
 * \brief     main program
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
#include <unistd.h>   // fork
#include <sys/wait.h> // waitpid
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // strcmp

#include "main.h"

/*
 * -----------------------------------------------------------------------------
 * --- CONFIGURABLE GLOBALS (defaults, overridden by command line) -------------
 */
uint32_t g_uplink_period_s   = 60;
uint8_t  g_packet_size       = 12;
bool     g_packet_size_fixed = true;

/*
 * -----------------------------------------------------------------------------
 * --- APPLICATION SELECTION ---------------------------------------------------
 */
#ifndef MAKEFILE_APP
#pragma GCC warning "Using default application PERIODICAL_UPLINK"
#define MAKEFILE_APP PERIODICAL_UPLINK
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

int main( int argc, char* argv[] )
{
    /* --- Parse command-line arguments ---
     *  argv[1] = period_s
     *  argv[2] = packet_size (max size if variable mode, 1-222)
     *  argv[3] = "fixed" or "var"
     */
    if( argc >= 2 )
    {
        int p = atoi( argv[1] );
        if( p < 1 )
        {
            p = 1;
        }
        g_uplink_period_s = ( uint32_t ) p;
    }

    if( argc >= 3 )
    {
        int s = atoi( argv[2] );
        if( s < 1 )
        {
            s = 1;
        }
        if( s > 222 )
        {
            s = 222;
        }
        g_packet_size = ( uint8_t ) s;
    }

    if( argc >= 4 )
    {
        if( strcmp( argv[3], "var" ) == 0 ||
            strcmp( argv[3], "variable" ) == 0 ||
            strcmp( argv[3], "1" ) == 0 )
        {
            g_packet_size_fixed = false;
        }
        else
        {
            g_packet_size_fixed = true;
        }
    }

    printf( "=== LoRaWAN Periodical Uplink ===\n" );
    printf( "  Period:      %u s\n", ( unsigned ) g_uplink_period_s );
    printf( "  Packet size: %u bytes (%s)\n", ( unsigned ) g_packet_size,
            g_packet_size_fixed ? "FIXED" : "VARIABLE 1..max" );
    printf( "=================================\n" );

    /* --- Fork-loop: restarts the app on mcu_panic (exit code 3) --- */
    pid_t cpid;
    int   wstatus = 0;

    do
    {
        if( ( cpid = fork( ) ) == 0 )
        {
#if MAKEFILE_APP == PERIODICAL_UPLINK
            main_periodical_uplink( );
#elif MAKEFILE_APP == PORTING_TESTS
            main_porting_tests( );
#else
#error "Unknown application"
#endif
        }
        waitpid( cpid, &wstatus, 0 );
    } while( WIFEXITED( wstatus ) && WEXITSTATUS( wstatus ) == 3 );

    return 0;
}
