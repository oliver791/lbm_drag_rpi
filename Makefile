##############################################################################
# Main makefile for basic_modem
##############################################################################
-include app_makefiles/app_printing.mk
-include app_makefiles/app_options.mk


#-----------------------------------------------------------------------------
# default action: print help
#-----------------------------------------------------------------------------
help:
	$(call echo_help_b, "Available TARGETs:	sx1276")
	$(call echo_help, "")
	$(call echo_help_b, "-------------------------------- Clean -------------------------------------")
	$(call echo_help, " * make clean_<TARGET>             : clean basic_modem app and lib for a given target")
	$(call echo_help, " * make clean_all                  : clean all")
	$(call echo_help, " * make clean_app                  : clean basic_modem app")
	$(call echo_help, "")
	$(call echo_help_b, "----------------------------- Compilation ----------------------------------")
	$(call echo_help, " * make <TARGET>                   : build basic_modem app and lib on a given target")
	$(call echo_help, "")
	$(call echo_help_b, "---------------------------- All inclusive ---------------------------------")
	$(call echo_help, " * make full_<TARGET>              : clean and build basic_modem on a given target")
	$(call echo_help, "")
	$(call echo_help_b, "---------------------- Optional build parameters ---------------------------")
	$(call echo_help, " * BOARD=xxx                       : choose which mcu board will be used:(default is DRAGINO_RPI)")
	$(call echo_help, " *                                  - DRAGINO_RPI")
	$(call echo_help, " * MODEM_APP=xxx                   : choose which modem application to build:(default is PERIODICAL_UPLINK)")
	$(call echo_help, " *                                  - PERIODICAL_UPLINK")
	$(call echo_help, " *                                  - PORTING_TESTS")
	$(call echo_help, " * REGION=xxx                      : choose which region should be compiled (default: ALL)")
	$(call echo_help, " *                                   Combinations also work (i.e. REGION=EU_868,US_915 )")
	$(call echo_help, " *                                  - AS_923")
	$(call echo_help, " *                                  - AU_915")
	$(call echo_help, " *                                  - CN_470")
	$(call echo_help, " *                                  - CN_470_RP_1_0")
	$(call echo_help, " *                                  - EU_868")
	$(call echo_help, " *                                  - IN_865")
	$(call echo_help, " *                                  - KR_920")
	$(call echo_help, " *                                  - RU_864")
	$(call echo_help, " *                                  - US_915")
	$(call echo_help, " *                                  - WW_2G4 (to be used only for lr1120 and sx128x targets)")
	$(call echo_help, " *                                  - ALL (to build all possible regions according to the radio target) ")
	$(call echo_help, " * CRYPTO=xxx                      : choose which crypto should be compiled (default: SOFT)")
	$(call echo_help, " *                                  - SOFT")
	$(call echo_help, " * LBM_TRACE=yes/no                : choose to enable or disable modem trace print (default: yes)")
	$(call echo_help, " * APP_TRACE=yes/no                : choose to enable or disable application trace print (default: yes)")
	$(call echo_help, " * ALLOW_RELAY_TX=yes/no           : choose to enable or disable RelayTx (default: no)")
	$(call echo_help, " * ALLOW_RELAY_RX=yes/no           : choose to enable or disable RelayRx (default: no)")
	$(call echo_help_b, "-------------------- Optional makefile parameters --------------------------")
	$(call echo_help, " * MULTITHREAD=yes/no              : Disable multithreaded build (default: yes)")
	$(call echo_help, " * VERBOSE=yes/no                  : Increase build verbosity (default: no)")
	$(call echo_help, " * SIZE=yes/no                     : Display size for all objects (default: no)")
	$(call echo_help, " * DEBUG=yes/no                    : Compile library and application with debug symbols (default: no)")



#-----------------------------------------------------------------------------
# Makefile include selection
#-----------------------------------------------------------------------------
ifeq ($(TARGET_RADIO),sx1276)
-include app_makefiles/app_sx127x.mk
endif

#-----------------------------------------------------------------------------
-include app_makefiles/app_common.mk

.PHONY: clean_all help

#-----------------------------------------------------------------------------
# Clean
#-----------------------------------------------------------------------------
clean_all: clean_app
	$(MAKE) -C $(LORA_BASICS_MODEM) clean_all $(MTHREAD_FLAG)

clean_sx1276:
	$(MAKE) clean_modem TARGET_RADIO=sx1276
	$(MAKE) clean_target TARGET_RADIO=sx1276

clean:
	$(MAKE) clean_modem
	$(MAKE) clean_target

clean_app:
	-rm -rf $(APPBUILD_ROOT)*


#-----------------------------------------------------------------------------
# Application Compilation
#-----------------------------------------------------------------------------

#-- Generic -------------------------------------------------------------------
app:
ifeq ($(TARGET_RADIO),nc)
	$(call echo_error,"No radio selected! Please specified the target radio using TARGET_RADIO=radio_name option")
else
ifneq ($(CRYPTO),SOFT)
ifneq ($(LBM_NB_OF_STACK),1)
	$(call echo_error, "----------------------------------------------------------")
	$(call echo_error, "More than one stack compiled: only soft crypto can be used")
	$(call echo_error, "Please use CRYPTO=SOFT option")
	$(call echo_error, "----------------------------------------------------------")
	exit 1
endif
endif
	$(MAKE) app_build
endif

#-- SX1276 -------------------------------------------------------------------
sx1276:
	$(MAKE) app TARGET_RADIO=sx1276 $(MTHREAD_FLAG)

full_sx1276:
	$(MAKE) clean_modem TARGET_RADIO=sx1276
	$(MAKE) clean_target TARGET_RADIO=sx1276
	$(MAKE) app TARGET_RADIO=sx1276 $(MTHREAD_FLAG)
