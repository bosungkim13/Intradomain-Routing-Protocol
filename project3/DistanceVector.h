#include "sharedUtils.h"
#include "dvUtils.h"
#include "Node.h"
#include "global.h"

class DistanceVector
{
public:
    DVForwardingTable forwardingTable;

    DistanceVector(); // Default constructor is unused, but necessary for compilation
    DistanceVector(Node* n, router_id id, adjacencyList_ptr adjList, portStatus_ptr portStatus, port_num numPorts);

    // DistanceVector(Node *n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, DVForwardingTable forwardingTable, port_num numPorts);
    // ~DistanceVector(); // TODO: figure out destructor logic later

    Packet createDVPacket(unsigned short destID);

    // Handle DV update packet received from a neighbor
    // This should involve updating DVForwardingTable struct (and I think it should send updated DV to neighbors?)
    // (this method will be called each time neighbor a neighbor periodically sends a DV packet every 30 seconds)
    void handleDVPacket(port_num port, Packet dvPacket);

    // Iterate through DV entries and remove those that have not been updated in the last 45 seconds
    // (this method will be called every 1 second)
    bool dvEntryExpiredCheck();

    bool portExpiredCheck();

    void handleCostChange(port_num port, int changeCost);

    void sendUpdates();

private:
    router_id myRouterID;
    adjacencyList_ptr adjacencyList;
    portStatus_ptr portStatus;
    Node *sys;
    port_num numPorts;
};