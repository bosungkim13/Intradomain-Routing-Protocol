#include "LinkState.h"
#include <cassert>

LinkState::LinkState(Node* n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, forwardingTable_ref forwardingTable, port_num numPorts) 
    : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {
}

Packet LinkState::CreatePacket(unsigned short size) {
    // TODO: do i need to malloc here? 
    Packet packet;
    packet.header.packetType = LS;
    packet.header.size = size;
    packet.header.sourceID = this->myRouterID;
    // Destination ID is not used in LS packet because it will flood to all neighbors
    packet.header.destID = 0;
    
    memcpy(packet.payload, &this->seqNum, sizeof(this->seqNum));
    int offset = sizeof(this->seqNum); // Update index to start after seqNum
    for (auto nbr: this->adjacencyList) {
        auto neighborID = nbr.first;
        auto cost = nbr.second.timeCost;

        // Add neighbor ID and cost to the payload
        memcpy(packet.payload + offset, &neighborID, sizeof(neighborID));
        offset += sizeof(neighborID);
        memcpy(packet.payload + offset, &cost, sizeof(cost));
        offset += sizeof(cost);
    }
    assert(offset <= MAX_PAYLOAD_SIZE);
    return packet;
}

void LinkState::SendUpdates() {
    // Send updates to all neighbors
    // Each neighbor will have its port number and cost (2 + 2 = 4 bytes)
    // Header size is 12 bytes and seqNum is 4 bytes which is start of payload
    unsigned int size = adjacencyList.size() * 4 + HEADER_SIZE + sizeof(seqNum);
    assert(12 == HEADER_SIZE);

    for (port_num portId = 0; portId < this->numPorts; portId++) {
        auto pit = portStatus.find(portId);

        // continue if port is detached
        if (pit == portStatus.end() || !(pit->second.isUp)) {
            continue;
        }

        Packet packet = this->CreatePacket(size);
        void* msg = serializePacket(packet);
        sys->send(portId, msg, size);
    }
}

void LinkState::FloodUpdates(port_num myPort, void* floodMe, unsigned short size) {
    for (port_num portId = 0; portId < this->numPorts; portId++) {
        if (portId == myPort) {
            continue;
        }
        char *copy = strdup(static_cast<char *>(floodMe));
        sys->send(portId, copy, size);
    }
    free(floodMe);
}

