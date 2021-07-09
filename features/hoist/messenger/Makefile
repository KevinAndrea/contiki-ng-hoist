CONTIKI = $(HOME)/contiki
CONTIKI_PROJECT = messenger

WITH_UIP6=1
UIP_CONF_IPV6=1

#Defaults
LIFE_UNIT?=0x1
LIFETIME?=0xff
DOUBLINGS?=6
INTERVAL_MIN?=11
OF?=rpl_mrhof
RDC?=1
SI?=2
BPC?=2
SD?=0
CI?=60

#Optimizations
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

#RPL Initialization
CFLAGS+= -DUIP_CONF_IPV6_RPL -DRPL_CONF_MAX_INSTANCES=1 -DRPL_ALLOWED_INSTANCES=\"M\" 
CFLAGS+= -DRPL_CONF_DEFAULT_LIFETIME_UNIT=$(LIFE_UNIT) -DRPL_CONF_DEFAULT_LIFETIME=$(LIFETIME)
CFLAGS+= -DRPL_CONF_DIO_INTERVAL_DOUBLINGS=$(DOUBLINGS) -DRPL_CONF_DIO_INTERVAL_MIN=$(INTERVAL_MIN)
CFLAGS+= -DRPL_CONF_OF=$(OF) -DRDC_OFF=$(RDC)
CFLAGS+= -DSEND_INTERVAL=$(SI)
CFLAGS+= -DBLOCKS_PER_CHECK=$(BPC)
CFLAGS+= -DSTARTING_DATA=$(SD)
CFLAGS+= -DCOLLECT_INTERVAL=$(CI)


all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include

