/*
 * Copyright (c) 2014-2020, Yanzi Networks AB.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

 /**
  * \addtogroup uip
  * @{
  */


 /**
  * \file
  *
  * Default RPL NBR policy
  * decides when to add a new discovered node to the nbr table from RPL.
  *
  * \author Joakim Eriksson <joakime@sics.se>
  * Contributors: Niclas Finne <nfi@sics.se>, Oriol Piñol <oriol@yanzi.se>,
  *
  */

#include "net/routing/rpl-classic/rpl-nbr-policy.h"
#include "net/routing/rpl-classic/rpl-private.h"
#include "net/nbr-table.h"
#include "net/link-stats.h"
#include "net/ipv6/uip-ds6-route.h"
#include "sys/log.h"

#define LOG_MODULE "RPL-nbrpol"
#define LOG_LEVEL LOG_LEVEL_NONE

/*---------------------------------------------------------------------------*/
/* GMU-MI
 * Adding instance as argument for Multi Instances.
 * Rank is meaningless in an MI situation without that context.
 * If instance arg is NULL, revert to default_instance. */
//static rpl_rank_t
//get_rank(const linkaddr_t *lladdr)
static rpl_rank_t
get_rank(const linkaddr_t *lladdr, rpl_instance_t *instance)
{
  if(instance == NULL) {
    instance = default_instance;
  }

  rpl_parent_t *p = rpl_get_parent((uip_lladdr_t *)lladdr, instance);
  if(p == NULL) {
    return RPL_INFINITE_RANK;
  } else {
  //  rpl_instance_t *instance = rpl_get_default_instance();
    return instance != NULL ? instance->of->rank_via_parent(p) : RPL_INFINITE_RANK;
  }
}
/*---------------------------------------------------------------------------*/
const linkaddr_t *
rpl_nbr_gc_get_worst(const linkaddr_t *lladdr1, const linkaddr_t *lladdr2)
{
  /* GMU-MI
   * Instance information is needed, however, most functions leading to this
   *  do not have access to that information.  As this is only invoked
   *  if the table is full, I'm setting these get_rank calls to pass in NULL
   *  so the get_rank function will just use the default instance.
   */
  //return get_rank(lladdr2) > get_rank(lladdr1) ? lladdr2 : lladdr1;
  return get_rank(lladdr2, NULL) > get_rank(lladdr1, NULL) ? lladdr2 : lladdr1;
  /* GMU-MI END */
}
/*---------------------------------------------------------------------------*/
static bool
can_accept_new_parent(const linkaddr_t *candidate_for_removal, rpl_dio_t *dio)
{
  rpl_rank_t rank_candidate;

  /* GMU-MI
   * Replacing instance from DIO instead of default instance.
   * Updating get_rank to use instance Information
   */
  rpl_instance_t *instance = rpl_get_instance(dio->instance_id);
  if(instance == NULL) {
    instance = default_instance;
  }

  /* There's space left in the table or the worst entry has no rank: accept */
  if(candidate_for_removal == NULL
     || (rank_candidate = get_rank(candidate_for_removal, instance)) == RPL_INFINITE_RANK) {
    return true;
  } else {
    //rpl_instance_t *instance = rpl_get_default_instance();
    rpl_rank_t new_path_rank;

    if(instance == NULL || dio == NULL) {
      return false;
    }

    new_path_rank = dio->rank + instance->min_hoprankinc;
    return new_path_rank < rank_candidate - instance->min_hoprankinc / 2;
  }
}
/*---------------------------------------------------------------------------*/
bool
rpl_nbr_can_accept_new(const linkaddr_t *new, const linkaddr_t *candidate_for_removal,
                       nbr_table_reason_t reason, void *data)
{
  bool accept;
  switch(reason) {
  case NBR_TABLE_REASON_RPL_DIO:
    accept = can_accept_new_parent(candidate_for_removal, (rpl_dio_t *)data);
    break;
  case NBR_TABLE_REASON_ROUTE:
  case NBR_TABLE_REASON_RPL_DAO:
    /* Stop adding children if there is no space for nexthop neighbors,
     * regardless of whether the table if full or not */
    accept = rpl_nbr_policy_get_free_nexthop_neighbors() > 0;
    break;
  case NBR_TABLE_REASON_RPL_DIS:
  case NBR_TABLE_REASON_UNDEFINED:
  case NBR_TABLE_REASON_IPV6_ND:
  case NBR_TABLE_REASON_MAC:
  case NBR_TABLE_REASON_LLSEC:
  case NBR_TABLE_REASON_LINK_STATS:
  case NBR_TABLE_REASON_IPV6_ND_AUTOFILL:
  default:
    /* Behavior for all but new RPL parents/children: accept anything until table is full. */
    accept = (candidate_for_removal == NULL);
    break;
  }
  LOG_DBG("%s new neighbor ", accept ? "accept" : "reject");
  LOG_DBG_LLADDR(new);
  LOG_DBG_(", reason %u, worst is ", reason);
  LOG_DBG_LLADDR(candidate_for_removal);
  LOG_DBG_(" (total free %u, free nexthop neighbors %u)\n",
           NBR_TABLE_MAX_NEIGHBORS - nbr_table_count_entries(),
           rpl_nbr_policy_get_free_nexthop_neighbors());
  return accept;
}
/*---------------------------------------------------------------------------*/
int
rpl_nbr_policy_get_free_nexthop_neighbors(void)
{
  return RPL_NBR_POLICY_MAX_NEXTHOP_NEIGHBORS - uip_ds6_route_count_nexthop_neighbors();
}
/*---------------------------------------------------------------------------*/
/** @}*/
