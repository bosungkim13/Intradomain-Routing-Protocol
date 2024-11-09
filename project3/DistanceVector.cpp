#include "DistanceVector.h" 
#include "sharedUtils.h"
#include "dvUtils.h"

DistanceVector::DistanceVector(Node* n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, DVForwardingTable forwardingTable, port_num numPorts):
    sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {}

Packet DistanceVector::createDVPacket(unsigned short size, unsigned short destID) {
    Packet packet;
    packet.header.packetType = DV;
    packet.header.size = size;
    packet.header.sourceID = this->myRouterID;

    packet.header.destID = destID;

    for (auto nbr: this->adjacencyList) {
        auto neighborID = nbr.first;
        auto tCost = nbr.second.timeCost;

        memcpy(packet.payload, &neighborID, sizeof(neighborID));
        memcpy(packet.payload + sizeof(neighborID), &tCost, sizeof(tCost));
    }
    return packet;

}

// sends an udpate to all neighbors
void DistanceVector::sendUpdates() {
    Packet newDV = createDVPacket(sizeof(DVPacketPayload), 0);
    // newDV.payload
    // TODO: serialize/deserialize of DV packets needs to be properly implemented.
    void * serializedDVPacket = serializeDVPacket(newDV);
    for (auto port: portStatus) {
        if (port.second.isUp) {
            sys->send(port.first, newDV);
        }
    }
}

// THIS IS JUST THE "RECEIVING AN UPDATE" CASE.
// THERE NEEDS TO BE SEPARATE LOGIC FOR WHEN THE ACTUAL TIME COST CHANGES, WHICH WILL BE HANDLED BY THE ROUTINE PING PONGS
void DistanceVector::handleDVPacket(port_num port, Packet pongPacket) {
    // TODO: PingPong phase needs to provide context for the initial DVs for each node.
    // The PingPong phase should also populate the initial forwarding tables for each node.

    // check staleness of packet (idk how to do this yet)

    // unpack the payload into a DVTable struct
    // DVPacketPayload dvPayload = deserializeDVPayload(pongPacket.payload);
    int neighborID = pongPacket.header.sourceID;
    DVForwardingTable dvPayload; // LET'S JUST ASSUME THIS IS WHAT DESERIALIZE DV PAYLOAD RETURNS, I'M PRETTY SURE THIS IS WHAT THE DISTANCE VECTORS SHOULD BE

    // bellman-ford algorithm
    // iterate thru the table from received packet and update adj list ref

    bool updateRequired = false;
    for (auto row: dvPayload.table) {
        auto dest  = row.first;
        auto nbrToDestRoute = row.second;
        if (adjacencyList[neighborID].timeCost + nbrToDestRoute.routeCost < forwardingTable.getRoute(dest).routeCost) {
            forwardingTable.updateRoute(dest, neighborID, adjacencyList[neighborID].timeCost + nbrToDestRoute.routeCost);
        }
    }

    // if the table was updated, send out a new DV packet to all neighbors
    if (updateRequired) sendUpdates();
};

// function that handles changes in neighbor to neighbor cost, determined from
// ping pong process
void DistanceVector::handleCostChange(port_num port, cost changeCost) {

    // get the neighbor ID from the port
    router_id neighborID = portStatus[port].destRouterID;
    bool updateRequired = false;
    // update the routing table with the new cost
    for (auto row: forwardingTable.table) {
        auto dest = row.first;
        auto route = row.second;
        if (route.nextHop == neighborID) {
            // special case where the route is just to the neighbor itself, set to changeCost
            if (dest == neighborID) {
                forwardingTable.updateRoute(dest, neighborID, changeCost);
            } else {
                // add to current cost 
                forwardingTable.updateRoute(dest, neighborID, changeCost + route.routeCost);
                if (changeCost + route.routeCost < adjacencyList[neighborID].timeCost) {
                    adjacencyList[neighborID].timeCost = changeCost + route.routeCost;
                    updateRequired = true;
                }
            }
        }
    }
    if (updateRequired) sendUpdates();
}

void updateDVFreshness(); 