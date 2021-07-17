CONTIKI = $(HOME)/contiki-ng-hoist
CONTIKI_PROJECT = multi-client multi-server-a #multi-server-b

#WITH_UIP6=1
#UIP_CONF_IPV6=1

#Configure RPL for Classic (Full RFC6550)
MAKE_ROUTING=MAKE_ROUTING_RPL_CLASSIC

# RPL Configuration

# - Lifetime
#LIFE_UNIT?=0xff
#LIFETIME?=0xffff

# - Trickle Settings
#DOUBLINGS?=6						# Max number of doublings for Trickle
#INTERVAL_MIN?=11				# 2^n ms minimum for Trickle

# - Algorithm Settings
#OF?=rpl_mrhof		# Objective Function
#RDC?=0					# RDC (0 - RDC Off)

#Send Defaults
SI?=20					# Send Interval in SECONDS

#Optimizations
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
#CFLAGS += -ffunction-sections
#LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

#RPL Initialization
#CFLAGS+= -DUIP_CONF_IPV6_RPL -DRPL_CONF_MAX_INSTANCES=3 -DRPL_ALLOWED_INSTANCES="\"OM\""
#CFLAGS+= -DRPL_CONF_DEFAULT_LIFETIME_UNIT=$(LIFE_UNIT) -DRPL_CONF_DEFAULT_LIFETIME=$(LIFETIME)
#CFLAGS+= -DRPL_CONF_DIO_INTERVAL_DOUBLINGS=$(DOUBLINGS) -DRPL_CONF_DIO_INTERVAL_MIN=$(INTERVAL_MIN)
#CFLAGS+= -DRPL_CONF_OF=$(OF) -DRDC_OFF=$(RDC)
CFLAGS+= -DSEND_INTERVAL_SECONDS=$(SI)

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include
