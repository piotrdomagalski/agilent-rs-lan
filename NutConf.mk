# Automatically generated on Thu Nov 15 20:12:14 2012
#
# Do not edit, modify UserConf.mk instead!
#

PLATFORM= AT91SAM7X_EK
HWDEF+=-D$(PLATFORM)
LDNAME=at91sam7x256_rom
LDSCRIPT=$(LDNAME).ld
LDPATH=$(top_srcdir)/arch/arm/ldscripts
TRGT = arm-none-eabi-
MCU=arm7tdmi
CRUROM=crurom


include $(top_appdir)/UserConf.mk
