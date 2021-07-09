#include "../common.h"

/* Connections - 1 for each Instance */
static struct uip_udp_conn *observer_conn;

/* Data Storage */
uint16_t data_storage[OBSERVER_STORAGE];

uint16_t next_storage_index = 0;
/* Timers */

#define DEBUG DEBUG_PRINT

PROCESS(observer_process, "Observer process");
AUTOSTART_PROCESSES(&observer_process);
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  /* Raw pointer to the Payload */
  uint8_t *appdata;

  /* Check to see if this is newly arrived data */
  if(uip_newdata()) {
    appdata = (uint8_t *)uip_appdata; 
    /* Extract pertinent information from the payload */
    uint8_t type = appdata[0];
    uint8_t bridge_id = appdata[1];
    uint8_t sender_id = appdata[2];
    uint8_t rate = (appdata[3] >> 6) & 0x3;
    uint8_t count = (appdata[3] & 0x3f);
    /* Extract pertinent information rom the packet itself */
    uint8_t hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;

    /* Extract the single data element from the payload.
     *  This is a 16-bit value starting at appdata[6] */
    int16_t *short_ptr = (int16_t *)&appdata[6];


//printf("[DBG] Observer Received new Data...\n");   
//printf("[DBG] ... From %d(via %d), Rate %d, Hops %d, Data %d\n", 
//    sender_id, bridge_id, rate, hops, short_ptr[0]);

    // [RXOBS],from,via,totalrx
    printf("[RXOBS],%d,%d,%d\n", sender_id, bridge_id,next_storage_index);

    data_storage[next_storage_index++] = short_ptr[0];
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(observer_process, ev, data)
{
  /* IPv6 Address of the local device */
  uip_ipaddr_t ipaddr;
  /* Root Interface for this Sink */
  struct uip_ds6_addr *root_if;

  /* Begin the process, then yield to allow any other processes execute before starting this one */
  PROCESS_BEGIN();
  PROCESS_PAUSE();

  /* Activate the Button Sensor - for Debugging */
  SENSORS_ACTIVATE(button_sensor);

  /* Enable serial line input.*/
  #if CONTIKI_TARGET_Z1
    uart0_set_input(serial_line_input_byte);
  #else
    uart1_set_input(serial_line_input_byte);
  #endif
  serial_line_init();

  /* Set Local Address and Start up RPL */  
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, RPL_OBSERVER_OCTET,1);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);

  /* Set thesink's root interface */
  root_if = uip_ds6_addr_lookup(&ipaddr);
  if(root_if != NULL) 
  {
    rpl_dag_t *dag;/* Establish a DAG for the new RPL instance to use */
    /* Bridge instances use 'b' + the ID (last octet of MAC) as the  Instance ID */
    dag = rpl_set_root(RPL_OBSERVER_INSTANCE,(uip_ip6addr_t *)&ipaddr);
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
  } 
  else 
  {
    PRINTF("failed to create a new RPL DAG\n");
  }

  /* This is a debug line to verify the settings were properly applied. */
  printf("[SETTINGS],%d,%d,%d,%d,%d\n", RPL_CONF_DEFAULT_LIFETIME_UNIT, RPL_CONF_DEFAULT_LIFETIME,
                                           RPL_CONF_DIO_INTERVAL_MIN,RPL_CONF_DIO_INTERVAL_DOUBLINGS,RDC_OFF);

/* If RDC_OFF is 1, this disables the RDC to use 100% duty cycle.  Else, ContikiMAC RDC */
#if RDC_OFF == 1
  NETSTACK_RDC.off(1);
#endif

  /* Set the transmission power */  
  cc2420_set_txpower(OBSERVER_TX_POWER);

/* Bind all of the ports.  Each Instance needs its own connection */
  observer_conn = udp_new(NULL, UIP_HTONS(BRIDGE_PORT), NULL);
  udp_bind(observer_conn, UIP_HTONS(OBSERVER_PORT));

  /* Main Loop.  This never ends. */
  while(1) {
    /* Yield the process to wait for events */
    PROCESS_YIELD();
    /* 1) Check for incoming packet payloads */
    if(ev == tcpip_event) {
      tcpip_handler();
    }
    /* 2) Check for a button press. This triggers a
     *  Global RPL Repair.  Currently used to assess the efficacy
     *  of OF0's reform time. */
    else if (ev == sensors_event && data == &button_sensor) {
      PRINTF("Initiaing global repair\n");
      rpl_repair_root(RPL_OBSERVER_INSTANCE);
    }  
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
