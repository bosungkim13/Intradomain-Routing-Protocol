#ifndef LINKSTATE_H
#define LINKSTATE_H

#include <global.h>
#include <sharedUtils.h>
#include <lsUtils.h>
#include <Node.h>

class LinkState {
    public:
        void UpdateTable();
        // Function that sends this router's updates to all neighbors
        void SendUpdates();
        // Function that forwards updates to all neighbors. This function is called when an update is received from a neighbor.
        void FloodUpdates(port_num myPort, void* floodMe, unsigned short size);
        // Function that creates a LS packet
        Packet CreatePacket(unsigned short size);
        // Function to handle LS packets
        void HandlePacket(port_num portId, void* packet, unsigned short size);
        // Function to find the next hop for a given destination
        router_id FindNextHop(unordered_map<router_id, std::pair<router_id, cost>> activeDistances, router_id destId);

        unordered_map<router_id, cost> LinkState::DeserializeLSPacket(void* deserializeMe, router_id &sourceId, seq_num &seqNum);

        bool NeedCostUpdated(router_id nbrId, unordered_map<router_id, cost> neighborCostTable);

        LinkState(Node* n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, forwardingTable_ref forwardingTable, port_num numPorts);
        ~LinkState();
    private:
        router_id myRouterID;
        adjacencyList_ref adjacencyList;
        portStatus_ref portStatus;
        forwardingTable_ref forwardingTable;

        // map from router to previous router and cost
        unordered_map<router_id, ls_path_info> nodeTable;
        // map from router to next router and cost. to be updated by the pong handling
        unordered_map<router_id, unordered_map<router_id, cost>> costTable;
        // map from router to sequence number
        unordered_map<router_id, seq_num> seqTable;


        Node* sys;
        port_num numPorts;
        unsigned int seqNum = 0;
};

#endif