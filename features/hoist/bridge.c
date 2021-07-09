#include "common.h"

/* Connections - 1 for each Instance */
struct uip_udp_conn *bridge_collector_conn;
struct uip_udp_conn *bridge_observer_conn;
struct uip_udp_conn *bridge_messenger_conn;

/* Timers */
struct etimer dio_timer;

/* Timeouts */
static struct etimer syn_timeout;
static struct etimer start_timeout_messenger;
static struct etimer start_timeout_observer;
static struct etimer terminate_timeout_messenger;
static struct etimer terminate_timeout_observer;
static struct etimer data_timeout;

/* IP Addresses */
uip_ipaddr_t my_ipaddr;
uip_ipaddr_t observer_ipaddr;
uip_ipaddr_t messenger_ipaddr;

/* Globals */
/* .. For all arrays, use indices [MESSENGER] and [OBSERVER] */
int8_t instance_table_indices[2] = {-1,-1};
/* .. Used for all timeouts to see if we should keep trying */
int8_t check_attempts[2] = {0};
uip_ds6_route_t *current_route[2] = {NULL,NULL};

/* Necessary to directly access the instance table */
extern rpl_instance_t instance_table[];

/* DEBUG Control */
#define DEBUG DEBUG_PRINT /* Enables PRINTF */

/* Begin the Bridge Process */
PROCESS(bridge_process, "Bridge process");
AUTOSTART_PROCESSES(&bridge_process);

/*---------------------------------------------------------------------------*/
/* Sends a control message to the proper destination based on type.
 *   The argument of type void * should be a uint8_t value of the TYPE
 */
static void 
send_control_message(void *ptr)
{
  uint8_t payload_buffer[4] = {0};
  /* [0] - Message Type */
  payload_buffer[0] = (uint8_t)ptr;
  /* [1] - Message Length */
  payload_buffer[1] = 4;
  /* [2] - My ID (IPv6 LSB) */
  payload_buffer[2] = my_ipaddr.u8[15];

  uint8_t route_index = 0; 

  /* Control Messages destined for the Messenger */
  if(payload_buffer[0] == SYN)
  {
    uip_udp_packet_sendto(bridge_messenger_conn, payload_buffer, 4, 
      &messenger_ipaddr, UIP_HTONS(MESSENGER_PORT));
    return;
  }
  /* There are no control messages for the Observer */
  /* Control Messages destined for the Collectors 
   *  Control Messages for Collectors are always sent only 
   *  to the current Collector in the sequence */
  if(payload_buffer[0] == START_MESSENGER || payload_buffer[0] == FIN_MESSENGER ||
          payload_buffer[0] == TERM_MESSENGER)
  {
    route_index = MESSENGER;
  }
  else if(payload_buffer[0] == START_OBSERVER || payload_buffer[0] == TERM_OBSERVER)
  {
    route_index = OBSERVER; 
  }

  if(payload_buffer[0] == START_MESSENGER || payload_buffer[0] == START_OBSERVER)
  {
    /* Used to ignore other bridges in the downward table.
         If another bridge receives this message, it will reply with
         [3] == INVALID.  On receipt of a response, if INVALID, we go to the next one. */
    payload_buffer[3] = BRIDGE;
  }
printf("[DBG] Sending type == %d, length == %d to ID: %d\n", payload_buffer[0], payload_buffer[1], current_route[route_index]->ipaddr.u8[15]);
  uip_udp_packet_sendto(bridge_collector_conn, payload_buffer, 4, 
    &current_route[route_index]->ipaddr, UIP_HTONS(COLLECTOR_PORT)); 
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  /* Extract the payload */
  uint8_t *appdata;
  appdata = (uint8_t *)uip_appdata;

  /* Get the message header information */
  uint8_t type = appdata[0];
  uint8_t len = appdata[1];
  uint8_t sender = appdata[2];
  uint8_t options = appdata[3];
//printf("[DBG] Got Message, type == %d\n", type);
  /* Process the type (this is a basic state machine, each type only has 
   *   one entry path.) */
   switch(type)
   {
      /* Received a SYN reply from the Messenger */
      case SYN:
//printf("[DBG] Got Synch Reply, starting Collectors\n"); 
        printf("[STARTMES]\n");  
        /* Set the Messenger's current route to the beginning of the downward routing 
         *  table, and start the messenger data transfer */
        etimer_stop(&syn_timeout);
        current_route[MESSENGER] = uip_ds6_route_head();
        etimer_set(&start_timeout_messenger, START_TIMEOUT_INT);
        check_attempts[MESSENGER] = 0;
        send_control_message((void *) START_MESSENGER);
        return;
        break;
      /* Received a STA initiation/reply from Collector/Bridge */
      case START_MESSENGER:
printf("[DBG] Received Start Messenger reply [options == %d] from %d, going to next one\n", options, sender);
        /* Got a STA initiation/reply from a Bridge, handle and move on */
        etimer_stop(&start_timeout_messenger);

        /* Got a STA request from another Bridge, let it know */
        if(options == BRIDGE)
        {
          uint8_t message_payload[4] = {START_MESSENGER, 4, my_ipaddr.u8[15], INVALID};
          uip_udp_packet_sendto(bridge_collector_conn, &message_payload, 4, 
            &UIP_IP_BUF->srcipaddr, UIP_HTONS(BRIDGE_PORT));
          return;
        }
        else
        {
          etimer_set(&data_timeout, DATA_TIMEOUT);
        }
        break;
      /* Received a STA initiation/reply from Collector/Bridge */
      case START_OBSERVER:
printf("[DBG] Received Start Observer reply [options == %d]", options);
        /* Got a STA initiation/reply from a Bridge, handle and move on */
        etimer_stop(&start_timeout_observer);
        /* Got a STA reply from a bridge.  Ignore it and move to the next device */
        if(options != BRIDGE)
        {
          current_route[OBSERVER] = uip_ds6_route_next(current_route[OBSERVER]);
          /* Next route is valid */
          if(current_route[OBSERVER] != NULL)
          {
            etimer_set(&start_timeout_observer, START_TIMEOUT_INT);
            check_attempts[OBSERVER] = 0;
            send_control_message((void *) START_OBSERVER);
            return;
          }
        }
        /* Got a STA request from another Bridge, let it know */
        else if(options == BRIDGE)
        {
          uint8_t message_payload[4] = {START_OBSERVER, 4, my_ipaddr.u8[15], INVALID};
          uip_udp_packet_sendto(bridge_collector_conn, &message_payload, 4, 
            &UIP_IP_BUF->srcipaddr, UIP_HTONS(BRIDGE_PORT));
          return;
        }
        break;
      /* Got a data packet to send to the messenger. */
      case DATA_MESSENGER:
        etimer_restart(&data_timeout);
        /* Need to add our address to the packet */
        appdata[1] = my_ipaddr.u8[15]; 
        len = appdata[3] & 0x3f;
        len *= 2;
        len += 6;
printf("[DBG] Got %d bytes for the Messenger from %d, fowarding\n", len, sender);
        uip_udp_packet_sendto(bridge_messenger_conn, appdata, len, 
          &messenger_ipaddr, UIP_HTONS(MESSENGER_PORT));
        break;
      /* Got a data packet to send to the observer. */
      case DATA_OBSERVER:
printf("[DBG] Got data for the Observer, sending upward.\n");  
        /* Need to add our address to the packet */
        appdata[1] = my_ipaddr.u8[15];    
        uip_udp_packet_sendto(bridge_observer_conn, appdata, 8, 
          &observer_ipaddr, UIP_HTONS(OBSERVER_PORT));
        break;
      /* Got a CHECK message. Forward to Messenger, and restart the 
           data timeout. */
      case CHECK:
printf("[DBG] Got CHECK for the Messenger from %d, sending upward.\n", sender); 

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

        etimer_restart(&data_timeout);
        uip_udp_packet_sendto(bridge_messenger_conn, appdata, len, 
          &messenger_ipaddr, UIP_HTONS(MESSENGER_PORT));
        break;
      /* Got an ACK.  Forward it to the current collector */
      case ACK:
printf("[DBG] Got ACK from the Messenger, sending to %d.\n", current_route[MESSENGER]->ipaddr.u8[15]);
        uip_udp_packet_sendto(bridge_collector_conn, appdata, len, 
          &current_route[MESSENGER]->ipaddr, UIP_HTONS(COLLECTOR_PORT));
        break;
      /* Got a FIN from the Collector.  Reply and start the next collector. */
      case FIN_MESSENGER:
printf("[DBG] Got a FIN from %d\n", sender);  
        /* Only act if this is the FIN we're expecting */
        if(sender == current_route[MESSENGER]->ipaddr.u8[15])
        {    
          etimer_stop(&data_timeout);
          send_control_message((void *) FIN_MESSENGER);
          current_route[MESSENGER] = uip_ds6_route_next(current_route[MESSENGER]);
          /* Next route is valid */
          if(current_route[MESSENGER] != NULL)
          {
            etimer_set(&start_timeout_messenger, START_TIMEOUT_INT);
            check_attempts[MESSENGER] = 0;
            send_control_message((void *) START_MESSENGER);
            return;
          }        
        }
        /* Otherwise, we've already seen it, so send confirm back */
        else
        {
          uip_udp_packet_sendto(bridge_collector_conn, appdata, 4, 
            &UIP_IP_BUF->srcipaddr, UIP_HTONS(COLLECTOR_PORT));
        }
        break;
      /* Got a TERM_MESSENGER reply from a Collector */
      case TERM_MESSENGER:
printf("[DBG] Got a TERM MESSENGER from %d\n", sender);  
        etimer_stop(&terminate_timeout_messenger);
        current_route[MESSENGER] = uip_ds6_route_next(current_route[MESSENGER]);
        /* Next route is valid */
        if(current_route[MESSENGER] != NULL)
        {
          etimer_set(&start_timeout_messenger, START_TIMEOUT_INT);
          check_attempts[MESSENGER] = 0;
          send_control_message((void *) TERM_MESSENGER);
          return;
        }
        break;
      /* Got a TERM_OBSERVER reply from a Collector */
      case TERM_OBSERVER:
printf("[DBG] Got a TERM OBSERVER from %d\n", sender);        
        etimer_stop(&terminate_timeout_observer);
        current_route[OBSERVER] = uip_ds6_route_next(current_route[OBSERVER]);
        /* Next route is valid */
        if(current_route[OBSERVER] != NULL)
        {
          etimer_set(&start_timeout_observer, START_TIMEOUT_INT);
          check_attempts[OBSERVER] = 0;
          send_control_message((void *) TERM_OBSERVER);
          return;
        }
        break;   
      case FULL:
        etimer_stop(&data_timeout);
        etimer_set(&terminate_timeout_messenger, TERM_TIMEOUT);
        send_control_message((void *)TERM_MESSENGER);
        break;
      /* Got something weird, ignore it */
      default: 
        break;
   }
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  /* Set the mobile sink addresses.  These will work upward only. */
  uip_ip6addr(&observer_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, RPL_OBSERVER_OCTET, 1);
  uip_ip6addr(&messenger_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, RPL_MESSENGER_OCTET, 1);
  /* Set my address.  This will *not* be used in any downward routes. */
  uip_ip6addr(&my_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&my_ipaddr, &uip_lladdr);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bridge_process, ev, data)
{
  /* IPv6 Address of the local device */
  uip_ipaddr_t ipaddr;
  /* Root Interface for this Sink */
  struct uip_ds6_addr *root_if;

  /* Begin the process, then yield to allow any other processes execute before starting this one */
  PROCESS_BEGIN();
  PROCESS_PAUSE();

  /* Enable serial line input.*/
  #if CONTIKI_TARGET_Z1
    uart0_set_input(serial_line_input_byte);
  #else
    uart1_set_input(serial_line_input_byte);
  #endif
  serial_line_init();

  /* Set Local Address and Start up RPL */  
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0,0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  /* Set the sink's root interface */
  root_if = uip_ds6_addr_lookup(&ipaddr);
  if(root_if != NULL) 
  {
    rpl_dag_t *dag; /* Establish a DAG for the new RPL instance to use */
    /* Bridge instances use 'b' + the ID (last octet of MAC) as the  Instance ID */
    dag = rpl_set_root('b' + ipaddr.u8[15],(uip_ip6addr_t *)&ipaddr);
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

  /* Set the global IP addresses (Observer, Messenger, and Local) */
  set_global_address();

  /* If RDC_OFF is 1, this disables the RDC to use 100% duty cycle.  Else, ContikiMAC RDC */
#if RDC_OFF == 1
  NETSTACK_RDC.off(1);
#endif

  /* Set the transmission power */
  cc2420_set_txpower(BRIDGE_TX_POWER);

  /* Bind all of the ports.  Each Instance needs its own connection */
  /* ... Bridge -> Collector */
  bridge_collector_conn = udp_new(NULL, UIP_HTONS(COLLECTOR_PORT), NULL);
  udp_bind(bridge_collector_conn, UIP_HTONS(BRIDGE_PORT));

  /* ... Observer -> Bridge */
  bridge_observer_conn = udp_new(NULL, UIP_HTONS(OBSERVER_PORT), NULL);
  udp_bind(bridge_observer_conn, UIP_HTONS(BRIDGE_PORT));  

  /* ... Messenger -> Bridge */
  bridge_messenger_conn = udp_new(NULL, UIP_HTONS(MESSENGER_PORT), NULL);
  udp_bind(bridge_messenger_conn, UIP_HTONS(BRIDGE_PORT)); 

  /* Set up a timer to check for DIO changes. Run once a second */
  etimer_set(&dio_timer, CLOCK_SECOND);

  /* Main Loop.  This never ends. */
  while(1) 
  {
    /* Yield the process to wait for events */
    PROCESS_YIELD();
    /* 1) Check for incoming packet payloads */
    if(ev == tcpip_event) 
    {
      /* Received data addressed to this device, handle it. */
      tcpip_handler();
    }
    /* 2) Check for expiring timers */
    else if(ev == PROCESS_EVENT_TIMER) 
    {
      int index = 0;
      /* DIO timer has gone off, check for new instances */
      if(data == &dio_timer)
      {
//printf("[DBG] ------ [0].used == %d, .iid == %d\n", instance_table[0].used, instance_table[0].instance_id);        
//printf("[DBG] ------ [1].used == %d, .iid == %d\n", instance_table[1].used, instance_table[1].instance_id);
//printf("[DBG] ------ [2].used == %d, .iid == %d\n", instance_table[2].used, instance_table[2].instance_id);
//printf("[DBG] -----------Indices[Mess] == %d, OBS = %d\n", instance_table_indices[MESSENGER], instance_table_indices[OBSERVER]);
        uint8_t found_observer = 0;
        uint8_t found_messenger = 0;
        /* Check each instance for membership */
        for(index = 0; index < 3; ++index)
        {
          /* Check each instance in the table */
          if(instance_table[index].used == 1)
          {
            /* Handle the Instance detected */
            switch(instance_table[index].instance_id)
            {
              case 'O':
                found_observer = 1;
                /* If this is a known instance, ignore.  Else, add */
                if(instance_table_indices[OBSERVER] == -1)
                {             
                  instance_table_indices[OBSERVER] = index;
                  current_route[OBSERVER] = uip_ds6_route_head();
                  /* Next route is valid */
                  if(current_route[OBSERVER] != NULL)
                  {
                    etimer_set(&start_timeout_observer, START_TIMEOUT_INT);
                    check_attempts[OBSERVER] = 0;
                    send_control_message((void *) START_OBSERVER);
                    printf("[STARTOBS]\n");  
                  }              
                }
                break;
              case 'M':
                found_messenger = 1;
                if(instance_table_indices[MESSENGER] == -1)
                {
                  instance_table_indices[MESSENGER] = index;
                  send_control_message((void *)SYN);
                  etimer_set(&syn_timeout, SYN_TIMEOUT);
                  check_attempts[MESSENGER] = 0;
                }   
                break;
            }
          }
          /* This is an unused index, was it used before? */
          else {
            if(instance_table_indices[OBSERVER] != -1 && found_observer == 0)
            {
//printf("[DBG] ... Lost 'O'\n");
              printf("[DEATHOBS]\n");
              instance_table_indices[OBSERVER] = -1;
              etimer_set(&terminate_timeout_observer, TERM_TIMEOUT);
              current_route[OBSERVER] = uip_ds6_route_head();
              send_control_message((void *)TERM_OBSERVER);
            }
            else if(instance_table_indices[MESSENGER] != -1 && found_messenger == 0)
            {            
//printf("[DBG] ... Lost 'M'\n");
              printf("[DEATHMES]\n");
              leds_off(LEDS_GREEN);
              instance_table_indices[MESSENGER] = -1;
              etimer_set(&terminate_timeout_messenger, TERM_TIMEOUT);
              current_route[MESSENGER] = uip_ds6_route_head();
              send_control_message((void *)TERM_MESSENGER);
            }
          }
        }
        etimer_restart(&dio_timer);
      }
      /* Timeout on the SYN message to the MESSENGER */
      else if(data == &syn_timeout)
      {  
//printf("[DBG] - SYN Timeout Triggered\n");        
        if(check_attempts[MESSENGER] < 128)
        {
          check_attempts[MESSENGER]++;
          send_control_message((void *)SYN);
          etimer_restart(&syn_timeout);
        }
      }
      else if(data == &start_timeout_messenger)
      {  
//printf("[DBG] - Start(M) Timeout Triggered\n");        
        if(check_attempts[MESSENGER] < MAX_ATTEMPTS)
        {
          check_attempts[MESSENGER]++;
          send_control_message((void *)START_MESSENGER);
          etimer_restart(&start_timeout_messenger);
        }
        else {
          current_route[MESSENGER] = uip_ds6_route_next(current_route[MESSENGER]);
          /* Next route is valid */
          if(current_route[MESSENGER] != NULL)
          {
            etimer_set(&start_timeout_messenger, START_TIMEOUT_INT);
            check_attempts[MESSENGER] = 0;
            send_control_message((void *) START_MESSENGER);
          }
        }
      }
      else if(data == &terminate_timeout_messenger)
      {  
//printf("[DBG] - Term(M) Timeout Triggered\n");        
        if(check_attempts[MESSENGER] < MAX_ATTEMPTS)
        {
          check_attempts[MESSENGER]++;
          send_control_message((void *)TERM_MESSENGER);
          etimer_restart(&terminate_timeout_messenger);
        }
        else {
          current_route[MESSENGER] = uip_ds6_route_next(current_route[MESSENGER]);
          /* Next route is valid */
          if(current_route[MESSENGER] != NULL)
          {
            etimer_set(&terminate_timeout_messenger, TERM_TIMEOUT);
            check_attempts[MESSENGER] = 0;
            send_control_message((void *) TERM_MESSENGER);
          }
        }
      }  
      else if(data == &terminate_timeout_observer)
      {  
//printf("[DBG] - Term(O) Timeout Triggered\n");        
        if(check_attempts[OBSERVER] < MAX_ATTEMPTS)
        {
          check_attempts[OBSERVER]++;
          send_control_message((void *)TERM_OBSERVER);
          etimer_restart(&terminate_timeout_observer);
        }
        else {
          current_route[OBSERVER] = uip_ds6_route_next(current_route[OBSERVER]);
          /* Next route is valid */
          if(current_route[OBSERVER] != NULL)
          {
            etimer_set(&terminate_timeout_observer, TERM_TIMEOUT);
            check_attempts[OBSERVER] = 0;
            send_control_message((void *) TERM_OBSERVER);
          }
        }
      }                
      else if(data == &start_timeout_observer)
      {  
//printf("[DBG] - Start(O) Timeout Triggered\n");           
        if(check_attempts[OBSERVER] < MAX_ATTEMPTS)
        {
          check_attempts[OBSERVER]++;
          send_control_message((void *)START_OBSERVER);
          etimer_restart(&start_timeout_observer);
        }
        else {
          current_route[OBSERVER] = uip_ds6_route_next(current_route[OBSERVER]);
          /* Next route is valid */
          if(current_route[OBSERVER] != NULL)
          {
            etimer_set(&start_timeout_observer, START_TIMEOUT_INT);
            check_attempts[OBSERVER] = 0;
            send_control_message((void *) START_OBSERVER);
          }
        } 
      }     
    } // end ev == PROCESS_EVENT_TIMER
  }// end while(1)

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
