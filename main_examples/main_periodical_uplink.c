/*!
 * \file      main_periodical_uplink.c
 *
 * \brief     main program for periodical uplink example with CSV logging
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
 * Modifications by Olivier (2026):
 * - Added CSV logging of TX, RX (DOWNDATA), JOIN, JOINFAIL events
 * - Captures radio parameters: SF, BW, CR, frequency, TX power
 * - Captures MAC layer parameters: data rate, ADR, nb_trans, duty cycle, etc.
 * - RSSI/SNR logging on downlinks
 * - Configurable uplink period, packet size and size mode via command line
 * - Random payload generation
 * - EXTRA field in JSON format for easy post-processing
 *
 * Usage: app_sx1276.elf [period_s] [packet_size] [fixed|var]
 *   period_s    : uplink period in seconds (default: 60, min: 1)
 *   packet_size : payload size in bytes (default: 12, max: 222)
 *                 In variable mode, this is the MAXIMUM size.
 *   fixed|var   : "fixed" = constant size each TX (default)
 *                 "var"   = random size between 1 and packet_size each TX
 *
 * Examples:
 *   sudo ./build_sx1276_drpi/app_sx1276.elf 30 50 fixed
 *   sudo ./build_sx1276_drpi/app_sx1276.elf 10 222 var
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */
#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "main.h"

#include "smtc_modem_test_api.h"
#include "smtc_modem_api.h"
#include "smtc_modem_utilities.h"

#include "smtc_modem_hal.h"
#include "smtc_hal_dbg_trace.h"

#include "example_options.h"

#include "smtc_hal_mcu.h"
#include "smtc_hal_gpio.h"

#include "modem_pinout.h"
#include "smtc_modem_relay_api.h"

#include "sx127x.h"

/* --- Defines nécessaires pour les headers internes LBM --- */
#ifndef RP2_103
#define RP2_103
#endif
#ifndef REGION_EU_868
#define REGION_EU_868
#endif
#ifndef NUMBER_OF_STACKS
#define NUMBER_OF_STACKS 1
#endif
/* --------------------------------------------------------- */

#include "lr1_stack_mac_layer.h"
#include "lorawan_api.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/**
 * @brief Minimum payload size used in variable mode.
 *        Change this value to raise the lower bound (e.g. 12).
 */
#define PACKET_SIZE_MIN_VARIABLE 1

/*
 * -----------------------------------------------------------------------------
 * --- FILE-SCOPE VARIABLES ----------------------------------------------------
 */

/* CSV file handle */
static FILE *csv_fp = NULL;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

static int  csv_init( void );
static void csv_write_row( const uint8_t deveui[8], const char *event,
                           const uint8_t *data, size_t datalen,
                           const char *sf, const char *extra );
static void csv_close( void );

/*
 * -----------------------------------------------------------------------------
 * --- CSV HELPER FUNCTIONS ----------------------------------------------------
 */

static void current_timestr( char *buf, size_t buflen )
{
    time_t t = time( NULL );
    struct tm tm;
    localtime_r( &t, &tm );
    strftime( buf, buflen, "%Y-%m-%d--%H-%M-%S", &tm );
}

static char *hex_to_str( const uint8_t *buf, size_t len )
{
    if( len == 0 )
    {
        return strdup( "" );
    }
    size_t outlen = len * 2 + 1;
    char *out = malloc( outlen );
    if( !out )
    {
        return NULL;
    }
    for( size_t i = 0; i < len; i++ )
    {
        snprintf( out + i * 2, 3, "%02X", buf[i] );
    }
    out[outlen - 1] = '\0';
    return out;
}

static void deveui_to_str( char *dst, size_t dstlen, const uint8_t deveui[8] )
{
    char tmp[17];
    for( int i = 0; i < 8; i++ )
    {
        snprintf( tmp + i * 2, 3, "%02X", deveui[i] );
    }
    tmp[16] = '\0';
    if( dstlen > 0 )
    {
        strncpy( dst, tmp, dstlen - 1 );
        dst[dstlen - 1] = '\0';
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- CSV FIELD ESCAPING (RFC 4180) -------------------------------------------
 */
static void csv_write_escaped_field( FILE *fp, const char *str )
{
    fputc( '"', fp );
    if( str != NULL )
    {
        while( *str )
        {
            if( *str == '"' )
            {
                fputc( '"', fp );
            }
            fputc( *str, fp );
            str++;
        }
    }
    fputc( '"', fp );
}

/*
 * -----------------------------------------------------------------------------
 * --- RADIO ENUM-TO-STRING HELPERS --------------------------------------------
 */

static const char* sx127x_sf_to_str( sx127x_lora_sf_t sf )
{
    switch( sf )
    {
    case SX127X_LORA_SF6:  return "SF6";
    case SX127X_LORA_SF7:  return "SF7";
    case SX127X_LORA_SF8:  return "SF8";
    case SX127X_LORA_SF9:  return "SF9";
    case SX127X_LORA_SF10: return "SF10";
    case SX127X_LORA_SF11: return "SF11";
    case SX127X_LORA_SF12: return "SF12";
    default:               return "SF?";
    }
}

static const char* sx127x_bw_to_str( sx127x_lora_bw_t bw )
{
    switch( bw )
    {
    case SX127X_LORA_BW_007: return "7.8k";
    case SX127X_LORA_BW_010: return "10.4k";
    case SX127X_LORA_BW_015: return "15.6k";
    case SX127X_LORA_BW_020: return "20.8k";
    case SX127X_LORA_BW_031: return "31.25k";
    case SX127X_LORA_BW_041: return "41.7k";
    case SX127X_LORA_BW_062: return "62.5k";
    case SX127X_LORA_BW_125: return "125k";
    case SX127X_LORA_BW_250: return "250k";
    case SX127X_LORA_BW_500: return "500k";
    default:                 return "BW?";
    }
}

static const char* sx127x_cr_to_str( sx127x_lora_cr_t cr )
{
    switch( cr )
    {
    case SX127X_LORA_CR_4_5: return "4/5";
    case SX127X_LORA_CR_4_6: return "4/6";
    case SX127X_LORA_CR_4_7: return "4/7";
    case SX127X_LORA_CR_4_8: return "4/8";
    default:                 return "CR?";
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- RANDOM PAYLOAD GENERATOR ------------------------------------------------
 */

static void generate_random_payload( uint8_t* buffer, uint8_t size )
{
    for( uint8_t i = 0; i < size; i++ )
    {
        buffer[i] = ( uint8_t )( rand( ) % 256 );
    }
}

/**
 * @brief Compute the actual payload size for the current TX.
 *
 * - In FIXED mode:    always returns g_packet_size.
 * - In VARIABLE mode: returns a random value in
 *                     [PACKET_SIZE_MIN_VARIABLE .. g_packet_size].
 */
static uint8_t compute_payload_size( void )
{
    if( g_packet_size_fixed )
    {
        return ( uint8_t ) g_packet_size;
    }
    else
    {
        uint8_t min_s = PACKET_SIZE_MIN_VARIABLE;
        uint8_t max_s = g_packet_size;

        if( min_s > max_s )
        {
            min_s = max_s;
        }

        return ( uint8_t )( min_s + ( rand( ) % ( max_s - min_s + 1 ) ) );
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- CSV FILE OPERATIONS -----------------------------------------------------
 */

static int csv_init( void )
{
    char timestr[32];
    current_timestr( timestr, sizeof( timestr ) );

    char filename[128];
    snprintf( filename, sizeof( filename ), "lorawan-%s.csv", timestr );

    csv_fp = fopen( filename, "a" );
    if( !csv_fp )
    {
        SMTC_HAL_TRACE_ERROR( "Failed to open CSV file %s: %s\n", filename, strerror( errno ) );
        return -1;
    }

    fprintf( csv_fp, "TIMESTAMP,DEVEUI,EVENT,DATA,SF,EXTRA\n" );
    fflush( csv_fp );
    return 0;
}

static void csv_write_row( const uint8_t deveui[8], const char *event,
                           const uint8_t *data, size_t datalen,
                           const char *sf, const char *extra )
{
    if( !csv_fp )
    {
        return;
    }

    char timestr[32];
    current_timestr( timestr, sizeof( timestr ) );

    char devstr[32];
    deveui_to_str( devstr, sizeof( devstr ), deveui );

    char *datas = hex_to_str( data, datalen );
    if( !datas )
    {
        datas = strdup( "" );
    }

    fprintf( csv_fp, "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",",
             timestr,
             devstr,
             event ? event : "",
             datas,
             sf ? sf : "" );

    csv_write_escaped_field( csv_fp, extra );
    fprintf( csv_fp, "\n" );

    fflush( csv_fp );
    free( datas );
}

static void csv_close( void )
{
    if( csv_fp )
    {
        fclose( csv_fp );
        csv_fp = NULL;
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

#ifndef MIN
#define MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#endif

#define xstr( a ) str( a )
#define str( a ) #a

#define ASSERT_SMTC_MODEM_RC( rc_func )                                                         \
    do                                                                                          \
    {                                                                                           \
        smtc_modem_return_code_t rc = rc_func;                                                  \
        if( rc == SMTC_MODEM_RC_NOT_INIT )                                                      \
        {                                                                                       \
            SMTC_HAL_TRACE_ERROR( "In %s - %s (line %d): %s\n", __FILE__, __func__, __LINE__,   \
                                  xstr( SMTC_MODEM_RC_NOT_INIT ) );                             \
        }                                                                                       \
        else if( rc == SMTC_MODEM_RC_INVALID )                                                  \
        {                                                                                       \
            SMTC_HAL_TRACE_ERROR( "In %s - %s (line %d): %s\n", __FILE__, __func__, __LINE__,   \
                                  xstr( SMTC_MODEM_RC_INVALID ) );                              \
        }                                                                                       \
        else if( rc == SMTC_MODEM_RC_BUSY )                                                     \
        {                                                                                       \
            SMTC_HAL_TRACE_ERROR( "In %s - %s (line %d): %s\n", __FILE__, __func__, __LINE__,   \
                                  xstr( SMTC_MODEM_RC_BUSY ) );                                 \
        }                                                                                       \
        else if( rc == SMTC_MODEM_RC_FAIL )                                                     \
        {                                                                                       \
            SMTC_HAL_TRACE_ERROR( "In %s - %s (line %d): %s\n", __FILE__, __func__, __LINE__,   \
                                  xstr( SMTC_MODEM_RC_FAIL ) );                                 \
        }                                                                                       \
        else if( rc == SMTC_MODEM_RC_NO_TIME )                                                  \
        {                                                                                       \
            SMTC_HAL_TRACE_WARNING( "In %s - %s (line %d): %s\n", __FILE__, __func__, __LINE__, \
                                    xstr( SMTC_MODEM_RC_NO_TIME ) );                            \
        }                                                                                       \
        else if( rc == SMTC_MODEM_RC_INVALID_STACK_ID )                                         \
        {                                                                                       \
            SMTC_HAL_TRACE_ERROR( "In %s - %s (line %d): %s\n", __FILE__, __func__, __LINE__,   \
                                  xstr( SMTC_MODEM_RC_INVALID_STACK_ID ) );                     \
        }                                                                                       \
        else if( rc == SMTC_MODEM_RC_NO_EVENT )                                                 \
        {                                                                                       \
            SMTC_HAL_TRACE_INFO( "In %s - %s (line %d): %s\n", __FILE__, __func__, __LINE__,    \
                                 xstr( SMTC_MODEM_RC_NO_EVENT ) );                              \
        }                                                                                       \
    } while( 0 )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define STACK_ID 0

static const uint8_t user_dev_eui[8]      = USER_LORAWAN_DEVICE_EUI;
static const uint8_t user_join_eui[8]     = USER_LORAWAN_JOIN_EUI;
static const uint8_t user_gen_app_key[16] = USER_LORAWAN_GEN_APP_KEY;
static const uint8_t user_app_key[16]     = USER_LORAWAN_APP_KEY;

#define WATCHDOG_RELOAD_PERIOD_MS 20000

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */
static uint8_t                  rx_payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH] = { 0 };
static uint8_t                  rx_payload_size = 0;
static smtc_modem_dl_metadata_t rx_metadata     = { 0 };
static uint8_t                  rx_remaining    = 0;

static uint32_t uplink_counter = 0;

#if defined( USE_RELAY_TX )
static smtc_modem_relay_tx_config_t relay_config = { 0 };
#endif

static int16_t last_rssi              = 0;
static int16_t last_snr               = 0;
static uint8_t last_rx_payload_length = 0;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

static void modem_event_callback( void );
static void send_uplink_counter_on_port( uint8_t port );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void main_periodical_uplink( void )
{
    uint32_t sleep_time_ms = 0;

    hal_mcu_init( );

    /* Seed random number generator for random payloads */
    srand( ( unsigned int ) time( NULL ) );

    smtc_modem_init( &modem_event_callback );

    if( csv_init( ) != 0 )
    {
        SMTC_HAL_TRACE_ERROR( "CSV init failed, continuing without CSV logging\n" );
    }
    atexit( csv_close );

    SMTC_HAL_TRACE_INFO( "Periodical uplink example is starting\n" );
    SMTC_HAL_TRACE_INFO( "  Period:      %d s\n", g_uplink_period_s );
    SMTC_HAL_TRACE_INFO( "  Packet size: %d bytes max (%s)\n", g_packet_size,
                         g_packet_size_fixed ? "FIXED" : "VARIABLE" );
    if( !g_packet_size_fixed )
    {
        SMTC_HAL_TRACE_INFO( "  Variable range: %d .. %d bytes\n",
                             PACKET_SIZE_MIN_VARIABLE, g_packet_size );
    }

    while( 1 )
    {
        sleep_time_ms = smtc_modem_run_engine( );

        if( smtc_modem_is_irq_flag_pending( ) == false )
        {
            hal_mcu_set_sleep_for_ms( MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS ) );
        }
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void modem_event_callback( void )
{
    SMTC_HAL_TRACE_MSG_COLOR( "Modem event callback\n", HAL_DBG_TRACE_COLOR_BLUE );

    smtc_modem_event_t current_event;
    uint8_t            event_pending_count;
    uint8_t            stack_id = STACK_ID;

    do
    {
        ASSERT_SMTC_MODEM_RC( smtc_modem_get_event( &current_event, &event_pending_count ) );

        switch( current_event.event_type )
        {
        case SMTC_MODEM_EVENT_RESET:
            SMTC_HAL_TRACE_INFO( "Event received: RESET\n" );

            ASSERT_SMTC_MODEM_RC( smtc_modem_set_deveui( stack_id, user_dev_eui ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_joineui( stack_id, user_join_eui ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_appkey( stack_id, user_gen_app_key ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_nwkkey( stack_id, user_app_key ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_region( stack_id, MODEM_EXAMPLE_REGION ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_report_all_downlinks_to_user( stack_id, true ) );

#if defined( USE_RELAY_TX )
            relay_config.second_ch_enable = false;
            relay_config.activation =
                SMTC_MODEM_RELAY_TX_ACTIVATION_MODE_ENABLE;
            relay_config.number_of_miss_wor_ack_to_switch_in_nosync_mode = 3;
            relay_config.smart_level = 8;
            relay_config.backoff = 0;
            ASSERT_SMTC_MODEM_RC( smtc_modem_relay_tx_enable( stack_id, &relay_config ) );
#endif

            ASSERT_SMTC_MODEM_RC( smtc_modem_join_network( stack_id ) );
            break;

        case SMTC_MODEM_EVENT_ALARM:
            SMTC_HAL_TRACE_INFO( "Event received: ALARM\n" );
            send_uplink_counter_on_port( 101 );
            ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( g_uplink_period_s ) );
            break;

        case SMTC_MODEM_EVENT_JOINED:
            SMTC_HAL_TRACE_INFO( "Event received: JOINED\n" );
            SMTC_HAL_TRACE_INFO( "Modem is now joined \n" );

            send_uplink_counter_on_port( 101 );
            ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( g_uplink_period_s ) );

            {
                const char *sf_txt = "";
                char extra[128];
                snprintf( extra, sizeof( extra ),
                          "{\"reason\" : \"Modem is now joined\"}" );

                sx127x_t* radio = ( sx127x_t* ) smtc_modem_get_radio_context( );
                if( radio != NULL && radio->pkt_type == SX127X_PKT_TYPE_LORA )
                {
                    sf_txt = sx127x_sf_to_str( radio->lora_mod_params.sf );
                }

                csv_write_row( user_dev_eui, "JOINED", NULL, 0, sf_txt, extra );
            }
            break;

        case SMTC_MODEM_EVENT_TXDONE:
            SMTC_HAL_TRACE_INFO( "Event received: TXDONE\n" );
            SMTC_HAL_TRACE_INFO( "Transmission done \n" );

            {
                const char *sf_txt = "";
                sx127x_t* radio = ( sx127x_t* ) smtc_modem_get_radio_context( );
                if( radio != NULL && radio->pkt_type == SX127X_PKT_TYPE_LORA )
                {
                    sf_txt = sx127x_sf_to_str( radio->lora_mod_params.sf );
                }
                csv_write_row( user_dev_eui, "TXDONE", NULL, 0, sf_txt,
                               "{\"status\" : \"OK\"}" );
            }
            break;

        case SMTC_MODEM_EVENT_DOWNDATA:
            SMTC_HAL_TRACE_INFO( "Event received: DOWNDATA\n" );
            ASSERT_SMTC_MODEM_RC(
                smtc_modem_get_downlink_data( rx_payload, &rx_payload_size, &rx_metadata, &rx_remaining ) );
            SMTC_HAL_TRACE_PRINTF( "Data received on port %u\n", rx_metadata.fport );
            SMTC_HAL_TRACE_ARRAY( "Received payload", rx_payload, rx_payload_size );

            {
                char extra[256] = "";
                const char *sf_txt = "";
                bool    has_rssi  = false;
                bool    has_snr   = false;
                int32_t rssi_dbm  = INT32_MIN;
                float   snr_db    = 0.0f;
                uint32_t freq_hz  = rx_metadata.frequency_hz;

                rssi_dbm = ( int32_t ) rx_metadata.rssi - 64;
                snr_db   = ( float ) rx_metadata.snr / 4.0f;

                if( ( rssi_dbm >= -140 ) && ( rssi_dbm <= 10 ) )
                {
                    has_rssi = true;
                }
                else
                {
                    SMTC_HAL_TRACE_WARNING( "rx_metadata.rssi implausible: %d\n", ( int ) rx_metadata.rssi );
                }

                if( ( snr_db >= -50.0f ) && ( snr_db <= 50.0f ) )
                {
                    has_snr = true;
                }
                else
                {
                    SMTC_HAL_TRACE_WARNING( "rx_metadata.snr implausible: %d (raw)\n", ( int ) rx_metadata.snr );
                }

                if( !has_rssi && !has_snr )
                {
                    sx127x_t* radio = ( sx127x_t* ) smtc_modem_get_radio_context( );
                    if( radio != NULL )
                    {
                        if( radio->pkt_type == SX127X_PKT_TYPE_LORA )
                        {
                            sx127x_lora_pkt_status_t lora_status = { 0 };
                            sx127x_status_t st = sx127x_get_lora_pkt_status( radio, &lora_status );
                            SMTC_HAL_TRACE_PRINTF( "sx127x_get_lora_pkt_status() -> %d\n", ( int ) st );
                            if( st == SX127X_STATUS_OK )
                            {
                                int32_t rssi_alt = ( int32_t ) lora_status.rssi_pkt_in_dbm;
                                int32_t snr_alt  = ( int32_t ) lora_status.snr_pkt_in_db;
                                if( ( rssi_alt >= -140 ) && ( rssi_alt <= 10 ) )
                                {
                                    rssi_dbm = rssi_alt;
                                    has_rssi = true;
                                }
                                if( ( snr_alt >= -50 ) && ( snr_alt <= 50 ) )
                                {
                                    snr_db = ( float ) snr_alt;
                                    has_snr = true;
                                }
                            }
                        }
                        else
                        {
                            sx127x_gfsk_pkt_status_t gfsk_status = { 0 };
                            sx127x_status_t st = sx127x_get_gfsk_pkt_status( radio, &gfsk_status );
                            SMTC_HAL_TRACE_PRINTF( "sx127x_get_gfsk_pkt_status() -> %d\n", ( int ) st );
                            if( st == SX127X_STATUS_OK )
                            {
                                int32_t rssi_alt = ( int32_t ) gfsk_status.rssi_sync;
                                if( ( rssi_alt >= -140 ) && ( rssi_alt <= 10 ) )
                                {
                                    rssi_dbm = rssi_alt;
                                    has_rssi = true;
                                }
                            }
                        }
                    }
                    else
                    {
                        SMTC_HAL_TRACE_WARNING( "radio context is NULL in DOWNDATA fallback\n" );
                    }
                }

                sx127x_t* radio_for_sf = ( sx127x_t* ) smtc_modem_get_radio_context( );
                if( radio_for_sf != NULL && radio_for_sf->pkt_type == SX127X_PKT_TYPE_LORA )
                {
                    sf_txt = sx127x_sf_to_str( radio_for_sf->lora_mod_params.sf );
                }

                if( has_rssi && has_snr )
                {
                    snprintf( extra, sizeof( extra ),
                              "{\"port\" : \"%u\", \"freq\" : \"%luHz(%.3fMHz)\", "
                              "\"rssi\" : \"%d dBm\", \"snr\" : \"%.2f dB\"}",
                              ( unsigned ) rx_metadata.fport,
                              ( unsigned long ) freq_hz, ( double ) freq_hz / 1e6,
                              ( int ) rssi_dbm, snr_db );
                }
                else if( has_rssi )
                {
                    snprintf( extra, sizeof( extra ),
                              "{\"port\" : \"%u\", \"freq\" : \"%luHz(%.3fMHz)\", "
                              "\"rssi\" : \"%d dBm\"}",
                              ( unsigned ) rx_metadata.fport,
                              ( unsigned long ) freq_hz, ( double ) freq_hz / 1e6,
                              ( int ) rssi_dbm );
                }
                else
                {
                    snprintf( extra, sizeof( extra ),
                              "{\"port\" : \"%u\", \"freq\" : \"%luHz(%.3fMHz)\"}",
                              ( unsigned ) rx_metadata.fport,
                              ( unsigned long ) freq_hz, ( double ) freq_hz / 1e6 );
                }

                csv_write_row( user_dev_eui, "DOWNDATA", rx_payload, rx_payload_size, sf_txt, extra );
            }
            break;

        case SMTC_MODEM_EVENT_JOINFAIL:
            SMTC_HAL_TRACE_INFO( "Event received: JOINFAIL\n" );

            {
                const char *sf_txt = "SF?";
                char extra[128];
                snprintf( extra, sizeof( extra ),
                          "{\"reason\" : \"JOINFAIL\"}" );

                sx127x_t* radio = ( sx127x_t* ) smtc_modem_get_radio_context( );
                if( radio != NULL && radio->pkt_type == SX127X_PKT_TYPE_LORA )
                {
                    sf_txt = sx127x_sf_to_str( radio->lora_mod_params.sf );
                }

                csv_write_row( user_dev_eui, "JOINFAIL", NULL, 0, sf_txt, extra );
            }
            break;

        case SMTC_MODEM_EVENT_ALCSYNC_TIME:
            SMTC_HAL_TRACE_INFO( "Event received: ALCSync service TIME\n" );
            break;

        case SMTC_MODEM_EVENT_LINK_CHECK:
            SMTC_HAL_TRACE_INFO( "Event received: LINK_CHECK\n" );
            break;

        case SMTC_MODEM_EVENT_CLASS_B_PING_SLOT_INFO:
            SMTC_HAL_TRACE_INFO( "Event received: CLASS_B_PING_SLOT_INFO\n" );
            break;

        case SMTC_MODEM_EVENT_CLASS_B_STATUS:
            SMTC_HAL_TRACE_INFO( "Event received: CLASS_B_STATUS\n" );
            break;

        case SMTC_MODEM_EVENT_LORAWAN_MAC_TIME:
            SMTC_HAL_TRACE_WARNING( "Event received: LORAWAN MAC TIME\n" );
            break;

        case SMTC_MODEM_EVENT_LORAWAN_FUOTA_DONE:
        {
            bool status = current_event.event_data.fuota_status.successful;
            if( status == true )
            {
                SMTC_HAL_TRACE_INFO( "Event received: FUOTA SUCCESSFUL\n" );
            }
            else
            {
                SMTC_HAL_TRACE_WARNING( "Event received: FUOTA FAIL\n" );
            }
            break;
        }

        case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_C:
            SMTC_HAL_TRACE_INFO( "Event received: MULTICAST CLASS_C STOP\n" );
            break;

        case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_B:
            SMTC_HAL_TRACE_INFO( "Event received: MULTICAST CLASS_B STOP\n" );
            break;

        case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_C:
            SMTC_HAL_TRACE_INFO( "Event received: New MULTICAST CLASS_C \n" );
            break;

        case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_B:
            SMTC_HAL_TRACE_INFO( "Event received: New MULTICAST CLASS_B\n" );
            break;

        case SMTC_MODEM_EVENT_FIRMWARE_MANAGEMENT:
            SMTC_HAL_TRACE_INFO( "Event received: FIRMWARE_MANAGEMENT\n" );
            if( current_event.event_data.fmp.status == SMTC_MODEM_EVENT_FMP_REBOOT_IMMEDIATELY )
            {
                csv_close( );
                smtc_modem_hal_reset_mcu( );
            }
            break;

        case SMTC_MODEM_EVENT_STREAM_DONE:
            SMTC_HAL_TRACE_INFO( "Event received: STREAM_DONE\n" );
            break;

        case SMTC_MODEM_EVENT_UPLOAD_DONE:
            SMTC_HAL_TRACE_INFO( "Event received: UPLOAD_DONE\n" );
            break;

        case SMTC_MODEM_EVENT_DM_SET_CONF:
            SMTC_HAL_TRACE_INFO( "Event received: DM_SET_CONF\n" );
            break;

        case SMTC_MODEM_EVENT_MUTE:
            SMTC_HAL_TRACE_INFO( "Event received: MUTE\n" );
            break;

        case SMTC_MODEM_EVENT_RELAY_TX_DYNAMIC:
            SMTC_HAL_TRACE_INFO( "Event received: RELAY_TX_DYNAMIC\n" );
            break;

        case SMTC_MODEM_EVENT_RELAY_TX_MODE:
            SMTC_HAL_TRACE_INFO( "Event received: RELAY_TX_MODE\n" );
            break;

        case SMTC_MODEM_EVENT_RELAY_TX_SYNC:
            SMTC_HAL_TRACE_INFO( "Event received: RELAY_TX_SYNC\n" );
            break;

        case SMTC_MODEM_EVENT_RELAY_RX_RUNNING:
            SMTC_HAL_TRACE_INFO( "Event received: RELAY_RX_RUNNING\n" );
#if defined( ALLOW_CSMA ) && defined( USE_RELAY_RX )
            {
                bool csma_state = false;
                ASSERT_SMTC_MODEM_RC( smtc_modem_csma_get_state( STACK_ID, &csma_state ) );
                if( ( current_event.event_data.relay_rx.status == true ) && ( csma_state == true ) )
                {
                    ASSERT_SMTC_MODEM_RC( smtc_modem_csma_set_state( STACK_ID, false ) );
                }
#if defined( ALLOW_CSMA_AND_ENABLE_AT_BOOT )
                if( current_event.event_data.relay_rx.status == false )
                {
                    ASSERT_SMTC_MODEM_RC( smtc_modem_csma_set_state( STACK_ID, true ) );
                }
#endif
            }
#endif
            break;

        case SMTC_MODEM_EVENT_REGIONAL_DUTY_CYCLE:
            SMTC_HAL_TRACE_INFO( "Event received: DUTY_CYCLE\n" );
            break;

        case SMTC_MODEM_EVENT_NO_DOWNLINK_THRESHOLD:
        {
            SMTC_HAL_TRACE_INFO( "Event received: NO_DOWNLINK_THRESHOLD\n" );
            if( current_event.event_data.no_downlink.status != 0 )
            {
                smtc_modem_alarm_clear_timer( );
                ASSERT_SMTC_MODEM_RC( smtc_modem_leave_network( stack_id ) );
                ASSERT_SMTC_MODEM_RC( smtc_modem_join_network( stack_id ) );
                SMTC_HAL_TRACE_INFO(
                    "Event received: %s-%s\n",
                    current_event.event_data.no_downlink.status & SMTC_MODEM_EVENT_NO_RX_THRESHOLD_ADR_BACKOFF_END
                        ? "ADR backoff end-"
                        : "",
                    current_event.event_data.no_downlink.status & SMTC_MODEM_EVENT_NO_RX_THRESHOLD_USER_THRESHOLD
                        ? "-User threshold reached"
                        : "" );
            }
            else
            {
                SMTC_HAL_TRACE_INFO( "Event type: Cleared\n" );
            }
            break;
        }

        case SMTC_MODEM_EVENT_TEST_MODE:
        {
            uint8_t status_test_mode = current_event.event_data.test_mode_status.status;
#if HAL_DBG_TRACE == HAL_FEATURE_ON
            char* status_name[] = {
                "SMTC_MODEM_EVENT_TEST_MODE_ENDED",
                "SMTC_MODEM_EVENT_TEST_MODE_TX_COMPLETED",
                "SMTC_MODEM_EVENT_TEST_MODE_TX_DONE",
                "SMTC_MODEM_EVENT_TEST_MODE_RX_DONE",
                "SMTC_MODEM_EVENT_TEST_MODE_RX_ABORTED"
            };
            if( status_test_mode < SMTC_MODEM_EVENT_TEST_MODE_RX_ABORTED )
            {
                SMTC_HAL_TRACE_INFO( "Event received: TEST_MODE :  %s\n", status_name[status_test_mode] );
            }
#endif
            if( status_test_mode == SMTC_MODEM_EVENT_TEST_MODE_RX_DONE )
            {
                int16_t rssi;
                int16_t snr;
                uint8_t rx_pl_length;
                smtc_modem_test_get_last_rx_packets( &rssi, &snr, rx_payload, &rx_pl_length );

                last_rssi              = rssi;
                last_snr               = snr;
                last_rx_payload_length = rx_pl_length;

                SMTC_HAL_TRACE_ARRAY( "rx_payload", rx_payload, rx_pl_length );
                SMTC_HAL_TRACE_PRINTF( "rssi: %d, snr: %d\n", rssi, snr );
            }
            break;
        }

        default:
            SMTC_HAL_TRACE_ERROR( "Unknown event %u\n", current_event.event_type );
            break;
        }
    } while( event_pending_count > 0 );
}

/**
 * @brief Send a random payload uplink on the specified port
 *
 * In FIXED mode:    payload size = g_packet_size (constant).
 * In VARIABLE mode: payload size = random value in
 *                   [PACKET_SIZE_MIN_VARIABLE .. g_packet_size].
 */
static void send_uplink_counter_on_port( uint8_t port )
{
    /* --- Compute actual payload size for this TX --- */
    uint8_t payload[255];
    uint8_t payload_size = compute_payload_size( );

    generate_random_payload( payload, payload_size );

    SMTC_HAL_TRACE_INFO( "TX #%lu: %d bytes (%s, max=%d), period=%ds\n",
                         ( unsigned long ) uplink_counter,
                         payload_size,
                         g_packet_size_fixed ? "fixed" : "variable",
                         g_packet_size,
                         g_uplink_period_s );

    /* --- CSV logging (TX) --- */
    {
        char extra[384]  = "";
        char extra2[768] = "";

        const char *sf_txt = "SF?";
        const char *bw_txt = "BW?";
        const char *cr_txt = "CR?";
        uint32_t freq_hz   = 0;
        uint8_t  reg_pa       = 0;
        uint8_t  output_power = 0;

        uint8_t  tx_data_rate              = 0xFF;
        uint8_t  tx_data_rate_adr          = 0xFF;
        int8_t   tx_power                  = INT8_MIN;
        uint8_t  nb_trans                  = 0xFF;
        uint8_t  nb_trans_cpt              = 0xFF;
        uint8_t  nb_available_tx_channel   = 0xFF;
        uint32_t tx_duty_cycle_timestamp_ms = 0;
        uint32_t max_duty_cycle_index      = 0;
        uint8_t  rx1_delay_s               = 0;

        lr1_stack_mac_t* mac = lorawan_api_stack_mac_get( STACK_ID );
        if( mac != NULL )
        {
            tx_data_rate              = mac->tx_data_rate;
            tx_data_rate_adr          = mac->tx_data_rate_adr;
            tx_power                  = mac->tx_power;
            nb_trans                  = mac->nb_trans;
            nb_trans_cpt              = mac->nb_trans_cpt;
            nb_available_tx_channel   = mac->nb_available_tx_channel;
            tx_duty_cycle_timestamp_ms = mac->tx_duty_cycle_timestamp_ms;
            max_duty_cycle_index      = mac->max_duty_cycle_index;
            rx1_delay_s               = mac->rx1_delay_s;
        }

        sx127x_t* radio = ( sx127x_t* ) smtc_modem_get_radio_context( );
        if( radio != NULL && radio->pkt_type == SX127X_PKT_TYPE_LORA )
        {
            sf_txt = sx127x_sf_to_str( radio->lora_mod_params.sf );
            bw_txt = sx127x_bw_to_str( radio->lora_mod_params.bw );
            cr_txt = sx127x_cr_to_str( radio->lora_mod_params.cr );
            reg_pa = radio->reg_pa_config;
            output_power = reg_pa & 0x0F;
            freq_hz = radio->rf_freq_in_hz;
        }

        snprintf( extra, sizeof( extra ),
                  "\"port\" : \"%u\", \"counter\" : \"%lu\", "
                  "\"size\" : \"%u\", \"size_mode\" : \"%s\", "
                  "\"size_max\" : \"%u\", \"period\" : \"%d\", "
                  "\"rssi\" : \"%d\", \"snr\" : \"%d\", "
                  "\"len\" : \"%u\", \"bw\" : \"%s\", \"cr\" : \"%s\", "
                  "\"freq\" : \"%luHz(%.3fMHz)\", \"output_power\" : \"%u\"",
                  ( unsigned ) port,
                  ( unsigned long ) uplink_counter,
                  ( unsigned ) payload_size,
                  g_packet_size_fixed ? "fixed" : "variable",
                  ( unsigned ) g_packet_size,
                  g_uplink_period_s,
                  ( int ) last_rssi,
                  ( int ) last_snr,
                  ( unsigned ) last_rx_payload_length,
                  bw_txt, cr_txt,
                  ( unsigned long ) freq_hz, ( double ) freq_hz / 1e6,
                  output_power );

        snprintf( extra2, sizeof( extra2 ),
                  "{%s, "
                  "\"tx_data_rate\" : \"%u\", \"tx_data_rate_adr\" : \"%u\", "
                  "\"tx_power\" : \"%d\", \"nb_trans\" : \"%u\", "
                  "\"nb_trans_cpt\" : \"%u\", "
                  "\"nb_available_tx_channel\" : \"%u\", "
                  "\"tx_duty_cycle_timestamp_ms\" : \"%u\", "
                  "\"max_duty_cycle_index\" : \"%u\", "
                  "\"rx1_delay_s\" : \"%u\"}",
                  extra,
                  ( unsigned ) tx_data_rate,
                  ( unsigned ) tx_data_rate_adr,
                  ( int ) tx_power,
                  ( unsigned ) nb_trans,
                  ( unsigned ) nb_trans_cpt,
                  ( unsigned ) nb_available_tx_channel,
                  ( unsigned ) tx_duty_cycle_timestamp_ms,
                  ( unsigned ) max_duty_cycle_index,
                  ( unsigned ) rx1_delay_s );

        csv_write_row( user_dev_eui, "TX", payload, payload_size, sf_txt, extra2 );
    }
    /* --- fin CSV logging --- */

    ASSERT_SMTC_MODEM_RC(
        smtc_modem_request_uplink( STACK_ID, port, false, payload, payload_size ) );
    uplink_counter++;
}

/* --- EOF ------------------------------------------------------------------ */
