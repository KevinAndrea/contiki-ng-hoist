#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

// Adding Includes for Multi-Instance
#include "net/routing/rpl-classic/rpl.h"  // rpl_instance_t
#include "net/ipv6/uip-udp-packet.h"      // uip_udp_packet_sendto

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#ifndef SEND_INTERVAL_SECONDS
#define SEND_INTERVAL_SECONDS 60
#endif

#define SEND_INTERVAL		  (SEND_INTERVAL_SECONDS * CLOCK_SECOND)

//static struct uip_udp_conn *server_a_conn;
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t client_ipaddr;
//static struct uip_udp_conn *server_b_conn;

//static struct simple_udp_connection udp_conn;

// Two Instances (A = 0, B = 1)
//static int8_t instance_table_indices[2] = {-1, -1};

// Needed to access the DAG ID for multi-instance
extern rpl_instance_t instance_table[];
extern rpl_instance_t *default_instance;
/*---------------------------------------------------------------------------*/
PROCESS(multi_client_process, "Multi-Instance Test Client");
AUTOSTART_PROCESSES(&multi_client_process);
/*---------------------------------------------------------------------------*/
/*
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");

}
*/

static void
tcpip_handler(void)
{
  LOG_INFO("Received Packet\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(multi_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static unsigned count;
  static char str[32];
  static unsigned long interval;
  static int i;
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
//  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
//                      UDP_SERVER_PORT, udp_rx_callback);

// IP Address set to AAAA::3 for Client
  uip_ip6addr(&client_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 3);

// Bind the Ports
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  while(1) {
    //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    PROCESS_YIELD();
    LOG_INFO("ev == %d (TE == %d, PET == %d)\n", ev, tcpip_event, PROCESS_EVENT_TIMER);
    if(ev == tcpip_event) {
      LOG_INFO("TCPIP_EVENT Triggered\n");
      tcpip_handler();

    }
    else if(ev == PROCESS_EVENT_TIMER) {
      LOG_INFO("Send Timer Triggered... %d, %d\n",
                NETSTACK_ROUTING.node_is_reachable(),
                NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr));
      if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
        /* Send to DAG root */
        LOG_INFO("Sending request %u to ", count);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        LOG_INFO("Printing Instances.  MAX = %d\n", RPL_MAX_INSTANCES);
        LOG_INFO("--Default Inst:   0x%x\n", default_instance->instance_id);
        for(i = 0; i < RPL_MAX_INSTANCES; i++) {
          LOG_INFO("Instance Index:   %d\n", i);
          LOG_INFO("-- Used:          %d\n", instance_table[i].used);
          LOG_INFO("-- Instance ID: 0x%x\n", instance_table[i].instance_id);
        }
        snprintf(str, sizeof(str), "hello %d", count);
        //simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
        uip_udp_packet_sendto(client_conn, str, sizeof(str),
                              &instance_table[count%2].current_dag->dag_id,
                              UIP_HTONS(UDP_SERVER_PORT));
        LOG_INFO("SENDING TO IDX: %d --- %s\n", count%2,str);
        count++;
      } else {
        LOG_INFO("Not reachable yet\n");
      }

      interval = SEND_INTERVAL - CLOCK_SECOND
                 + (random_rand() % (2 * CLOCK_SECOND));

  /* Add some jitter */
      LOG_INFO("Send Timer Set... %lu ticks (%lu sec)", interval, interval/CLOCK_SECOND);
      etimer_set(&periodic_timer, interval);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
