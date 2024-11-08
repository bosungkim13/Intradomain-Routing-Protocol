#include "DistanceVector.h" 
#include "sharedUtils.h"
#include "dvUtils.h"

DistanceVector::DistanceVector(Node* n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, forwardingTable_ref forwardingTable, port_num numPorts):
    sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {}

Packet DistanceVector::createDVPacket(unsigned short size, unsigned short destID) {
    Packet packet;
    packet.header.packetType = DV;
    packet.header.size = size;
    packet.header.sourceID = this->myRouterID;

    packet.header.destID = destID;

    for (auto nbr: this->adjacencyList) {
        auto neighborID = nbr.first;
        auto cost = nbr.second.timeCost;

        memcpy(packet.payload, &neighborID, sizeof(neighborID));
        memcpy(packet.payload + sizeof(neighborID), &cost, sizeof(cost));
    }
    return packet;

}

void DistanceVector::handleDVPacket(port_num port, Packet pongPacket) {
    // check staleness of packet

    // unpack the payload into a DVTable struct

    // bellman-ford algorithm
    // iterate thru the table from received packet and update adj list ref

    // TODO: do this after implementing basic methods
};

void updateDVFreshness(); 