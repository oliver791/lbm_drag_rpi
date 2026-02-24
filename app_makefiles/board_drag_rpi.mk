##############################################################################
# Definitions for the Dragino GPS HAT on a Raspberry Pi 3B+ board 
##############################################################################

#-----------------------------------------------------------------------------
# Compilation flags
#-----------------------------------------------------------------------------

#MCU compilation flags
MCU_FLAGS ?= -mcpu=cortex-a53 -mtune=cortex-a53

BOARD_C_DEFS = -D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=600

#-----------------------------------------------------------------------------
# Hardware-specific sources
#-----------------------------------------------------------------------------
BOARD_C_SOURCES = \
	smtc_modem_hal/smtc_modem_hal.c\
	smtc_hal_drag_rpi/smtc_hal_nvm.c\
	smtc_hal_drag_rpi/smtc_hal_gpio.c\
	smtc_hal_drag_rpi/smtc_hal_mcu.c\
	smtc_hal_drag_rpi/smtc_hal_rtc.c\
	smtc_hal_drag_rpi/smtc_hal_rng.c\
	smtc_hal_drag_rpi/smtc_hal_spi.c\
	smtc_hal_drag_rpi/smtc_hal_lp_timer.c\
	smtc_hal_drag_rpi/smtc_hal_trace.c

BOARD_ASM_SOURCES = 

BOARD_C_INCLUDES =  \
	-I.\
	-Ismtc_modem_hal\
	-Ismtc_hal_drag_rpi\
	-I/usr/aarch64-linux-gnu/usr/local/include
