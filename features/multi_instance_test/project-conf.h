// RPL Constants
// Enables Multicast with Storing Mode (RPL-Classic has non MCAST Default)
#define RPL_CONF_MOP RPL_MOP_STORING_MULTICAST

// Allocates for 3 Instances. [Needed by the Bridges (O, M, B)]
#define RPL_CONF_MAX_INSTANCES  3

// Enable DAO-ACK Messages
#define RPL_CONF_WITH_DAO_ACK   1

//  Legacy
//#define PROCESS_CONF_NO_PROCESS_NAMES 1         // Saves RAM - No Proc Names Stored
