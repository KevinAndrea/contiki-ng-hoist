#include "../common.h"
#include "net/rpl/rpl-private.h"

/* Connections - 1 for each Instance */
static struct uip_udp_conn *messenger_conn;

static rpl_dag_t *dag;/* Establish a DAG for the new RPL instance to use */

/* Data Storage */
uint16_t data_storage[2][MESSENGER_STORAGE];
uint16_t next_store = 0;
uint16_t next_expected[15][4];
uint8_t next_bridge_index = 0;
uint8_t message_payload[4];

uint8_t current_data_index = 0;
int fd;
uint16_t bytes_written = 0;
/* Timers */

PROCESS(messenger_process, "Messenger process");
AUTOSTART_PROCESSES(&messenger_process);
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
    uint8_t bridge_id = UIP_IP_BUF->srcipaddr.u8[15];
    uint8_t sender_id = appdata[2];
    uint8_t rate = (appdata[3] >> 6) & 0x3;
    uint8_t count = (appdata[3] & 0x3f);
    uint16_t start_index = appdata[4];
    /* Extract pertinent information rom the packet itself */
    uint8_t hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;


    int i;
    int bridge_index = -1;
    for(i = 0; i < next_bridge_index; ++i)
    {
      if(next_expected[i][0] == bridge_id) {
        bridge_index = i;
        break;
      }
    }
    if(bridge_index == -1)
    {
      next_expected[next_bridge_index][0] = bridge_id;
      bridge_index = next_bridge_index++;
    }

    if(type == DATA_MESSENGER)
    {
      if(start_index > next_expected[bridge_index][1])
      {
//printf("[DBG] Next EXPECTED == %d, GOT %d.  Rejecting\n", next_expected[bridge_index][1], start_index);
        printf("[REJECT],%d,%d,%d,%d\n", sender_id, bridge_id, next_expected[bridge_index][1], start_index);
        return;        
      }        


      uint16_t *short_payload_ptr = (uint16_t *)&appdata[6];
      int8_t index;
      uint8_t payload_index = 0;
      if(next_store + (count * 2) > MESSENGER_STORAGE)
      {
//printf("[DBG] Buffer full, switching to next array and writing data\n");        
        fd = cfs_open("data.dat", CFS_APPEND);
        int i;
        /* Zeroize remaining blocks before storing */
        for(i = next_store; i < MESSENGER_STORAGE; ++i)
        {
          data_storage[current_data_index][i] = 0;
        }
        cfs_write(fd, data_storage[current_data_index], MESSENGER_STORAGE * 2);
        cfs_close(fd);
        bytes_written += MESSENGER_STORAGE * 2;
        current_data_index = (current_data_index + 1)%2;
        next_store = 0;
//printf("[DBG] ... Bytes written total == %d\n", bytes_written);        
      }
printf("[DBG] Messenger Received new Data...\n");   
printf("[DBG] ... From %d(via %d), Rate %d, Hops %d...\n", 
  sender_id, bridge_id, rate, hops);
      for(index = 0; index < count; index++)
      {
        data_storage[current_data_index][next_store++] = sender_id;
        data_storage[current_data_index][next_store++] = short_payload_ptr[payload_index++];
//  printf("[DBG] ...... Data == %d\n", short_payload_ptr[payload_index - 1]);        
      }
      short_payload_ptr = (uint16_t *)&appdata[4];

      next_expected[bridge_index][1] = short_payload_ptr[0] + count;
      next_expected[bridge_index][2] = 0;
      next_expected[bridge_index][3] = 0;
printf("[DBG] ... Next Expected[BI=%d] = %d (SPP[0] == %d, count == %d)\n", bridge_id,next_expected[bridge_index][1],short_payload_ptr[0],count);
printf("[DBG] ....  Next Store == %d\n", next_store);
      //[RXMES],sender,bridge,received
      printf("[RXMES],%d,%d,%d\n", sender_id, bridge_id, next_expected[bridge_index][1]);
    }
    else if(type == CHECK)
    {

if(uip_ds6_route_head() != NULL)
{
  printf("[DWN]");
  uip_ds6_route_t *r;
  for(r = uip_ds6_route_head();
    r != NULL;
    r = uip_ds6_route_next(r)) {
    printf(",(%d via ", r->ipaddr.u8[15]);
    printf("%d)", (uip_ds6_route_nexthop(r))->u8[15]);
  }
  printf("\n");
}

      if(next_expected[bridge_index][2] == next_expected[bridge_index][1])
      {
        next_expected[bridge_index][3]++;
        if(next_expected[bridge_index][3] > 4)
        {
printf("[DBG] 4 checks received, purging routes\n");
          rpl_remove_routes(dag);
          next_expected[bridge_index][3] = 0;
        }
      }
      next_expected[bridge_index][2] = next_expected[bridge_index][1];

      message_payload[0] = ACK;
      message_payload[1] = 4;
      uint16_t *short_ptr = (uint16_t *)&message_payload[2];
      short_ptr[0] = next_expected[bridge_index][1];
printf("[DBG] Check -  Next Expected[BI=%d] = %d\n", bridge_id, next_expected[bridge_index][1]);
      uip_udp_packet_sendto(messenger_conn, &message_payload, 4, 
        &UIP_IP_BUF->srcipaddr, UIP_HTONS(BRIDGE_PORT));
    }
    else if(type == SYN)
    {
      message_payload[0] = SYN;
      message_payload[1] = 4;
      message_payload[2] = 0;
      message_payload[3] = 0;
//printf("[DBG] SYN Replied\n");
      uip_udp_packet_sendto(messenger_conn, &message_payload, 4, 
        &UIP_IP_BUF->srcipaddr, UIP_HTONS(BRIDGE_PORT));
    }    
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(messenger_process, ev, data)
{
  /* IPv6 Address of the local device */
  uip_ipaddr_t ipaddr;
  /* Root Interface for this Sink */
  struct uip_ds6_addr *root_if;

  /* Begin the process, then yield to allow any other processes execute before starting this one */
  PROCESS_BEGIN();
  PROCESS_PAUSE();

#if NEED_FORMATTING
  cfs_coffee_format();
#endif

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
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, RPL_MESSENGER_OCTET,1);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);

  /* Set thesink's root interface */
  root_if = uip_ds6_addr_lookup(&ipaddr);
  if(root_if != NULL) 
  {
    /* Bridge instances use 'b' + the ID (last octet of MAC) as the  Instance ID */
    dag = rpl_set_root(RPL_MESSENGER_INSTANCE,(uip_ip6addr_t *)&ipaddr);
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
  } 
  else 
  {
    printf("failed to create a new RPL DAG\n");
  }

  /* This is a debug line to verify the settings were properly applied. */
  printf("[SETTINGS],%d,%d,%d,%d,%d\n", RPL_CONF_DEFAULT_LIFETIME_UNIT, RPL_CONF_DEFAULT_LIFETIME,
                                           RPL_CONF_DIO_INTERVAL_MIN,RPL_CONF_DIO_INTERVAL_DOUBLINGS,RDC_OFF);

/* If RDC_OFF is 1, this disables the RDC to use 100% duty cycle.  Else, ContikiMAC RDC */
#if RDC_OFF == 1
  NETSTACK_RDC.off(1);
#endif

  /* Set the transmission power */  
  cc2420_set_txpower(MESSENGER_TX_POWER);

/* Bind all of the ports.  Each Instance needs its own connection */
  messenger_conn = udp_new(NULL, UIP_HTONS(BRIDGE_PORT), NULL);
  udp_bind(messenger_conn, UIP_HTONS(MESSENGER_PORT));

  int i;
  for(i = 0; i < 15; ++i)
  {
    next_expected[i][0] = 0;
    next_expected[i][1] = 0;
  }

  SENSORS_ACTIVATE(button_sensor);

  /* Main Loop.  This never ends. */
  while(1) {
    /* Yield the process to wait for events */
    PROCESS_YIELD();
    /* 1) Check for incoming packet payloads */
    if(ev == tcpip_event) 
    {
      tcpip_handler();
    }
    else if (ev == sensors_event && data == &button_sensor) 
    {

      if(next_store != 0)
      {
        fd = cfs_open("data.dat", CFS_APPEND);
        /* Zeroize remaining blocks before storing */
        int i;
        for(i = next_store; i < MESSENGER_STORAGE; ++i)
        {
          data_storage[current_data_index][i] = 0;
        }
        cfs_write(fd, data_storage[current_data_index], MESSENGER_STORAGE * 2);
        cfs_close(fd);
        bytes_written += MESSENGER_STORAGE * 2;
        current_data_index = (current_data_index + 1)%2;
        next_store = 0;
      }

      printf("Bytes dumped == 0, Bytes written == %d\n", bytes_written);
      uint16_t bytes_dumped = 0;
      fd = cfs_open("data.dat", CFS_READ);
      int i;
      while(bytes_dumped < bytes_written)
      {
        cfs_read(fd, &data_storage[current_data_index], MESSENGER_STORAGE * 2);
        bytes_dumped += MESSENGER_STORAGE * 2;
        printf("[DBG]");
        for(i = 0; i < MESSENGER_STORAGE; i++)
        {
          printf("%d,", data_storage[current_data_index][i]);
          if(i%16 == 0)
            printf("\n");
        }
        printf("\n");
      }
      cfs_close(fd);      
    }      
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
