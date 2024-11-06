#ifndef LSUTILS_H
#define LSUTILS_H

#include <unordered_map>
#include <global.h>
#include <sharedUtils.h>

typedef std::unordered_map<router_id, int>& seqNum_ref;
typedef std::unordered_map<router_id, Neighbor>& adjacencyList_ref; // Adjust type as needed
typedef std::unordered_map<port_num, PortStatusEntry>& portStatus_ref; // Adjust type as needed
typedef std::unordered_map<router_id, router_id>& forwardingTable_ref;    // Adjust type as needed

struct ls_path_info {
    router_id myID;
    time_stamp lastUpdate;
    cost timeCost;
    ls_path_info(router_id id, time_stamp update, cost cost) : myID(id), lastUpdate(update), timeCost(cost) {}
};

#endif