/*!
 * \file      main.h
 *
 * \brief     main program header
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* --- Configurable globals (set in main.c from command line) --- */
extern uint32_t g_uplink_period_s;
extern uint8_t  g_packet_size;
extern bool     g_packet_size_fixed;

/* --- Application entry points --- */
void main_periodical_uplink( void );
void main_porting_tests( void );

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
