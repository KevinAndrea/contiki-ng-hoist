#include "../common.h"
#include "dev/tmp102.h"


/* Connections - 1 for each Instance */
struct uip_udp_conn *collector_conn;

/* Timers */
struct ctimer collect_timer;
struct ctimer data_timer;

/* Timeouts */
static struct etimer finish_timeout;
static struct etimer check_timeout;

/* Buffers */
uint8_t control_payload_buffer[4];
uint8_t observer_payload_buffer[8];
uint8_t messenger_payload_buffer[58];

/* IP Addresses */
uip_ipaddr_t my_ipaddr;

/* Globals */
/* .. Used for all timeouts to see if we should keep trying */
int8_t check_attempts = 0;
uip_ds6_route_t *current_bridge = NULL;
uint8_t finished_flag = 0;

/* Necessary to directly access the instance table */
extern rpl_instance_t instance_table[];

/* Global flag to track Observer status */
uint8_t observer_online = 0;

/* Data Collection Array */
int16_t data_array[COLLECTOR_STORAGE];
uint16_t next_send = 0;
uint16_t next_expected = 0;
uint16_t next_collect = 0;
uint16_t blocks_sent = 0;
int16_t last_start = -1;

extern rpl_instance_t instance_table[];

PROCESS(collector_process, "Collector process");
AUTOSTART_PROCESSES(&collector_process);
/*---------------------------------------------------------------------------*/
static void 
send_control_message(void *ptr)
{
  /* [0] - Message Type */
  control_payload_buffer[0] = (uint16_t)ptr;
  /* [1] - Message Length */
  control_payload_buffer[1] = 4;
  /* [2] - My ID (IPv6 LSB) */
  control_payload_buffer[2] = my_ipaddr.u8[15];

//printf("[DBG] Sending type == %d, length == %d to the Bridge\n", control_payload_buffer[0], control_payload_buffer[1]);

  uip_udp_packet_sendto(collector_conn, control_payload_buffer, 4, 
    &instance_table[0].current_dag->dag_id, UIP_HTONS(BRIDGE_PORT)); 
}
/*---------------------------------------------------------------------------*/
static void
collect_data(void *ptr) 
{
  /* In real life, a sensor would sense stuff here */
  /*  Instead, we'll 'collect' temperatures equal to the index */
  uint16_t data = next_collect;
  /* Store the data */
  data_array[next_collect++] = data;

  uint16_t send_initial = (uint16_t)ptr;
printf("[DBG] Collected on index %d: data = %d\n", next_collect - 1, data);
  if(observer_online == 1)
  {
printf("[DBG] .. Sending to Observer\n");
    uint16_t *short_pointer = (uint16_t *)&observer_payload_buffer[4];
    observer_payload_buffer[0] = DATA_OBSERVER;
    observer_payload_buffer[1] = 0;
    observer_payload_buffer[2] = my_ipaddr.u8[15];
    /* buffer[3]: Upper 4-bits = Send Rate,Lower = Data Count */
    observer_payload_buffer[3] = (uint8_t) (SEND_RATE / CLOCK_SECOND);
    observer_payload_buffer[3] <<= 4;
    observer_payload_buffer[3] += (uint8_t)1;
    /* Buffer[4..5] = Start Index */
    short_pointer[0] = 0;
    /* Buffer[6..7] = Data 1 */
    short_pointer[1] = data;

    printf("[TXOBS],%d,%d\n", instance_table[0].current_dag->dag_id.u8[15],next_collect);

    /* Send to the Observer */
    uip_udp_packet_sendto(collector_conn, observer_payload_buffer, 8,
      &instance_table[0].current_dag->dag_id, UIP_HTONS(BRIDGE_PORT));
  }
  
  /* On first contact, only send current data, no rescheduling */
  if(send_initial == 0)
  {
   // ctimer_set(&collect_timer, COLLECT_RATE + BACKOFF(), collect_data, (void *) 0);
    ctimer_restart(&collect_timer);
  }
}
/*---------------------------------------------------------------------------*/
static void
send_data(void *ptr)
{
//printf("[DBG] SEND DATA (data timer) just procced\n");
  /* If we have confirmed receipt of everything we have, send FIN */
  if(next_expected == next_collect)
  {
//printf("[DBG] Triggering FINISH\n");    
    finished_flag = 1;
    printf("[FINISHED],%d\n", next_expected);
    send_control_message((void *) FIN_MESSENGER);
    etimer_set(&finish_timeout, FIN_TIMEOUT);
    check_attempts = 0;
  }
  /* If we've sent everything or sent our max blocks, ask for check */
  else if (next_send == next_collect || blocks_sent >= MAX_BLOCKS)
  {
//printf("[DBG] Triggering CHECK\n");    
    send_control_message((void *) CHECK);
    etimer_set(&check_timeout, CHECK_TIMEOUT);
    check_attempts = 0;
  }
  /* Otherwise, load up the next block */
  else 
  {    
    finished_flag = 0;
//printf("[DBG] Beginning mess send process... next_send == %d\n", next_send);
    /* 1: Find out how much we need to send */
    uint16_t count_to_send = next_collect - next_send;
    uint16_t upper_index = next_send + MIN(count_to_send, DATA_PER_BLOCK);
    count_to_send = MIN(count_to_send, DATA_PER_BLOCK);
//printf("[DBG] Beginning mess send process... next_send == %d\n", next_send);
    /* 2: Prep the buffer and header */
    uint8_t payload_length = (count_to_send * 2) + 6;
//printf("[DBG] Creating a payload buffer of size %d\n", payload_length);    
    uint16_t *short_pointer = (uint16_t *)&messenger_payload_buffer[4];
    messenger_payload_buffer[0] = DATA_MESSENGER;
    messenger_payload_buffer[1] = 0;
    messenger_payload_buffer[2] = my_ipaddr.u8[15];
    /* buffer[3]: Upper 4-bits = Send Rate,Lower = Data Count */
    messenger_payload_buffer[3] = (uint8_t)(SEND_RATE / CLOCK_SECOND);
    messenger_payload_buffer[3] <<= 6;
    short_pointer[0] = next_send;

    /* 3: Load em up */
//printf("[DBG] Buffer == %x, CTS == %x\n", messenger_payload_buffer[3], count_to_send);
    messenger_payload_buffer[3] += (uint8_t) count_to_send;
//printf("[DBG] Buffer == %x\n", messenger_payload_buffer[3]);    

//printf("[DBG] Sending %d(%d) datas to the Messenger from [%d] to [%d]\n", count_to_send, messenger_payload_buffer[3]&0x3f, next_send, upper_index);

    int index;
    int pointer_index = 1;
    for(index = next_send; index < upper_index; ++index)
    {
//printf("[DBG] ... Sending [%d] = %d\n", index, data_array[index]);      
      short_pointer[pointer_index] = data_array[index];
      pointer_index++;
    }
    next_send = upper_index;
    blocks_sent++;
//printf("[DBG] ... Next Send == %d\n", next_send);
    /* Send to the Messenger */
printf("[DBG] Sending to the bridge: %d, %d bytes\n", instance_table[0].current_dag->dag_id.u8[15], payload_length);

    if(last_start != short_pointer[0])
    {
      printf("[TXMES],%d,%d,%d\n",instance_table[0].current_dag->dag_id.u8[15],short_pointer[0], count_to_send);
      last_start = short_pointer[0];
    }
    else
    {
      printf("[TXMREPEAT],%d,%d,%d\n",instance_table[0].current_dag->dag_id.u8[15],short_pointer[0], count_to_send);
    }

    uip_udp_packet_sendto(collector_conn, messenger_payload_buffer, payload_length,
      &instance_table[0].current_dag->dag_id, UIP_HTONS(BRIDGE_PORT));
printf("[DBG] Restarting data timer\n");
    ctimer_restart(&data_timer);
printf("[DBG] Returning\n");    
    //ctimer_set(&data_timer, SEND_RATE + BACKOFF(), send_data, NULL);
  }
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
//printf("[DBG] Got message, type == %d\n", type);
  /* Process the type (this is a basic state machine, each type only has 
   *   one entry path.) */
  switch(type)
  {
    case START_MESSENGER:
//printf("[DBG] Ordered to start Messenger\n");   
      printf("[STARTED]\n");     
      leds_off(LEDS_RED);
      finished_flag = 0;
      ctimer_stop(&collect_timer);
      ctimer_stop(&data_timer);
      etimer_stop(&check_timeout);
      etimer_stop(&finish_timeout);
  
      send_control_message((void *) START_MESSENGER);

      next_send = next_expected;
      blocks_sent = 0;

      uint16_t timing = SEND_RATE + BACKOFF();
printf("[DBG]  Setting data timer to %d\n", timing);      
      ctimer_set(&data_timer, timing, send_data, NULL);
///// Separate send_rate and data_coll_rate
      break;
    case START_OBSERVER:
printf("[DBG] Ordered to start Observer\n");    
      send_control_message((void *) START_OBSERVER);
      observer_online = 1;
      collect_data((void*)1);
      break;
    case ACK:
      etimer_stop(&check_timeout);
      uint16_t *acknowledged = (uint16_t *)&appdata[2];
      next_expected = acknowledged[0];
      next_send = next_expected;
      ctimer_restart(&data_timer);
      blocks_sent = 0;
      printf("[RXACK],%d\n", next_expected);
printf("[DBG] Received Acknowledgement, next expected == %d\n", next_expected);      
      break;
    case FIN_MESSENGER:
      if(finished_flag == 1)
      {
printf("[DBG] FINISHED - I'm done and the Bridge knows it. Next send == %d\n", next_send);    
        leds_on(LEDS_RED);
      }
      else 
      {
printf("[DBG] FINISHED - With Termination! Next send == %d\n", next_send);    
      }
      ctimer_stop(&collect_timer);
      ctimer_stop(&data_timer);
      etimer_stop(&check_timeout);
      etimer_stop(&finish_timeout);

      blocks_sent = 0;

      ctimer_restart(&collect_timer);
      //ctimer_set(&collect_timer, COLLECT_RATE + BACKOFF(), collect_data, (void *) 0);
      break;
    case TERM_MESSENGER:
printf("[DBG] Ordered to terminate contact with Messenger\n");    
      leds_off(LEDS_RED);
      ctimer_stop(&collect_timer);
      ctimer_stop(&data_timer);
      etimer_stop(&check_timeout);
      etimer_stop(&finish_timeout);

      send_control_message((void *) TERM_MESSENGER);
      blocks_sent = 0;

      ctimer_set(&collect_timer, COLLECT_RATE + BACKOFF(), collect_data, (void *) 0);      
      break;
    case TERM_OBSERVER:
printf("[DBG] Ordered to terminate contact with Observer\n");       
      send_control_message((void *) TERM_OBSERVER);
      observer_online = 0;
      break;
  }  
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(collector_process, ev, data)
{
  PROCESS_BEGIN();
  PROCESS_PAUSE();

  #if CONTIKI_TARGET_Z1
    uart0_set_input(serial_line_input_byte);
  #else
    uart1_set_input(serial_line_input_byte);
  #endif
    serial_line_init();

  node_id_restore(); 
  uip_ip6addr(&my_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, RPL_COLLECTOR_OCTET, node_id);

  cc2420_set_txpower(COLLECTOR_TX_POWER);

  collector_conn = udp_new(NULL, UIP_HTONS(BRIDGE_PORT), NULL);
  udp_bind(collector_conn, UIP_HTONS(COLLECTOR_PORT));

  /* This is a debug line to verify the settings were properly applied. */
  printf("[SETTINGS],%d,%d,%d,%d,%d\n", MAX_BLOCKS, SEND_INTERVAL,STARTING_DATA,0,0);

  int i;
  for(i = 0; i < STARTING_DATA; i++)
  {
    data_array[next_collect++] = i;
  }

  ctimer_set(&collect_timer, COLLECT_RATE + BACKOFF() + random_rand()%CLOCK_SECOND, collect_data, (void *) 0);

  while(1) 
  {
    PROCESS_YIELD();
    if(ev == tcpip_event) 
    {
      tcpip_handler();
    }
    else if(ev == PROCESS_EVENT_TIMER) 
    {
      if(data == &check_timeout)
      {
        if(check_attempts >= MAX_ATTEMPTS)
        {
printf("[DBG] Check attempts exceeded, beginning loooooong check before restarting.\n");          
          check_attempts = 0;
          send_control_message((void *) CHECK);
          etimer_set(&check_timeout, CLOCK_SECOND * 20);
        }
        else
        {
          check_attempts++;
          send_control_message((void *) CHECK);
          etimer_restart(&check_timeout);
        }
      }
      else if(data == &finish_timeout)
      {
        if(check_attempts < MAX_ATTEMPTS)
        {
          check_attempts++;
          send_control_message((void *)FIN_MESSENGER);
          etimer_restart(&finish_timeout);
        }
        else
        {
printf("[DBG] Finish attempts exceeded, resuming collection.\n");          
          ctimer_set(&collect_timer, CLOCK_SECOND * COLLECT_RATE, collect_data, (void *) 0);
        }
      }    
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
