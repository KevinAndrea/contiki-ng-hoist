#ifndef COMMON_H
#define COMMON_H

#include "contiki.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "net/rpl/rpl.h"
#include "sys/node-id.h"
#include "dev/serial-line.h"
#include "dev/leds.h"
#include "lib/random.h"
#include "dev/button-sensor.h"
#include "dev/cc2420.h"
#include "net/uip-debug.h"
#if CONTIKI_TARGET_Z1
#include "dev/uart0.h"
#else
#include "dev/uart1.h"
#endif

/* Coffee Headers */
#include "cfs/cfs.h"
#include "cfs/cfs-coffee.h"

/* Standard C Headers */
#include <stdio.h>
#include <string.h>

#ifndef NEED_FORMATTING
#define NEED_FORMATTING 0
#endif

/* Indices */
#define MESSENGER           0
#define OBSERVER            1
#define COLLECTOR           2
#define BRIDGE              3
#define INVALID             4

/* Data Collection */
#define MAX_BLOCKS          BLOCKS_PER_CHECK
//#define DATA_PER_BLOCK     26 /* Single */
#define DATA_PER_BLOCK     19 /* Max - Adj for Fragments on Retrans */ 

/* Storage */
//#define OBSERVER_STORAGE    1279 /* Max Size */
//#define COLLECTOR_STORAGE   856  /* Max Size */

/* Messenger Storage 
 *  - 2492 bytes is max (1246 16-bit blocks)
 *  - Each data point is 4 bytes (2 + 2)
 *  - 623 data entries
 *  - ... At 1 coll/hr, this is 25 Motes/day
 */
#define MESSENGER_STORAGE   512 
#define OBSERVER_STORAGE    1024
#define COLLECTOR_STORAGE   512

//#define MESSENGER_STORAGE   512 /* Tested Sizes*/
//#define OBSERVER_STORAGE    256
//#define COLLECTOR_STORAGE   256

/* Power Rates (1-31(max))*/
#define BRIDGE_TX_POWER      31
#define MESSENGER_TX_POWER   31
#define OBSERVER_TX_POWER    31
#define COLLECTOR_TX_POWER   31

/* Message Types 4-bits only */
#define SYN                 0
#define DATA_MESSENGER      1
#define ACK                 2
#define START_MESSENGER     3
#define FIN_MESSENGER       4
#define TERM_MESSENGER      5
#define CHECK               6
#define DATA_OBSERVER       7
#define START_OBSERVER      8
#define TERM_OBSERVER       9
#define FULL               10

/* Addresses */
#define RPL_OBSERVER_OCTET        0x4F
#define RPL_COLLECTOR_OCTET       0x53
#define RPL_BRIDGE_OCTET          0x42
#define RPL_MESSENGER_OCTET       0x4D

/* Instances */
#define RPL_OBSERVER_INSTANCE     0x4F
#define RPL_BRIDGE_INSTANCE       0x42
#define RPL_MESSENGER_INSTANCE    0x4D

/* Ports */
#define OBSERVER_PORT    0x4F
#define COLLECTOR_PORT   0x53
#define BRIDGE_PORT      0x42
#define MESSENGER_PORT   0x4D

/* Timer Intervals */
//#define SEND_INTERVAL       2 /* Seconds */
#define SEND_RATE       CLOCK_SECOND * SEND_INTERVAL
#define COLLECT_RATE    CLOCK_SECOND * COLLECT_INTERVAL
#define BACKOFF_MIN     CLOCK_SECOND / 3
#define BACKOFF_ADD     CLOCK_SECOND / 4

/* Timeout Intervals */
#define START_TIMEOUT_INT  CLOCK_SECOND * 4
#define TERM_TIMEOUT       CLOCK_SECOND * 4
#define SYN_TIMEOUT        CLOCK_SECOND * 3
#define FIN_TIMEOUT        CLOCK_SECOND * 3
#define CHECK_TIMEOUT      CLOCK_SECOND * 3
#define DATA_TIMEOUT       CLOCK_SECOND * 5
#define MAX_ATTEMPTS       4

 /* Timeout Intervals * /
#define START_TIMEOUT_INT  CLOCK_SECOND * 4
#define TERM_TIMEOUT       CLOCK_SECOND * 4
#define SYN_TIMEOUT        CLOCK_SECOND * 5
#define FIN_TIMEOUT        CLOCK_SECOND * 3
#define CHECK_TIMEOUT      CLOCK_SECOND * 3
#define DATA_TIMEOUT       CLOCK_SECOND * 10
#define MAX_ATTEMPTS       5
*/
/* Common MACROS */
#define BACKOFF() (((random_rand()%2)<<2)-1)*(BACKOFF_MIN + random_rand()%BACKOFF_ADD)
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define MAX(a,b) ((a > b)?a:b)
#define MIN(a,b) ((a < b)?a:b)


#endif