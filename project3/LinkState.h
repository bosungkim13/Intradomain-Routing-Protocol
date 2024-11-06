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

        LinkState(Node* n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, forwardingTable_ref forwardingTable, port_num numPorts);
        ~LinkState();
    private:
        router_id myRouterID;
        adjacencyList_ref adjacencyList;
        portStatus_ref portStatus;
        forwardingTable_ref forwardingTable;
        Node* sys;
        port_num numPorts;
        unsigned int seqNum = 0;
};

#endif