CONTIKI = $(HOME)/contiki
CONTIKI_PROJECT = collector

SI?=2
CI?=60
BPC?=2
SD?=0

#Optimizations
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

WITH_UIP6=1
UIP_CONF_IPV6=1
CFLAGS+= -DUIP_CONF_IPV6_RPL -DRPL_CONF_MAX_INSTANCES=1
CFLAGS+= -DRPL_DISALLOWED_INSTANCES="\"OM\""  
CFLAGS+= -DRPL_CONF_MAX_DAG_PER_INSTANCE=15
CFLAGS+= -DSEND_INTERVAL=$(SI)
CFLAGS+= -DBLOCKS_PER_CHECK=$(BPC)
CFLAGS+= -DSTARTING_DATA=$(SD)
CFLAGS+= -DCOLLECT_INTERVAL=$(CI)

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include
