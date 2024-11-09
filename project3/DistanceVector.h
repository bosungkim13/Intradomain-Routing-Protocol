#include "sharedUtils.h"
#include "dvUtils.h"
#include "Node.h"
#include "global.h"

class DistanceVector{
    public:
        DistanceVector(Node* n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, DVForwardingTable forwardingTable, port_num numPorts);
        ~DistanceVector(); // TODO: figure out destructor logic later

        Packet createDVPacket(unsigned short size, unsigned short destID);

        // Handle DV update packet received from a neighbor 
        // This should involve updating DVForwardingTable struct (and I think it should send updated DV to neighbors?)
        // (this method will be called each time neighbor a neighbor periodically sends a DV packet every 30 seconds)
        void handleDVPacket(port_num port, Packet pongPacket);

        // Iterate through DV entries and remove those that have not been updated in the last 45 seconds
        // (this method will be called every 1 second)
        void updateFreshness();

        void handleCostChange(port_num port, cost changeCost);

        void sendUpdates();

    private:
        router_id myRouterID;
        adjacencyList_ref adjacencyList;
        portStatus_ref portStatus;
        DVForwardingTable forwardingTable;
        Node* sys;
        port_num numPorts;
        unsigned int seqNum = 0;
};